/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief TCP protocol planner
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tcp.h"
#include "../packetflowspan.h"
#include "ipv4.h"
#include "ipv6.h"

#include <arpa/inet.h>
#include <pcapplusplus/EthLayer.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/IPv6Layer.h>
#include <pcapplusplus/TcpLayer.h>

namespace generator {

constexpr uint64_t TCP_HDR_LEN = sizeof(pcpp::tcphdr);
constexpr uint64_t CONN_HANDSHAKE_FWD_PKTS = 2;
constexpr uint64_t CONN_HANDSHAKE_REV_PKTS = 1;
constexpr uint64_t TERM_HANDSHAKE_FWD_PKTS = 2;
constexpr uint64_t TERM_HANDSHAKE_REV_PKTS = 2;
constexpr uint64_t CONN_HANDSHAKE_FWD_BYTES = CONN_HANDSHAKE_FWD_PKTS * TCP_HDR_LEN;
constexpr uint64_t CONN_HANDSHAKE_REV_BYTES = CONN_HANDSHAKE_REV_PKTS * TCP_HDR_LEN;
constexpr uint64_t TERM_HANDSHAKE_FWD_BYTES = TERM_HANDSHAKE_FWD_PKTS * TCP_HDR_LEN;
constexpr uint64_t TERM_HANDSHAKE_REV_BYTES = TERM_HANDSHAKE_REV_PKTS * TCP_HDR_LEN;

// Max size of a generated packet
constexpr uint64_t MAX_PACKET_SIZE
	= 1500; // NOTE: Get this from configuration when it is implemented

constexpr uint16_t TCP_WINDOW_SIZE
	= 64512; // NOTE: Make this configurable and actually check that we ACK soon enough

enum class TcpPacketKind : uint64_t {
	Unknown,
	Data,
	Start1, // SYN ->
	Start2, // <- SYN ACK
	Start3, // ACK ->
	End1, // FIN ->
	End2, // <- ACK
	End3, // <- FIN
	End4, // ACK ->
};

enum class TcpMap : int {
	Kind = 1,
};

Tcp::Tcp(uint16_t portSrc, uint16_t portDst)
	: _portSrc(portSrc)
	, _portDst(portDst)
{
	_ackNumber = 0;
	_seqNumber = 0;
}

void Tcp::PlanConnectionHandshake(FlowPlanHelper& planner)
{
	Packet::layerParams params;
	Packet* pkt;

	if (planner.FwdPktsRemaining() == 0) {
		return;
	}
	pkt = planner.NextPacket();
	pkt->_direction = Direction::Forward;
	pkt->_isFinished = true;
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::Start1);
	pkt->_layers.emplace_back(this, params);
	pkt->_size += pcpp::TcpLayer().getHeaderLen();
	params.clear();
	planner._assignedFwdPkts++;

	if (planner.RevPktsRemaining() == 0) {
		return;
	}
	pkt = planner.NextPacket();
	pkt->_direction = Direction::Reverse;
	pkt->_isFinished = true;
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::Start2);
	pkt->_layers.emplace_back(this, params);
	pkt->_size += pcpp::TcpLayer().getHeaderLen();
	params.clear();
	planner._assignedRevPkts++;

	if (planner.FwdPktsRemaining() == 0) {
		return;
	}
	pkt = planner.NextPacket();
	pkt->_direction = Direction::Forward;
	pkt->_isFinished = true;
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::Start3);
	pkt->_layers.emplace_back(this, params);
	pkt->_size += pcpp::TcpLayer().getHeaderLen();
	planner._assignedFwdPkts++;
}

