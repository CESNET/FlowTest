/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Implementation of network profile metrics.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "metrics.h"
#include <algorithm>
#include <array>

#define REL_DIFF(val, ref) fabs(val - ref) * 100 / ref

Metrics::Metrics(
	const std::vector<Biflow>& data,
	double protoThreshold,
	double portThreshold,
	std::optional<const std::vector<bool>> filter)
	: _filter(filter)
{
	std::array<uint64_t, UINT8_MAX + 1> allProtos {};
	std::array<uint64_t, UINT16_MAX + 1> allPorts {};
	std::vector<uint8_t> protoMap;
	std::vector<uint16_t> portMap;
	uint64_t ipv4Cnt = 0;
	uint64_t ipv6Cnt = 0;
	uint64_t sizeLess128 = 0;
	uint64_t sizeLess512 = 0;
	uint64_t sizeLess1024 = 0;
	uint64_t sizeOther = 0;
	uint64_t biflowsCnt = 0;

	protoMap.reserve(UINT8_MAX + 1);
	portMap.reserve(UINT16_MAX + 1);

	for (size_t i = 0; i < data.size(); i++) {
		if (filter and !(*filter)[i]) {
			continue;
		}
		const auto& biflow = data[i];
		biflowsCnt += 1;

		packetsCnt += biflow.packets + biflow.packets_rev;
		bytesCnt += biflow.bytes + biflow.bytes_rev;
		if (biflow.l3_proto == 4) {
			ipv4Cnt++;
		} else {
			ipv6Cnt++;
		}

		allProtos[biflow.l4_proto] += 1;
		if (allProtos[biflow.l4_proto] == 1) {
			protoMap.push_back(biflow.l4_proto);
		}

		allPorts[biflow.src_port] += 1;
		if (allPorts[biflow.src_port] == 1) {
			portMap.push_back(biflow.src_port);
		}

		allPorts[biflow.dst_port] += 1;
		if (allPorts[biflow.dst_port] == 1) {
			portMap.push_back(biflow.dst_port);
		}

		uint64_t avgPacketSize
			= (biflow.bytes + biflow.bytes_rev) / (biflow.packets + biflow.packets_rev);
		if (avgPacketSize <= 128) {
			sizeLess128 += 1;
		} else if (avgPacketSize <= 512) {
			sizeLess512 += 1;
		} else if (avgPacketSize <= 1024) {
			sizeLess1024 += 1;
		} else {
			sizeOther += 1;
		}
	}

	std::sort(protoMap.begin(), protoMap.end(), [&](uint64_t a, uint64_t b) {
		return allProtos[a] > allProtos[b];
	});
	std::sort(portMap.begin(), portMap.end(), [&](uint64_t a, uint64_t b) {
		return allPorts[a] > allPorts[b];
	});

	pktsBtsRatio = static_cast<double>(packetsCnt) / static_cast<double>(bytesCnt);
	bflsPktsRatio = static_cast<double>(biflowsCnt) / static_cast<double>(packetsCnt);
	bflsBtsRatio = static_cast<double>(biflowsCnt) / static_cast<double>(bytesCnt);

	ipv4 = static_cast<double>(ipv4Cnt) / static_cast<double>(biflowsCnt);
	ipv6 = static_cast<double>(ipv6Cnt) / static_cast<double>(biflowsCnt);

	pktSizes.small = static_cast<double>(sizeLess128) / static_cast<double>(biflowsCnt);
	pktSizes.medium = static_cast<double>(sizeLess512) / static_cast<double>(biflowsCnt);
	pktSizes.large = static_cast<double>(sizeLess1024) / static_cast<double>(biflowsCnt);
	pktSizes.huge = static_cast<double>(sizeOther) / static_cast<double>(biflowsCnt);

	for (uint8_t p : protoMap) {
		auto representation = static_cast<double>(allProtos[p]) / static_cast<double>(biflowsCnt);
		if (representation < protoThreshold) {
			break;
		}
		protos.emplace(p, representation);
	}

	for (uint16_t p : portMap) {
		auto representation
			= static_cast<double>(allPorts[p]) / (2 * static_cast<double>(biflowsCnt));
		if (representation < portThreshold) {
			break;
		}
		ports.emplace(p, representation);
	}
}

/* Window interval is the same as the window size (configuration param) */
constexpr unsigned WINDOW_SIZE = 1u;

