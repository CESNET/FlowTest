/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TLS application layer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tls.h"
#include "../buffer.h"
#include "../flowplanhelper.h"
#include "tlsconstants.h"

#include <pcapplusplus/PayloadLayer.h>

#include <cassert>

namespace generator {

namespace {

static constexpr std::size_t PACKET_SPLIT_THRESHOLD = 256;

enum class TlsMap : int {
	StoreIndex = 1,
};

} // namespace

static std::vector<std::pair<Buffer, Direction>>
MakeHandshakePackets(TlsBuilder& builder, std::size_t maxPayloadSize)
{
	assert(maxPayloadSize > 0);

	std::vector<std::pair<Buffer, Direction>> packets;
	auto putPackets = [&](std::initializer_list<Buffer> messages, Direction dir) {
		for (auto& buffer : Buffer::Concat(messages).Split(maxPayloadSize)) {
			packets.emplace_back(std::move(buffer), dir);
		}
	};

	putPackets(
		{
			builder.BuildClientHello(),
		},
		Direction::Forward);

	putPackets(
		{
			builder.BuildServerHello(),
			builder.BuildCertificate(),
			builder.BuildServerKeyExchange(),
			builder.BuildServerHelloDone(),
		},
		Direction::Reverse);

	putPackets(
		{
			builder.BuildClientKeyExchange(),
			builder.BuildChangeCipherSpec(),
			builder.BuildEncryptedHandshake(),
		},
		Direction::Forward);

	putPackets(
		{
			builder.BuildChangeCipherSpec(),
			builder.BuildEncryptedHandshake(),
		},
		Direction::Reverse);

	return packets;
}

static bool ShouldIncludeHandshake(
	const FlowPlanHelper& planner,
	const std::vector<std::pair<Buffer, Direction>>& handshakePackets)
{
	uint64_t handshakeFwdPkts = 0;
	uint64_t handshakeRevPkts = 0;
	uint64_t handshakeFwdBytes = 0;
	uint64_t handshakeRevBytes = 0;

	for (const auto& [packet, dir] : handshakePackets) {
		if (dir == Direction::Forward) {
			handshakeFwdPkts++;
			handshakeFwdBytes += packet.Length();
		} else {
			handshakeRevPkts++;
			handshakeRevBytes += packet.Length();
		}
	}

	return planner.PktsRemaining(Direction::Forward) >= handshakeFwdPkts
		&& planner.BytesRemaining(Direction::Forward) >= handshakeFwdBytes
		&& planner.PktsRemaining(Direction::Reverse) >= handshakeRevPkts
		&& planner.BytesRemaining(Direction::Reverse) >= handshakeRevBytes
		&& planner.PktsRemaining() > handshakeFwdPkts + handshakeRevPkts
		&& planner.BytesRemaining()
		> handshakeFwdBytes + handshakeRevBytes + TLS_MIN_APPLICATION_DATA_PKT_LEN;
}

Tls::Tls(uint64_t maxPayloadSizeHint)
	: _maxPayloadSizeHint(maxPayloadSizeHint)
{
}

void Tls::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);

	auto handshakePackets
		= MakeHandshakePackets(_builder, std::max(PACKET_SPLIT_THRESHOLD, _maxPayloadSizeHint));
	if (ShouldIncludeHandshake(planner, handshakePackets)) {
		for (auto& [packet, dir] : handshakePackets) {
			auto* pkt = planner.NextPacket();
			Packet::layerParams params;

			params[int(TlsMap::StoreIndex)] = _messageStore.size();
			pkt->_size += packet.Length();
			_messageStore.emplace_back(std::move(packet));

			pkt->_direction = dir;
			pkt->_isFinished = true;

			pkt->_layers.emplace_back(this, params);
			planner.IncludePkt(pkt);
		}
	}

	// Remaining packets - application data packets
	while (planner.PktsTillEnd() > 0) {
		auto* pkt = planner.NextPacket();
		Packet::layerParams params;

		pkt->_size += TLS_MIN_APPLICATION_DATA_PKT_LEN;
		pkt->_layers.emplace_back(this, params);
	}
}

void Tls::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	auto storeIndexIt = params.find(int(TlsMap::StoreIndex));

	if (storeIndexIt != params.end()) {
		auto storeIndex = std::get<uint64_t>(storeIndexIt->second);

		assert(storeIndex < _messageStore.size());
		packet.addLayer(_messageStore[storeIndex].ToLayer().release());
		plan._size = 0;
		return;
	}

	auto message = _builder.BuildApplicationData(plan._size);
	assert(message.Length() == plan._size);
	packet.addLayer(message.ToLayer().release());
	plan._size = 0;
}

} // namespace generator