void Tcp::PlanTerminationHandshake(FlowPlanHelper& planner)
{
	Packet::layerParams params;
	Packet* pkt;

	if (planner.FwdPktsRemaining() == 0) {
		return;
	}
	pkt = planner.NextPacket();
	pkt->_direction = Direction::Forward;
	pkt->_isFinished = true;
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::End1);
	pkt->_layers.emplace_back(this, params);
	pkt->_size += pcpp::TcpLayer().getHeaderLen();
	planner._assignedFwdPkts++;
	params.clear();

	if (planner.RevPktsRemaining() == 0) {
		return;
	}
	pkt = planner.NextPacket();
	pkt->_direction = Direction::Reverse;
	pkt->_isFinished = true;
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::End2);
	pkt->_layers.emplace_back(this, params);
	pkt->_size += pcpp::TcpLayer().getHeaderLen();
	planner._assignedRevPkts++;
	params.clear();

	if (planner.RevPktsRemaining() == 0) {
		return;
	}
	pkt = planner.NextPacket();
	pkt->_direction = Direction::Reverse;
	pkt->_isFinished = true;
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::End3);
	pkt->_layers.emplace_back(this, params);
	pkt->_size += pcpp::TcpLayer().getHeaderLen();
	planner._assignedRevPkts++;
	params.clear();

	if (planner.FwdPktsRemaining() == 0) {
		return;
	}
	pkt = planner.NextPacket();
	pkt->_direction = Direction::Forward;
	pkt->_isFinished = true;
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::End4);
	pkt->_layers.emplace_back(this, params);
	pkt->_size += pcpp::TcpLayer().getHeaderLen();
	planner._assignedFwdPkts++;
}

void Tcp::PlanData(FlowPlanHelper& planner)
{
	while (true) {
		bool pktsAvail;
		if (_shouldPlanTermHandshake) {
			pktsAvail = planner.PktsTillEnd() > TERM_HANDSHAKE_FWD_PKTS + TERM_HANDSHAKE_REV_PKTS;
		} else {
			pktsAvail = planner.PktsTillEnd() > 0;
		}

		if (!pktsAvail) {
			break;
		}

		Packet::layerParams params;
		Packet* pkt = planner.NextPacket();

		params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::Data);

		pkt->_size += pcpp::TcpLayer().getHeaderLen();
		pkt->_layers.emplace_back(this, params);
	}
}

void Tcp::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);

	DetermineIfHandshakesShouldBePlanned(planner);

	if (_shouldPlanConnHandshake) {
		PlanConnectionHandshake(planner);
	}

	PlanData(planner);

	if (_shouldPlanTermHandshake) {
		PlanTerminationHandshake(planner);
	}
}

void Tcp::Build(PcppPacket& packet, Packet::layerParams& params, Packet& plan)
{
	pcpp::TcpLayer* tcpLayer;
	if (plan._direction == Direction::Forward) {
		tcpLayer = new pcpp::TcpLayer(_portSrc, _portDst);
	} else {
		tcpLayer = new pcpp::TcpLayer(_portDst, _portSrc);
	}

	pcpp::tcphdr* tcpHdr = tcpLayer->getTcpHeader();

	TcpPacketKind kind = static_cast<TcpPacketKind>(std::get<uint64_t>(params[int(TcpMap::Kind)]));
	if (kind == TcpPacketKind::Start1) {
		tcpHdr->synFlag = 1;
	} else if (kind == TcpPacketKind::Start2) {
		tcpHdr->synFlag = 1;
		tcpHdr->ackFlag = 1;
	} else if (kind == TcpPacketKind::Start3) {
		tcpHdr->ackFlag = 1;
	} else if (kind == TcpPacketKind::End1) {
		tcpHdr->finFlag = 1;
		tcpHdr->ackFlag = 1;
	} else if (kind == TcpPacketKind::End2) {
		tcpHdr->ackFlag = 1;
	} else if (kind == TcpPacketKind::End3) {
		tcpHdr->finFlag = 1;
		tcpHdr->ackFlag = 1;
	} else if (kind == TcpPacketKind::End4) {
		tcpHdr->ackFlag = 1;
	} else if (kind == TcpPacketKind::Data) {
		tcpHdr->ackFlag = 1;
	}

	tcpHdr->windowSize = htons(TCP_WINDOW_SIZE);

	if (plan._size >= tcpLayer->getHeaderLen()) {
		plan._size -= tcpLayer->getHeaderLen();
	} else {
		plan._size = 0;
	}

	if (plan._direction == Direction::Forward) {
		tcpHdr->sequenceNumber = htonl(_seqNumber);
		tcpHdr->ackNumber = htonl(_ackNumber);
		_seqNumber += (tcpHdr->synFlag || tcpHdr->finFlag) ? 1 : plan._size;
	} else {
		tcpHdr->sequenceNumber = htonl(_ackNumber);
		tcpHdr->ackNumber = htonl(_seqNumber);
		_ackNumber += (tcpHdr->synFlag || tcpHdr->finFlag) ? 1 : plan._size;
	}

	packet.addLayer(tcpLayer, true);
}

