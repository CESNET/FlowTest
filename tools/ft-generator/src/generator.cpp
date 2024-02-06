/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "generator.h"
#include "utils.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

namespace generator {

// Progress is logged after at most this many seconds
static constexpr int PROGRESS_TIMEOUT_SECS = 10;

/**
 * Minimal value for a port to be considered "ephemeral" or "local" as opposed to a port used by a
 * service
 *
 * This is suggested to be 49152 according to IANA and the RFC 6335, but Linux kernels typically use
 * a lower value of 32768 as the default (See /proc/sys/net/ipv4/ip_local_port_range)
 *
 * See https://en.wikipedia.org/wiki/Ephemeral_port for more info
 */
static constexpr uint16_t EPHEMERAL_PORT_MIN = 32768;

template <typename T>
static std::string ToHumanSize(T value)
{
	return ToMetricUnits(value) + "B";
}

Generator::Generator(
	FlowProfileProvider& profilesProvider,
	TrafficMeter& trafficMeter,
	const config::Config& config,
	const config::CommandLineArgs& args)
	: _trafficMeter(trafficMeter)
	, _config(config)
	, _args(args)
{
	profilesProvider.Provide(_profiles);
	_stats._numAllFlows = _profiles.size();

	PrepareProfiles();

	if (!args.ShouldNotCheckDiskSpace()) {
		CheckEnoughDiskSpace();
	}

	_flowMaker = std::make_unique<FlowMaker>(
		_profiles,
		_config,
		args.GetSeed(),
		!_args.ShouldNotCheckFlowCollisions(),
		args.GetPrepareQueueSize());
}

void Generator::PrepareProfiles()
{
	auto compare = [](const FlowProfile& lhs, const FlowProfile& rhs) -> bool {
		return lhs._startTime < rhs._startTime;
	};
	std::sort(_profiles.begin(), _profiles.end(), compare);

	// Adjust profile timestamps to start from zero
	if (!_profiles.empty()) {
		ft::Timestamp minTime = _profiles[0]._startTime;
		for (auto& profile : _profiles) {
			profile._startTime -= minTime;
			profile._endTime -= minTime;

			assert(profile._startTime <= profile._endTime);
			assert(profile._startTime >= ft::Timestamp(0, 0));
			assert(profile._endTime >= ft::Timestamp(0, 0));
		}
	}

	// Switch around the directions if necessary so that the communication from the client is always
	// the forward direction
	for (auto& profile : _profiles) {
		if (profile._srcPort < EPHEMERAL_PORT_MIN && profile._dstPort >= EPHEMERAL_PORT_MIN) {
			profile = profile.Reversed();
		}
	}
}

void Generator::CheckEnoughDiskSpace()
{
	auto availableDiskSpace = std::filesystem::space(_args.GetOutputFile()).free;

	uint64_t expectedDiskSize = 0;
	for (const auto& profile : _profiles) {
		expectedDiskSize += profile.ExpectedSizeOnDisk();
	}

	if (expectedDiskSize > availableDiskSpace) {
		throw DiskSpaceError(
			"Not enough free disk space in the target location (estimated PCAP size: "
			+ ToHumanSize(expectedDiskSize)
			+ ", free disk space: " + ToHumanSize(availableDiskSpace)
			+ "). Run with --no-diskspace-check to proceed anyway.");
	}

	_logger->info(
		"INFO: Estimated size of generated PCAP: " + ToHumanSize(expectedDiskSize)
		+ " (free disk space: " + ToHumanSize(availableDiskSpace) + ")");
}

std::optional<GeneratorPacket> Generator::GenerateNextPacket()
{
	std::unique_ptr<Flow> flow;

	while (true) {
		std::unique_ptr<Flow> flowTmp = GetNextFlow();

		if (!flowTmp) {
			return std::nullopt;
		}

		if (!_minNextPacketTime) {
			flow = std::move(flowTmp);
			break;
		}

		ft::Timestamp nextPacketTime = flowTmp->GetNextPacketTime();
		if (nextPacketTime >= *_minNextPacketTime) {
			flow = std::move(flowTmp);
			break;
		}

		ft::Timestamp diff = *_minNextPacketTime - nextPacketTime;
		flowTmp->ShiftTimestamp(diff.ToNanoseconds());
		_calendar.Push(std::move(flowTmp));
	}

	PacketExtraInfo extra = flow->GenerateNextPacket(_packet);
	_trafficMeter.RecordPacket(flow->GetId(), extra._time, extra._direction, _packet);
	_minNextPacketTime = extra._time
		+ ft::Timestamp::From<ft::TimeUnit::Nanoseconds>(
							 CalcMinGlobalPacketGapPicos(
								 GetPacketSizeFromIPLayer(_packet),
								 _config.GetTimestamps())
							 / 1000);

	if (!flow->IsFinished()) {
		_calendar.Push(std::move(flow));
	} else {
		OnFlowClosed(*flow.get());
	}

	GeneratorPacket result;
	result._data = reinterpret_cast<const std::byte*>(_packet.getRawPacket()->getRawData());
	result._size = _packet.getRawPacket()->getRawDataLen();
	result._time = extra._time;
	return result;
}

std::unique_ptr<Flow> Generator::GetNextFlow()
{
	if (_calendar.IsEmpty()) {
		if (_flowMaker->HasProfilesRemaining()) {
			auto [flow, profile] = _flowMaker->MakeNextFlow();
			OnFlowOpened(*flow.get(), profile);
			return std::move(flow);
		} else {
			return nullptr;
		}
	}

	if (!_flowMaker->HasProfilesRemaining()) {
		return _calendar.Pop();
	}

	ft::Timestamp nextCalendarTime = _calendar.Top().GetNextPacketTime();
	ft::Timestamp nextProfileStartTime = _flowMaker->GetNextProfileStartTime();
	if (nextCalendarTime > nextProfileStartTime) {
		auto [flow, profile] = _flowMaker->MakeNextFlow();
		OnFlowOpened(*flow.get(), profile);
		return std::move(flow);
	} else {
		return _calendar.Pop();
	}
}

void Generator::OnFlowOpened(const Flow& flow, const FlowProfile& profile)
{
	_trafficMeter.OpenFlow(flow.GetId(), profile);
	_stats._numOpenFlows++;
}

void Generator::OnFlowClosed(const Flow& flow)
{
	_trafficMeter.CloseFlow(flow.GetId());

	_stats._numOpenFlows--;
	_stats._numClosedFlows++;

	auto elapsedSecs = std::time(nullptr) - _stats._prevProgressTime;
	auto percent = int(double(_stats._numClosedFlows) / _stats._numAllFlows * 100.0);

	_logger->debug(
		"Flow Stats: {} total, {} open, {} closed ({}%)",
		_stats._numAllFlows,
		_stats._numOpenFlows,
		_stats._numClosedFlows,
		percent);

	if (elapsedSecs > PROGRESS_TIMEOUT_SECS || percent - _stats._prevProgressPercent >= 10) {
		_logger->info(
			"{:.0f}% complete",
			std::round(_stats._numClosedFlows / double(_stats._numAllFlows) * 100.0));
		_stats._prevProgressTime = std::time(nullptr);
		_stats._prevProgressPercent = percent;
	}
}

} // namespace generator
