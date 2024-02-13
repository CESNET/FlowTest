/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "calendar.h"
#include "config/commandlineargs.h"
#include "config/config.h"
#include "flow.h"
#include "flowmaker.h"
#include "flowprofile.h"
#include "logger.h"
#include "timestamp.h"
#include "trafficmeter.h"

#include <cstddef>
#include <ctime>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace generator {

class DiskSpaceError : public std::runtime_error {
	using std::runtime_error::runtime_error;
};

/**
 * @brief A generated packet
 */
struct GeneratorPacket {
	Timestamp _time; //< The flow time of the packet
	uint64_t _size; //< The number of bytes of the packet
	const std::byte* _data; //< The bytes of the packet
};

/**
 * @brief Class representing the packet generator
 */
class Generator {
public:
	/**
	 * @brief Construct a new Generator object
	 *
	 * @param profilesProvider  The flow profiles provider
	 * @param trafficMeter      The traffic meter
	 * @param config            The configuration
	 * @param args              The command line args
	 */
	Generator(
		FlowProfileProvider& profilesProvider,
		TrafficMeter& trafficMeter,
		const config::Config& config,
		const config::CommandLineArgs& args);

	/**
	 * @brief Generate the next packet
	 *
	 * @return The packet or std::nullptr if there are no more packets to generate
	 *
	 * @warning The packet data is only valid while the generator instance exists and until this
	 *          method is called again!
	 */
	std::optional<GeneratorPacket> GenerateNextPacket();

private:
	std::shared_ptr<spdlog::logger> _logger = ft::LoggerGet("Generator"); //< The logger
	std::vector<FlowProfile> _profiles; //< The flow profiles
	Calendar _calendar; //< The calendar of active flows
	PcppPacket _packet; //< The current packet instance
	TrafficMeter& _trafficMeter; //< Traffic statistics
	const config::Config& _config; //< The configuration
	const config::CommandLineArgs& _args; //< The command line args

	struct {
		std::size_t _numOpenFlows = 0; //< Number of open flows
		std::size_t _numClosedFlows
			= 0; //< Number of closed flows, i.e. flows fully generated so far
		std::size_t _numAllFlows = 0; //< Total number of flows to generate
		std::time_t _prevProgressTime = 0; //< Time when progress was last logged
		int _prevProgressPercent = 0; //< Last progress percent printed
	} _stats;

	std::unique_ptr<FlowMaker> _flowMaker;

	void PrepareProfiles();
	void CheckEnoughDiskSpace();
	std::unique_ptr<Flow> GetNextFlow();
	void OnFlowOpened(const Flow& flow, const FlowProfile& profile);
	void OnFlowClosed(const Flow& flow);
};

} // namespace generator
