/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size probability configuration section
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "../packetsizegenerator.h"

#include <yaml-cpp/yaml.h>

#include <vector>

namespace generator {
namespace config {

/**
 * @brief Represents the packet_size_probabilities configuration section
 */
class PacketSizeProbabilities {
public:
	/**
	 * @brief Construct a new Packet Size Probabilities object with default packet sizes and
	 * probabilities
	 */
	PacketSizeProbabilities();

	/**
	 * @brief Construct a new Packet Size Probabilities object from a YAML node containing the
	 * configuration section
	 *
	 * @param node The YAML configuration node
	 */
	PacketSizeProbabilities(const YAML::Node& node);

	/**
	 * @brief Retrieve the configured sizes and probabilities as a IntervalInfo vector
	 */
	const std::vector<IntervalInfo>& AsIntervals() const { return _intervals; }

	/**
	 * @brief Retrieve the configured sizes and probabilities as a IntervalInfo vector in a
	 * normalized form.
	 *
	 * The normalized form assures that all the probabilities sum up to 1, are
	 * sorted, and that the intervals are continous, in other words that they
	 * directly follow eachother, i.e. if the first interval is an interval
	 * from 100 to 200, the next interval is guaranteed to start from 201 (note
	 * that its probability can be 0 though). This ensures that other
	 * components that work with these intervals can make certain assumptions
	 * to simplify their code.
	 */
	const std::vector<IntervalInfo>& AsNormalizedIntervals() const { return _normalizedIntervals; }

private:
	std::vector<IntervalInfo> _intervals;
	std::vector<IntervalInfo> _normalizedIntervals;
};

} // namespace config
} // namespace generator
