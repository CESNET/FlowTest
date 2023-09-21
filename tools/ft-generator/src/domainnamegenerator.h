/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Domain name generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "randomgenerator.h"

#include <string>
#include <vector>

namespace generator {

/**
 * @brief A singleton component to generate random domain names
 */
class DomainNameGenerator {
public:
	/**
	 * @brief Get the domain name generator instance
	 */
	static DomainNameGenerator& GetInstance();

	/**
	 * @brief Generate a domain name
	 *
	 * @param length  The desired length
	 * @return The randomly generated domain name
	 */
	std::string Generate(std::size_t length);

private:
	const std::size_t _minWordLen;
	const std::size_t _maxWordLen;
	std::vector<std::size_t> _startIndexForWordLen;
	std::vector<std::size_t> _endIndexForWordLen;

	RandomGenerator& _rng = RandomGenerator::GetInstance();

	DomainNameGenerator();
};

} // namespace generator
