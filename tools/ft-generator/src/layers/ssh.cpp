/**
 * @file
 * @author Jaroslav Renda <jaroslav.renda@cesnet.cz>
 * @brief SSH application layer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ssh.h"
#include "../flowplanhelper.h"
#include "sshbuilder.h"
#include "sshconstants.h"
#include <cstdint>

namespace generator {

namespace {

enum class SshMap : int {
	StoreIndex = 1,
};

} // namespace

// Helper function to build the unencrypted key exchange pacekts. It does not check if there are
// enough packets or bytes at the begining.
// Returns vector with pairs consisting of message buffer and the direction of the packet.
template <typename Callback>
static void MakeSshExchangePackets(SshBuilder& builder, SshSettings& set, Callback callback)
{
	callback(builder.BuildIdString(), Direction::Forward);
	callback(builder.BuildIdString(), Direction::Reverse);
	callback(builder.BuildKexInit(set, Direction::Forward), Direction::Forward);
	callback(builder.BuildKexInit(set, Direction::Reverse), Direction::Reverse);
	callback(builder.BuildClientECDH(set), Direction::Forward);
	callback(builder.BuildServerECDH(set), Direction::Reverse);
	callback(builder.BuildNewKeys(), Direction::Forward);
	callback(builder.BuildNewKeys(), Direction::Reverse);
}

/**
 * @brief Decides if the ssh connection should be established or not
 *
 * @param flow Current flow
 * @return True if enough resources, False if not enough and should not establish the connection
 */
bool ShouldPlanSsh(Flow& flow)
{
	// check that there are at least enough packets in each direction
	if (flow._fwdPackets < SSH_MIN_DIR_PKT_CNT) {
		return false;
	}
	if (flow._revPackets < SSH_MIN_DIR_PKT_CNT) {
		return false;
	}

	// check that there are at least enought bytes for kex exchange in each direction
	if (flow._fwdBytes < SSH_MAX_KEX_BYTES_PER_DIR) {
		return false;
	}
	if (flow._revBytes < SSH_MAX_KEX_BYTES_PER_DIR) {
		return false;
	}

	// Check that there are enough bytes in the flow for minimal payload
	uint64_t encryptionBytes = (flow._fwdBytes + flow._revBytes) - 2 * SSH_MAX_KEX_BYTES_PER_DIR;
	uint64_t encryptionPackets = (flow._fwdPackets + flow._revPackets) - 2 * SSH_MIN_KEX_PKT_CNT;
	if (SSH_MIN_ALGORITHM_PAYLOAD_SIZE > encryptionBytes / encryptionPackets) {
		return false;
	}

	return true;
}

void Ssh::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);

	uint64_t availableBytesFwd = planner.BytesRemaining(Direction::Forward);
	uint64_t availableBytesRev = planner.BytesRemaining(Direction::Reverse);

	// if the flow doesn't fit the requiered sizes, do not establish the connection and generate
	// random valid ssh packets.
	if (!ShouldPlanSsh(flow)) {
		uint32_t totalBytes = flow._fwdBytes + flow._revBytes;
		uint32_t totalPackets = flow._packets.size();
		uint32_t pktSize = totalBytes / totalPackets;
		while (planner.PktsTillEnd() > 0) {
			auto* pkt = planner.NextPacket();
			Packet::layerParams params;
			pkt->_size += pktSize;
			pkt->_layers.emplace_back(this, params);
		}
		return;
	}

	MakeSshExchangePackets(_builder, _settings, [&](const Buffer& buffer, Direction dir) {
		auto* pkt = planner.NextPacket();

		Packet::layerParams params;

		params[int(SshMap::StoreIndex)] = _messageStore.size();

		pkt->_size += buffer.Length();
		_messageStore.emplace_back(buffer);

		pkt->_direction = dir;
		if (dir == Direction::Forward) {
			availableBytesFwd -= pkt->_size;
		} else if (dir == Direction::Reverse) {
			availableBytesRev -= pkt->_size;
		}
		pkt->_isFinished = true;

		pkt->_layers.emplace_back(this, params);
		planner.IncludePkt(pkt);
	});

	// if the amount of bytes exceeds the minimum required payload size for all the packets use only
	// the extra space for generation this should generally help to prevent generating more bytes
	// than wanted and few packets stealing all the bytes
	uint64_t totalBytes = availableBytesRev + availableBytesFwd;
	uint64_t safeReserveBytes;
	if ((totalBytes > (planner.PktsTillEnd() * SSH_MIN_ALGORITHM_PAYLOAD_SIZE))) {
		safeReserveBytes = totalBytes - (planner.PktsTillEnd() * SSH_MIN_ALGORITHM_PAYLOAD_SIZE);
	} else {
		safeReserveBytes = totalBytes;
	}

	// Plan the rest of the packets as encrypted ssh packet
	while (planner.PktsTillEnd() > 0) {
		auto* pkt = planner.NextPacket();
		Packet::layerParams params;

		auto& rnd = RandomGenerator::GetInstance();
		uint32_t pktSize;
		if (safeReserveBytes > SSH_MIN_ALGORITHM_PAYLOAD_SIZE) {
			pktSize = rnd.RandomUInt(SSH_MIN_ALGORITHM_PAYLOAD_SIZE, safeReserveBytes);
			pktSize = std::min(pktSize, WIRESHARK_MAX_BYTES);
			safeReserveBytes -= pktSize;
		} else {
			pktSize = SSH_MIN_ALGORITHM_PAYLOAD_SIZE;
		}

		pkt->_size += pktSize;
		pkt->_layers.emplace_back(this, params);
	}
}

void Ssh::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	auto storeIndexIt = params.find(int(SshMap::StoreIndex));

	if (storeIndexIt != params.end()) {
		packet.addLayer(_messageStore[_kexPacketCntr].ToLayer().release());
		_kexPacketCntr++;
		plan._size = 0;
		return;
	}

	auto message = _builder.BuildEncryptedPackage(plan._size, _settings);
	packet.addLayer(message.ToLayer().release());
	plan._size = 0;
}

} // namespace generator
