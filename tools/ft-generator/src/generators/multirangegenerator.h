/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Multi range generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../randomgenerator.h"

#include <stdexcept>
#include <vector>

namespace generator {

/**
 * @brief Generate addresses from multiple address generators
 *
 * The generator to generate the address from is choosen randomly
 *
 * @tparam GeneratorT  Type of the address generator
 * @tparam AddressT    Type of the generated address by the generator
 */
template <typename GeneratorT, typename AddressT>
class MultiRangeGenerator {
public:
	/**
	 * @brief Construct a new Multi Range Generator object
	 *
	 * @param generators  The generators
	 */
	MultiRangeGenerator(std::vector<GeneratorT> generators)
		: _generators(std::move(generators))
	{
		if (_generators.empty()) {
			throw std::invalid_argument("no generators provided");
		}
	}

	/**
	 * @brief Generate next address from one of the provided generators, chosen randomly
	 *
	 * @return The generated address
	 */
	AddressT Generate()
	{
		size_t index = RandomGenerator::GetInstance().RandomUInt() % _generators.size();
		return _generators[index].Generate();
	}

private:
	std::vector<GeneratorT> _generators;
};

} // namespace generator
