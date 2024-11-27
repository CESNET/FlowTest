/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Flow maker
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "config/config.h"
#include "flow.h"
#include "flowprofile.h"
#include "generators/addressgenerators.h"
#include "logger.h"
#include "normalizedflowidentifier.h"
#include "timestamp.h"

#include <future>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <utility>

namespace generator {

/**
 * @brief A class for creation of flow objects
 */
class FlowMaker {
public:
	/**
	 * @brief Instantiate a FlowMaker object
	 *
	 * @param profiles The vector of flow profiles
	 * @param config The config
	 * @param seed The seed
	 * @param shouldCheckFlowCollisions Whether flow identifier collisions should be checked for
	 * @param prepareQueueSize Size of the flow preparation queue
	 */
	FlowMaker(
		const std::vector<FlowProfile>& profiles,
		const config::Config& config,
		uint64_t seed,
		bool shouldCheckFlowCollisions,
		std::optional<std::size_t> prepareQueueSize);

	/**
	 * @brief Check if there are any remaning flows to be made
	 *
	 * @return true if more flows can be created, false if all flows have already been created
	 */
	bool HasProfilesRemaining() const { return !_preparationQueue.empty(); }

	/**
	 * @brief Retrieve start time of the next flow profile that will be created
	 */
	const ft::Timestamp& GetNextProfileStartTime() const
	{
		return _preparationQueue.front().second._startTime;
	}

	/**
	 * @brief Make the next flow
	 */
	std::pair<std::unique_ptr<Flow>, const FlowProfile&> MakeNextFlow();

private:
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("FlowMaker");

	const std::vector<FlowProfile>& _profiles;
	const config::Config& _config;
	uint64_t _seed;
	bool _shouldCheckFlowCollisions;
	std::size_t _prepareQueueSize;
	AddressGenerators _addressGenerators;

	std::size_t _nextProfileIdx = 0;

	std::set<NormalizedFlowIdentifier> _seenFlowIdentifiers;

	std::queue<std::pair<std::future<std::unique_ptr<Flow>>, const FlowProfile&>> _preparationQueue;

	void StartPreprareNextFlow();
};

} // namespace generator
