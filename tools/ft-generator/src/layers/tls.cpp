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
#include "../randomgenerator.h"
#include "tlsconstants.h"

#include <pcapplusplus/PayloadLayer.h>

namespace generator {

namespace {

enum class TlsMap : int {
	StoreIndex = 1,
};

} // namespace

static constexpr int FWD_HANDSHAKE_PKTS = 4;
static constexpr int REV_HANDSHAKE_PKTS = 6;

void Tls::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);

	auto putPacket = [&](Buffer message, Direction dir) {
		auto* pkt = planner.NextPacket();
		Packet::layerParams params;

		params[int(TlsMap::StoreIndex)] = _messageStore.size();
		pkt->_size += message.Length();
		_messageStore.emplace_back(std::move(message));

		pkt->_direction = dir;
		pkt->_isFinished = true;

		pkt->_layers.emplace_back(this, params);
		planner.IncludePkt(pkt);
	};

	if (planner.PktsRemaining(Direction::Forward) >= FWD_HANDSHAKE_PKTS
		&& planner.PktsRemaining(Direction::Reverse) >= REV_HANDSHAKE_PKTS) {
		putPacket(_builder.BuildClientHello(), Direction::Forward);
		putPacket(_builder.BuildServerHello(), Direction::Reverse);
		putPacket(_builder.BuildCertificate(), Direction::Reverse);
		putPacket(_builder.BuildServerKeyExchange(), Direction::Reverse);
		putPacket(_builder.BuildServerHelloDone(), Direction::Reverse);
		putPacket(_builder.BuildClientKeyExchange(), Direction::Forward);
		putPacket(_builder.BuildChangeCipherSpec(), Direction::Forward);
		putPacket(_builder.BuildEncryptedHandshake(), Direction::Forward);
		putPacket(_builder.BuildChangeCipherSpec(), Direction::Reverse);
		putPacket(_builder.BuildEncryptedHandshake(), Direction::Reverse);
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
