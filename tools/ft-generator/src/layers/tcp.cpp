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

#include <arpa/inet.h>
#include <pcapplusplus/TcpLayer.h>

#include <random>

namespace generator {

constexpr uint64_t CONNECTION_HANDSHAKE_FWD_PKTS = 2;
constexpr uint64_t CONNECTION_HANDSHAKE_REV_PKTS = 1;
constexpr uint64_t CONNECTION_HANDSHAKE_PKTS = CONNECTION_HANDSHAKE_FWD_PKTS + CONNECTION_HANDSHAKE_REV_PKTS;

constexpr uint64_t TERM_HANDSHAKE_FWD_PKTS = 2;
constexpr uint64_t TERM_HANDSHAKE_REV_PKTS = 2;
constexpr uint64_t TERM_HANDSHAKE_PKTS = TERM_HANDSHAKE_FWD_PKTS + TERM_HANDSHAKE_REV_PKTS;

constexpr uint16_t TCP_WINDOW_SIZE = 64512; //NOTE: Make this configurable and actually check that we ACK soon enough

enum class TcpPacketKind : uint64_t {
	Unknown,
	Data,
	Ack,
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

class FlowPlanHelper {
public:
	explicit FlowPlanHelper(Flow& flow);

	Packet* NextPacket();
	uint64_t PktsRemaining();
	uint64_t FwdPktsRemaining();
	uint64_t RevPktsRemaining();

	Flow* _flow;
	Packet* _packet = nullptr;
	uint64_t _assignedFwdPkts;
	uint64_t _assignedRevPkts;
	bool _first = true;
	std::list<Packet>::iterator _it;
};

FlowPlanHelper::FlowPlanHelper(Flow& flow)
	: _flow(&flow)
{
	PacketFlowSpan span(_flow, false);
	auto [fwdAvail, revAvail] = span.getAvailableDirections();
	_assignedFwdPkts = _flow->_fwdPackets - fwdAvail;
	_assignedRevPkts = _flow->_revPackets - revAvail;
}

Packet* FlowPlanHelper::NextPacket()
{
	if (_first) {
		_it = _flow->_packets.begin();
		_first = false;
	} else if (_it != _flow->_packets.end()) {
		++_it;
	}

	while (_it != _flow->_packets.end() && _it->_isFinished) {
		++_it;
	}

	if (_it != _flow->_packets.end()) {
		_packet = &(*_it);
	} else {
		_packet = nullptr;
	}

	return _packet;
}

uint64_t FlowPlanHelper::PktsRemaining()
{
	return FwdPktsRemaining() + RevPktsRemaining();
}

uint64_t FlowPlanHelper::FwdPktsRemaining()
{
	return _flow->_fwdPackets - _assignedFwdPkts;
}

uint64_t FlowPlanHelper::RevPktsRemaining()
{
	return _flow->_revPackets - _assignedRevPkts;
}

Tcp::Tcp(uint16_t portSrc, uint16_t portDst) :
	_portSrc(portSrc), _portDst(portDst)
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

	if (planner.FwdPktsRemaining() == 0) {
		return;
	}
	pkt = planner.NextPacket();
	pkt->_direction = Direction::Forward;
	pkt->_isFinished = true;
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::End3);
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
	params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::End4);
	pkt->_layers.emplace_back(this, params);
	pkt->_size += pcpp::TcpLayer().getHeaderLen();
	planner._assignedRevPkts++;
}

void Tcp::PlanData(FlowPlanHelper& planner)
{
	bool fwdNeedsAck = false;
	bool revNeedsAck = false;

	double fwdPktChance = double(planner.FwdPktsRemaining()) / double(planner.PktsRemaining());
	std::random_device rd;
	std::mt19937 eng(rd());
	std::uniform_real_distribution<> dist(0.0, 1.0);

	while (true) {
		bool fwdAvail = planner.FwdPktsRemaining() > TERM_HANDSHAKE_FWD_PKTS;
		bool revAvail = planner.RevPktsRemaining() > TERM_HANDSHAKE_REV_PKTS;
		if (!fwdAvail && !revAvail) {
			break;
		}

		Direction dir;
		if (fwdAvail && revAvail) {
			dir = dist(eng) <= fwdPktChance ? Direction::Forward : Direction::Reverse;
		} else {
			dir = fwdAvail ? Direction::Forward : Direction::Reverse;
		}

		Packet::layerParams params;
		Packet* pkt = planner.NextPacket();

		if (dir == Direction::Forward) {
			if (fwdNeedsAck) {
				params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::Ack);
				pkt->_isFinished = true;
				fwdNeedsAck = false;
			} else {
				params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::Data);
				revNeedsAck = true;
			}
			planner._assignedFwdPkts++;
		} else {
			if (revNeedsAck) {
				params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::Ack);
				pkt->_isFinished = true;
				revNeedsAck = false;
			} else {
				params[int(TcpMap::Kind)] = uint64_t(TcpPacketKind::Data);
				fwdNeedsAck = true;
			}
			planner._assignedRevPkts++;
		}

		pkt->_direction = dir;
		pkt->_size += pcpp::TcpLayer().getHeaderLen();
		pkt->_layers.emplace_back(this, params);
	}
}

void Tcp::PlanFlow(Flow& flow)
{
	FlowPlanHelper planner(flow);

	PlanConnectionHandshake(planner);
	PlanData(planner);
	PlanTerminationHandshake(planner);
}

void Tcp::Build(pcpp::Packet& packet, Packet::layerParams& params, Packet& plan)
{
	pcpp::TcpLayer *tcpLayer;
	if (plan._direction == Direction::Forward) {
		tcpLayer = new pcpp::TcpLayer(_portSrc, _portDst);
	} else {
		tcpLayer = new pcpp::TcpLayer(_portDst, _portSrc);
	}

	pcpp::tcphdr *tcpHdr = tcpLayer->getTcpHeader();

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
	} else if (kind == TcpPacketKind::End2) {
		tcpHdr->ackFlag = 1;
	} else if (kind == TcpPacketKind::End3) {
		tcpHdr->finFlag = 1;
	} else if (kind == TcpPacketKind::End4) {
		tcpHdr->ackFlag = 1;
	} else if (kind == TcpPacketKind::Ack) {
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

} // namespace generator