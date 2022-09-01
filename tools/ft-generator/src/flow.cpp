/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Implementation of Flow interface
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "flow.h"
#include "layer.h"
#include "packet.h"
#include "packetflowspan.h"
#include "valuegenerator.h"
#include "layers/ethernet.h"
#include "layers/icmpecho.h"
#include "layers/icmprandom.h"
#include "layers/ipv4.h"
#include "layers/ipv6.h"
#include "layers/payload.h"
#include "layers/tcp.h"
#include "layers/udp.h"

#include <pcapplusplus/Packet.h>

#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

namespace generator {

static constexpr int ICMP_HEADER_SIZE = sizeof(pcpp::icmphdr);

const static std::vector<IntervalInfo> PacketSizeProbabilities{
	{64, 79, 0.2824},
	{80, 159, 0.073},
	{160, 319, 0.0115},
	{320, 639, 0.012},
	{640, 1279, 0.0092},
	{1280, 1518, 0.6119}
};

Flow::Flow(uint64_t id, const FlowProfile& profile, AddressGenerators& addressGenerators) :
	_fwdPackets(profile._packets),
	_revPackets(profile._packetsRev),
	_fwdBytes(profile._bytes),
	_revBytes(profile._bytesRev),
	_tsFirst(profile._startTime),
	_tsLast(profile._endTime),
	_id(id)
{
	MacAddress macSrc = addressGenerators.GenerateMac();
	MacAddress macDst = addressGenerators.GenerateMac();
	AddLayer(std::make_unique<Ethernet>(macSrc, macDst));

	switch (profile._l3Proto) {
	case L3Protocol::Unknown:
		throw std::runtime_error("Unknown L3 protocol");

	case L3Protocol::Ipv4: {
		IPv4Address ipSrc = addressGenerators.GenerateIPv4();
		IPv4Address ipDst = addressGenerators.GenerateIPv4();
		AddLayer(std::make_unique<IPv4>(ipSrc, ipDst));
	} break;

	case L3Protocol::Ipv6: {
		IPv6Address ipSrc = addressGenerators.GenerateIPv6();
		IPv6Address ipDst = addressGenerators.GenerateIPv6();
		AddLayer(std::make_unique<IPv6>(ipSrc, ipDst));
	} break;
	}

	switch (profile._l4Proto) {
	case L4Protocol::Unknown:
		throw std::runtime_error("Unknown L4 protocol");

	case L4Protocol::Tcp: {
		uint16_t portSrc = profile._srcPort;
		uint16_t portDst = profile._dstPort;
		AddLayer(std::make_unique<Tcp>(portSrc, portDst));
	} break;

	case L4Protocol::Udp: {
		uint16_t portSrc = profile._srcPort;
		uint16_t portDst = profile._dstPort;
		AddLayer(std::make_unique<Udp>(portSrc, portDst));
	} break;

	case L4Protocol::Icmp: {
		AddLayer(MakeIcmpLayer());
	} break;

	case L4Protocol::Icmpv6:
		throw std::runtime_error("ICMPv6 not implemented");
	}

	AddLayer(std::make_unique<Payload>());

	Plan();
}

std::unique_ptr<Layer> Flow::MakeIcmpLayer()
{
	double fwdRevRatioDiff = 1.0;
	if (_fwdPackets + _revPackets > 0) {
		double min = std::min(_fwdPackets, _revPackets);
		double max = std::max(_fwdPackets, _revPackets);
		fwdRevRatioDiff = 1.0 - (min / max);
	}

	double bytesPerPkt = (_fwdBytes + _revBytes) / (_fwdPackets + _revPackets);

	/*
	 * A simple heuristic to choose the proper ICMP packet generation strategy based
	 * on the flow characteristics
	 *
	 * NOTE: Might need further evaluation if this is a "good enough" way to do this
	 * and/or some tweaking
	*/
	std::unique_ptr<Layer> layer;
	if ((_fwdPackets <= 3 || _revPackets <= 3) && (bytesPerPkt <= 1.10 * ICMP_HEADER_SIZE)) {
		// Low amount of small enough packets
		layer = std::make_unique<IcmpRandom>();
	} else if (fwdRevRatioDiff <= 0.2) {
		// About the same number of packets in both directions
		layer = std::make_unique<IcmpEcho>();
	} else if (bytesPerPkt <= 1.10 * ICMP_HEADER_SIZE) {
		// Small enough packets
		layer = std::make_unique<IcmpRandom>();
	} else {
		// Enough packets and many of them
		layer = std::make_unique<IcmpEcho>();
	}

	return layer;
}

Flow::~Flow()
{
	//NOTE: Explicit destructor is required because the class contains
	//      std::unique_ptr<Layer> where Layer is an incomplete type!
}

void Flow::AddLayer(std::unique_ptr<Layer> layer)
{
	_layerStack.emplace_back(std::move(layer));
}

void Flow::Plan()
{
	_packets.resize(_fwdPackets + _revPackets);

	for (auto& layer : _layerStack) {
		layer->PlanFlow(*this);
	}

	PlanPacketsDirections();
	PlanPacketsTimestamps();
	PlanPacketsSizes();
}

std::pair<pcpp::Packet, PacketExtraInfo> Flow::GenerateNextPacket()
{
	if (_packets.empty()) {
		throw std::runtime_error("no more packets to generate in flow");
	}

	pcpp::Packet packet;
	PacketExtraInfo extra;

	Packet packetPlan = _packets.front();
	extra._direction = packetPlan._direction;
	extra._time = packetPlan._timestamp;

	for (auto& [layer, params] : packetPlan._layers) {
		layer->Build(packet, params, packetPlan);
	}

	packet.computeCalculateFields();

	_packets.erase(_packets.begin());

	return {packet, extra};
}

int64_t Flow::GetNextPacketTime() const
{
	return _packets.front()._timestamp;
}

bool Flow::IsFinished() const
{
	return _packets.empty();
}

void Flow::PlanPacketsDirections()
{
	PacketFlowSpan packetsSpan(this, true);
	auto [fwd, rev] = packetsSpan.getAvailableDirections();

	std::vector<Direction> directions;
	directions.insert(directions.end(), fwd, Direction::Forward);
	directions.insert(directions.end(), rev, Direction::Reverse);

	std::random_shuffle(directions.begin(), directions.end());

	size_t id = 0;
	for (auto& packet : packetsSpan) {
		if (packet._direction == Direction::Unknown) {
			packet._direction = directions[id++];
		}
	}
}

void Flow::PlanPacketsTimestamps()
{
	PacketFlowSpan packetsSpan(this, false);

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint64_t> dis(_tsFirst, _tsLast);

	std::vector<uint64_t> timestamps({_tsFirst, _tsLast});

	size_t timestampsToGen = _packets.size() > 2 ? _packets.size() - 2 : 0;

	for (size_t i = 0; i < timestampsToGen; i++) {
		timestamps.emplace_back(dis(gen));
	}

	std::sort(timestamps.begin(), timestamps.end());

	size_t id = 0;
	for (auto& packet : packetsSpan) {
		packet._timestamp = timestamps[id++];
	}
}

void Flow::PlanPacketsSizes()
{
	ValueGenerator fwdGen(_fwdPackets, _fwdBytes, PacketSizeProbabilities);
	ValueGenerator revGen(_revPackets, _revBytes, PacketSizeProbabilities);

	for (auto& packet : _packets) {
		if (packet._isFinished) {
			auto& generator = packet._direction == Direction::Forward ? fwdGen : revGen;
			generator.GetValueExact(packet._size);
		}
	}

	for (auto& packet : _packets) {
		if (!packet._isFinished) {
			auto& generator = packet._direction == Direction::Forward ? fwdGen : revGen;
			packet._size = std::max(packet._size, generator.GetValue()); //NOTE: Add the option to GetValue to choose minimum size?
		}
	}
}

} // namespace generator
