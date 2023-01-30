/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "generator.h"

namespace generator {

Generator::Generator(
	FlowProfileProvider& profilesProvider,
	TrafficMeter& trafficMeter,
	const config::Config& config)
	: _trafficMeter(trafficMeter)
	, _config(config)
	, _addressGenerators(
		  config.GetIPv4().GetIpRange(),
		  config.GetIPv6().GetIpRange(),
		  config.GetMac().GetMacRange())
{
	profilesProvider.Provide(_profiles);

	auto compare = [](const FlowProfile& lhs, const FlowProfile& rhs) -> bool {
		return lhs._startTime < rhs._startTime;
	};
	std::sort(_profiles.begin(), _profiles.end(), compare);

	if (!_profiles.empty()) {
		timeval minTime = _profiles[0]._startTime;
		for (auto& profile : _profiles) {
			profile._startTime -= minTime;
			profile._endTime -= minTime;

			assert(profile._startTime <= profile._endTime);
			assert(profile._startTime >= (timeval {0, 0}));
			assert(profile._endTime >= (timeval {0, 0}));
		}
	}
}

std::optional<GeneratorPacket> Generator::GenerateNextPacket()
{
	std::unique_ptr<Flow> flow = GetNextFlow();
	if (!flow) {
		return std::nullopt;
	}

	auto [packet, extra] = flow->GenerateNextPacket();
	_trafficMeter.RecordPacket(flow->GetId(), extra._time, extra._direction, packet);
	_packet = std::move(packet);

	if (!flow->IsFinished()) {
		_calendar.Push(std::move(flow));
	} else {
		_trafficMeter.CloseFlow(flow->GetId());
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

	timeval nextCalendarTime = _calendar.Top().GetNextPacketTime();
	timeval nextProfileStartTime = _profiles[_nextProfileIdx]._startTime;
	return (nextCalendarTime > nextProfileStartTime) ? MakeNextFlow() : _calendar.Pop();
}

std::unique_ptr<Flow> Generator::MakeNextFlow()
{
	const FlowProfile& profile = _profiles[_nextProfileIdx];
	_nextProfileIdx++;
	std::unique_ptr<Flow> flow
		= std::make_unique<Flow>(_nextFlowId++, profile, _addressGenerators, _config);
	_trafficMeter.OpenFlow(flow->GetId(), profile);
	return flow;
}

} // namespace generator
