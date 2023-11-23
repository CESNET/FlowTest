#include "flowmaker.h"

namespace generator {

// Number of attempts to try to create a flow with unique addresses before failing
static constexpr int UNIQUE_FLOW_NUM_ATTEMPTS = 10;

FlowMaker::FlowMaker(
	const std::vector<FlowProfile>& profiles,
	const config::Config& config,
	bool shouldCheckFlowCollisions)
	: _profiles(profiles)
	, _config(config)
	, _shouldCheckFlowCollisions(shouldCheckFlowCollisions)
	, _addressGenerators(
		  config.GetIPv4().GetIpRange(),
		  config.GetIPv6().GetIpRange(),
		  config.GetMac().GetMacRange())
{
}

std::pair<std::unique_ptr<Flow>, const FlowProfile&> FlowMaker::MakeNextFlow()
{
	const FlowProfile& profile = _profiles[_nextProfileIdx];
	uint64_t flowId = _nextProfileIdx;
	_nextProfileIdx++;

	FlowAddresses addresses;

	if (_shouldCheckFlowCollisions) {
		std::unique_ptr<Flow> tmp;

		// Check for flow collisions, i.e. that the address 5-tuple of the generated flow is unique
		for (int i = 0; i < UNIQUE_FLOW_NUM_ATTEMPTS; i++) {
			tmp = std::make_unique<Flow>(flowId, profile, _addressGenerators, _config);
			const auto& ident = tmp->GetNormalizedFlowIdentifier();
			const auto [_, inserted] = _seenFlowIdentifiers.insert(ident);
			if (inserted) {
				flow = std::move(tmp);
				break;
			}

			_logger->debug("Flow collision detected ({})! (attempt {})", ident.ToString(), i);
		}

		if (!flow) {
			throw std::runtime_error(
				"Flow collision detected (" + tmp->GetNormalizedFlowIdentifier().ToString() + ")! "
				"Could not create a flow with unique addresses "
				"(" + std::to_string(UNIQUE_FLOW_NUM_ATTEMPTS) + " attempts). "
				"Run with --no-collision-check to override this behavior and proceed anyway.");
		}

	} else {
		// Ignore check collisions because --no-collision-check was specified
		flow = std::make_unique<Flow>(flowId, profile, _addressGenerators, _config);
	}

	return {std::move(flow), profile};
}

} // namespace generator
