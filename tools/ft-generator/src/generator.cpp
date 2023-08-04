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
#include <stdexcept>
#include <string>

namespace generator {

// Progress is logged after at most this many seconds
static constexpr int PROGRESS_TIMEOUT_SECS = 10;

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
	, _addressGenerators(
		  config.GetIPv4().GetIpRange(),
		  config.GetIPv6().GetIpRange(),
		  config.GetMac().GetMacRange())
	, _args(args)
{
	profilesProvider.Provide(_profiles);
	_stats._numAllFlows = _profiles.size();

	PrepareProfiles();

	if (!args.ShouldNotCheckDiskSpace()) {
		CheckEnoughDiskSpace();
	}
}

void Generator::PrepareProfiles()
{
	auto compare = [](const FlowProfile& lhs, const FlowProfile& rhs) -> bool {
		return lhs._startTime < rhs._startTime;
	};
	std::sort(_profiles.begin(), _profiles.end(), compare);

	// Adjust profile timestamps to start from zero
	if (!_profiles.empty()) {
		Timeval minTime = _profiles[0]._startTime;
		for (auto& profile : _profiles) {
			profile._startTime -= minTime;
			profile._endTime -= minTime;

			assert(profile._startTime <= profile._endTime);
			assert(profile._startTime >= Timeval(0, 0));
			assert(profile._endTime >= Timeval(0, 0));
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
		throw std::runtime_error(
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
	std::unique_ptr<Flow> flow = GetNextFlow(); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
	if (!flow) {
		return std::nullopt;
	}

	PacketExtraInfo extra = flow->GenerateNextPacket(_packet);
	_trafficMeter.RecordPacket(flow->GetId(), extra._time, extra._direction, _packet);

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
	bool hasProfilesRemaining = (_nextProfileIdx < _profiles.size());

	if (_calendar.IsEmpty()) {
		return hasProfilesRemaining ? MakeNextFlow() : nullptr;
	}

	if (!hasProfilesRemaining) {
		return _calendar.Pop();
	}

	Timeval nextCalendarTime = _calendar.Top().GetNextPacketTime();
	Timeval nextProfileStartTime = _profiles[_nextProfileIdx]._startTime;
	return (nextCalendarTime > nextProfileStartTime) ? MakeNextFlow() : _calendar.Pop();
}

std::unique_ptr<Flow> Generator::MakeNextFlow()
{
	const FlowProfile& profile = _profiles[_nextProfileIdx];
	_nextProfileIdx++;
	std::unique_ptr<Flow> flow
		= std::make_unique<Flow>(_nextFlowId++, profile, _addressGenerators, _config);
	OnFlowOpened(*flow.get(), profile);
	return flow;
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