void Metrics::GatherWindowStats(const std::vector<Biflow>& data, unsigned histSize)
{
	const auto nOfWindows = histSize / WINDOW_SIZE;
	std::vector<Window> windowsAcc;
	windowsAcc.resize(nOfWindows);

	for (size_t i = 0; i < data.size(); i++) {
		if (_filter && !(*_filter)[i]) {
			continue;
		}
		const auto& biflow = data[i];
		for (unsigned windowIndex = biflow.start_window_idx / WINDOW_SIZE;
			 windowIndex <= biflow.end_window_idx / WINDOW_SIZE + 1;
			 windowIndex++) {
			double pkts = 0;
			for (unsigned j = 0; j < WINDOW_SIZE; j++) {
				const auto secIndex = windowIndex * WINDOW_SIZE + j;
				if (secIndex <= biflow.end_window_idx) {
					pkts += biflow.GetHistogramBin(secIndex);
				}
			}

			if (pkts > 0) {
				windowsAcc[windowIndex].packetsCnt += pkts;
			}
		}
	}

	windows.resize(nOfWindows);
	for (size_t i = 0; i < windows.size(); i++) {
		auto& acc = windowsAcc[i];
		auto& stats = windows[i];

		stats.pktsToTotalRatio = acc.packetsCnt / static_cast<double>(packetsCnt);
	}
}

double Metrics::GetRSD(const std::vector<Biflow>& data, unsigned histSize) const
{
	const auto mean = static_cast<double>(packetsCnt) / histSize;
	double acc = 0;

	std::vector<double> pktsAcc;
	pktsAcc.resize(histSize);

	for (size_t i = 0; i < data.size(); i++) {
		if (_filter && !(*_filter)[i]) {
			continue;
		}
		const auto& biflow = data[i];

		for (unsigned secIndex = 0; secIndex < histSize; secIndex++) {
			const auto pkts = biflow.GetHistogramBin(secIndex);
			pktsAcc[secIndex] += pkts;
		}
	}

	for (const auto pkts : pktsAcc) {
		acc += std::pow(pkts - mean, 2);
	}
	return std::sqrt(acc / histSize) / mean;
}

MetricsDiff Metrics::Diff(const Metrics& ref) const
{
	MetricsDiff diff;
	diff.pktsBtsRatio = REL_DIFF(pktsBtsRatio, ref.pktsBtsRatio);
	diff.bflsPktsRatio = REL_DIFF(bflsPktsRatio, ref.bflsPktsRatio);
	diff.bflsBtsRatio = REL_DIFF(bflsBtsRatio, ref.bflsBtsRatio);
	diff.ipv4 = (ref.ipv4 == 0) ? 0 : REL_DIFF(ipv4, ref.ipv4);
	diff.ipv6 = (ref.ipv6 == 0) ? 0 : REL_DIFF(ipv6, ref.ipv6);

	diff.avgPktSize.small = REL_DIFF(pktSizes.small, ref.pktSizes.small);
	diff.avgPktSize.medium = REL_DIFF(pktSizes.medium, ref.pktSizes.medium);
	diff.avgPktSize.large = REL_DIFF(pktSizes.large, ref.pktSizes.large);
	diff.avgPktSize.huge = REL_DIFF(pktSizes.huge, ref.pktSizes.huge);

	for (const auto& proto : ref.protos) {
		auto val = protos.find(proto.first);
		diff.protos[proto.first]
			= (val != protos.end()) ? REL_DIFF(val->second, proto.second) : 100;
	}

	for (const auto& port : ref.ports) {
		auto val = ports.find(port.first);
		diff.ports[port.first] = (val != ports.end()) ? REL_DIFF(val->second, port.second) : 100;
	}

	diff.ComputeFitness();
	return diff;
}

void MetricsDiff::ComputeFitness()
{
	fitness = 100 - pow(pktsBtsRatio, 2) - pow(bflsPktsRatio, 2) - pow(bflsBtsRatio, 2);
	fitness = fitness - pow(ipv4, 2) - pow(ipv6, 2);
	fitness -= avgPktSize.ComputeFitness();

	for (const auto& proto : protos) {
		fitness -= pow(proto.second, 2);
	}

	for (const auto& port : ports) {
		fitness -= pow(port.second, 2);
	}

	if (fitness < 0) {
		fitness = 0;
	}
}

bool MetricsDiff::IsAcceptable(double deviation) const
{
	deviation *= 100;
	if (pktsBtsRatio > deviation || bflsPktsRatio > deviation || bflsBtsRatio > deviation
		|| ipv4 > deviation || ipv6 > deviation) {
		return false;
	}

	if (not avgPktSize.IsAcceptable(deviation)) {
		return false;
	}

	if (std::any_of(protos.begin(), protos.end(), [=](const auto& x) {
			return x.second > deviation;
		})) {
		return false;
	}

	if (std::any_of(ports.begin(), ports.end(), [=](const auto& x) {
			return x.second > deviation;
		})) {
		return false;
	}

	return true;
}
