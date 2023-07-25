/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size value generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>
#include <vector>

namespace generator {

/**
 * @brief Definition of a probability interval
 */
struct IntervalInfo {
	uint64_t _from; //< Left bound of the interval
	uint64_t _to; //< Right bound of the interval
	double _probability; //< Probability of choosing this interval
};

/**
 * @brief Value generator class
 *
 * The ideal use-case is as follows:
 *
 *  1. Instantiate the object
 *  2. Perform as many calls to `GetValueExact` as possible, to fill out the values that are already
 *     known
 *  3. Call `PlanRemaining`, which plans the remaining packets and sets up information for
 *     subsequent calls to `GetValue()`
 *  4. Perform calls to `GetValue` to fill out the sizes of the remaining packets
 */
class PacketSizeGenerator {
public:
	/**
	 * @brief Construct a new Packet Size Generator object
	 *
	 * @param intervals   The intervals to generate the values from. The interval ranges must be
	 * continous!
	 * @param numPkts     The target number of packets
	 * @param numBytes    The target number of bytes
	 *
	 * This is not exact, the values provided are mere targets that we will try to approach. We will
	 * likely not reach exactly the specified values, but we will try to do the best we can.
	 */
	static std::unique_ptr<PacketSizeGenerator>
	Construct(const std::vector<IntervalInfo>& intervals, uint64_t numPkts, uint64_t numBytes);

	/**
	 * @brief The destructor
	 */
	virtual ~PacketSizeGenerator() = default;

	/**
	 * @brief Take an exact value
	 *
	 * @param value  The required value
	 */
	virtual void GetValueExact(uint64_t value) = 0;

	/**
	 * @brief Plan the remaining number of bytes and packets
	 *
	 * This should be called before GetValue is first called, preferably after all calls to
	 * GetValueExact are finished for optimal results, but it's not a requirement.
	 *
	 * This method can be called multiple times, i.e. after more calls to GetValueExact are
	 * performed and replanning might be beneficial.
	 */
	virtual void PlanRemaining() = 0;

	/**
	 * @brief Get a random value from the generated values
	 *
	 * @return A value
	 */
	virtual uint64_t GetValue() = 0;

	/**
	 * @brief Print debug stats
	 *
	 * This is purely a helper function that prints certain statistics to the debug log
	 */
	virtual void PrintReport() = 0;
};

} // namespace generator