/**
 * @brief The maximum number of bytes per packet, including the TCP header
 */
uint64_t Tcp::CalcMaxBytesPerPkt()
{
	int64_t maxBytesPerPkt = MAX_PACKET_SIZE;

	maxBytesPerPkt -= sizeof(pcpp::ether_header);

	auto* prevLayer = GetPrevLayer();
	assert(prevLayer != nullptr);
	if (prevLayer && prevLayer->Is<IPv4>()) {
		maxBytesPerPkt -= sizeof(pcpp::iphdr);
	} else if (prevLayer && prevLayer->Is<IPv6>()) {
		maxBytesPerPkt -= sizeof(pcpp::ip6_hdr);
	}

	assert(maxBytesPerPkt > 0);
	return std::max<int64_t>(0, maxBytesPerPkt);
}

void Tcp::DetermineIfHandshakesShouldBePlanned(FlowPlanHelper& planner)
{
	// Do we have enough packets to generate valid handshakes?
	_shouldPlanConnHandshake = true;
	_shouldPlanTermHandshake = true;

	if (planner.FwdPktsRemaining() < CONN_HANDSHAKE_FWD_PKTS + TERM_HANDSHAKE_FWD_PKTS
		|| planner.RevPktsRemaining() < CONN_HANDSHAKE_REV_PKTS + TERM_HANDSHAKE_REV_PKTS
		|| planner.FwdBytesRemaining() < CONN_HANDSHAKE_FWD_BYTES + TERM_HANDSHAKE_FWD_BYTES
		|| planner.RevBytesRemaining() < CONN_HANDSHAKE_REV_BYTES + TERM_HANDSHAKE_REV_BYTES) {
		// Not enough bytes or packets in atleast one of the directions for a complete handshake
		_shouldPlanConnHandshake = false;
		_shouldPlanTermHandshake = false;
		return;
	}

	// Remaining values after handshake
	uint64_t fwdPktsRemaining
		= planner.FwdPktsRemaining() - CONN_HANDSHAKE_FWD_PKTS - TERM_HANDSHAKE_FWD_PKTS;
	uint64_t fwdBytesRemaining
		= planner.FwdBytesRemaining() - CONN_HANDSHAKE_FWD_BYTES - TERM_HANDSHAKE_FWD_BYTES;
	uint64_t revPktsRemaining
		= planner.RevPktsRemaining() - CONN_HANDSHAKE_REV_PKTS - TERM_HANDSHAKE_REV_PKTS;
	uint64_t revBytesRemaining
		= planner.RevBytesRemaining() - CONN_HANDSHAKE_REV_BYTES - TERM_HANDSHAKE_REV_BYTES;

	if ((fwdPktsRemaining == 0 && fwdBytesRemaining > 0)
		|| (revPktsRemaining == 0 && revBytesRemaining > 0)) {
		// Payload cannot be placed when using handshake
		_shouldPlanConnHandshake = false;
		_shouldPlanTermHandshake = false;
		return;
	}

	// Average bytes per packet with handshake
	uint64_t fwdBpp = fwdPktsRemaining == 0 ? 0 : fwdBytesRemaining / fwdPktsRemaining;
	uint64_t revBpp = revPktsRemaining == 0 ? 0 : revBytesRemaining / revPktsRemaining;

	// Average bytes per packet without handshake
	uint64_t fwdBppAlt = planner.FwdPktsRemaining() == 0
		? 0
		: planner.FwdBytesRemaining() / planner.FwdPktsRemaining();
	uint64_t revBppAlt = planner.RevPktsRemaining() == 0
		? 0
		: planner.RevBytesRemaining() / planner.RevPktsRemaining();

	// Is the handshake the difference between being able to generate the desired size or not?
	uint64_t maxBytesPerPkt = CalcMaxBytesPerPkt();
	if ((fwdBpp > maxBytesPerPkt && fwdBppAlt <= maxBytesPerPkt)
		|| (revBpp > maxBytesPerPkt && revBppAlt <= maxBytesPerPkt)) {
		// It is, skip the handshake
		_shouldPlanConnHandshake = false;
		_shouldPlanTermHandshake = false;
	}
}

} // namespace generator
