/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Flow maker
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "flowmaker.h"

#include <cassert>

namespace generator {

// Number of attempts to try to create a flow with unique addresses before failing
static constexpr int UNIQUE_FLOW_NUM_ATTEMPTS = 10;

static constexpr int DEFAULT_PREPARE_QUEUE_SIZE = 128;

FlowMaker::FlowMaker(
	const std::vector<FlowProfile>& profiles,
	const config::Config& config,
	uint64_t seed,
	bool shouldCheckFlowCollisions,
	std::optional<std::size_t> prepareQueueSize)
	: _profiles(profiles)
	, _config(config)
	, _seed(seed)
	, _shouldCheckFlowCollisions(shouldCheckFlowCollisions)
	, _prepareQueueSize(prepareQueueSize.value_or(DEFAULT_PREPARE_QUEUE_SIZE))
	, _addressGenerators(
		  config.GetIPv4().GetIpRange(),
		  config.GetIPv6().GetIpRange(),
		  config.GetMac().GetMacRange())
{
	assert(_prepareQueueSize >= 1);
	while (_nextProfileIdx < _profiles.size() && _preparationQueue.size() < _prepareQueueSize) {
		StartPreprareNextFlow();
	}
}

std::pair<std::unique_ptr<Flow>, const FlowProfile&> FlowMaker::MakeNextFlow()
{
	auto& [future, profile] = _preparationQueue.front();
	auto flow = future.get();
	_preparationQueue.pop();

	if (_nextProfileIdx < _profiles.size()) {
		StartPreprareNextFlow();
	}

	return {std::move(flow), profile};
}

void FlowMaker::StartPreprareNextFlow()
{
	const FlowProfile& profile = _profiles[_nextProfileIdx];
	uint64_t flowId = _nextProfileIdx;
	_nextProfileIdx++;

	FlowAddresses addresses;

	if (_shouldCheckFlowCollisions) {
		// Check for flow collisions, i.e. that the address 5-tuple of the generated flow is unique
		bool collision = true;

		for (int i = 0; i < UNIQUE_FLOW_NUM_ATTEMPTS; i++) {
			addresses = GenerateAddresses(profile, _addressGenerators);

			const auto& identifier = GetNormalizedFlowIdentifier(profile, addresses);
			const auto [_, inserted] = _seenFlowIdentifiers.insert(identifier);
			if (inserted) {
				collision = false;
				break;
			}

			_logger->debug("Flow collision detected ({})! (attempt {})", identifier.ToString(), i);
		}

		if (collision) {
			throw std::runtime_error(
				"Flow collision detected (" + GetNormalizedFlowIdentifier(profile, addresses).ToString() + ")! "
				"Could not create a flow with unique addresses "
				"(" + std::to_string(UNIQUE_FLOW_NUM_ATTEMPTS) + " attempts). "
				"Run with --no-collision-check to override this behavior and proceed anyway.");
		}

	} else {
		addresses = GenerateAddresses(profile, _addressGenerators);
	}

	auto future = std::async(std::launch::async, [=]() {
		RandomGenerator::InitInstance(_seed ^ flowId);
		std::unique_ptr<Flow> flow = std::make_unique<Flow>(flowId, profile, addresses, _config);
		return flow;
	});
	_preparationQueue.push({std::move(future), profile});
}

} // namespace generator
