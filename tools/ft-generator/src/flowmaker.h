#pragma once

#include "config/config.h"
#include "flow.h"
#include "flowprofile.h"
#include "generators/addressgenerators.h"
#include "logger.h"
#include "normalizedflowidentifier.h"
#include "timestamp.h"

#include <memory>
#include <set>
#include <utility>

namespace generator {

class FlowMaker {
public:
	FlowMaker(
		const std::vector<FlowProfile>& profiles,
		const config::Config& config,
		bool shouldCheckFlowCollisions);

	bool HasProfilesRemaining() const { return _nextProfileIdx < _profiles.size(); }

	const Timestamp& GetNextProfileStartTime() const
	{
		return _profiles[_nextProfileIdx]._startTime;
	}

	std::pair<std::unique_ptr<Flow>, const FlowProfile&> MakeNextFlow();

private:
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("FlowMaker");

	const std::vector<FlowProfile>& _profiles;
	const config::Config& _config;
	bool _shouldCheckFlowCollisions;
	AddressGenerators _addressGenerators;

	std::size_t _nextProfileIdx = 0;

	std::set<NormalizedFlowIdentifier> _seenFlowIdentifiers;
};

} // namespace generator
