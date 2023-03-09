/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief The generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "calendar.h"
#include "config/config.h"
#include "flow.h"
#include "flowprofile.h"
#include "generators/addressgenerators.h"
#include "timeval.h"
#include "trafficmeter.h"

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

namespace generator {

/**
 * @brief A generated packet
 */
struct GeneratorPacket {
	Timeval _time; //< The flow time of the packet
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
	 */
	Generator(
		FlowProfileProvider& profilesProvider,
		TrafficMeter& trafficMeter,
		const config::Config& config);

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
	std::vector<FlowProfile> _profiles; //< The flow profiles
	std::size_t _nextProfileIdx = 0; //< The index of the next flow profile to use to create a flow
	Calendar _calendar; //< The calendar of active flows
	uint64_t _nextFlowId = 0; //< ID of the next constructed flow
	PcppPacket _packet; //< The current packet instance
	TrafficMeter& _trafficMeter; //< Traffic statistics
	const config::Config& _config; //< The configuration
	AddressGenerators _addressGenerators; //< The address generators

	std::unique_ptr<Flow> GetNextFlow();
	std::unique_ptr<Flow> MakeNextFlow();
};

} // namespace generator
