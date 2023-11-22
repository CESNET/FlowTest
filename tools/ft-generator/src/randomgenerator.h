/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Random number generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "randomgeneratorengine.h"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace generator {

inline constexpr std::string_view LOWERCASE_LETTERS = "abcdefghijklmnopqrstuvwxyz";
inline constexpr std::string_view UPPERCASE_LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
inline constexpr std::string_view NUMBERS = "0123456789";
inline constexpr std::string_view ALPHANUMERIC_CHARS
	= "abcdefghijklmnopqrstuvwxzyABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/**
 * @brief A random number generator
 *
 * Used instead of std::random for better performance
 *
 * Uses the Xoshiro256++ as its engine and provides various value generation functions on top
 */
class RandomGenerator {
public:
	/**
	 * Initialize the Random Generator instance
	 *
	 * @param seed The random generation seed
	 */
	static void InitInstance(std::optional<uint64_t> seed = std::nullopt);

	/**
	 * @brief Get the Random Generator instance
	 *
	 * @return RandomGenerator&
	 */
	static RandomGenerator& GetInstance();

	/**
	 * @brief Generate a random 64 bit unsigned integer
	 *
	 * @return uint64_t  The generated random value
	 */
	uint64_t RandomUInt();

	/**
	 * @brief Generate a random 64 bit unsigned integer in specified range
	 *
	 * @param min  The minimum possible value
	 * @param max  The maximum possible value (including!)
	 * @return uint64_t  The generated random value
	 */
	uint64_t RandomUInt(uint64_t min, uint64_t max);

	/**
	 * @brief Generate a random float in range <0.0, 1.0)
	 *
	 * @return float  The generated random value
	 */
	float RandomFloat();

	/**
	 * @brief Generate a random double in range <0.0, 1.0)
	 *
	 * @return double  The generated random value
	 */
	double RandomDouble();

	/**
	 * @brief Generate a random double in specified range
	 *
	 * @param min  The minimum possible value
	 * @param max  The maximum possible value
	 * @return double  The generated random value
	 */
	double RandomDouble(double min, double max);

	/**
	 * @brief Choose a random value from a container of values
	 *
	 * @param values  The values (non-empty)
	 * @return The randomly chosen value
	 *
	 * @throws std::logic_error If the provided values vector is empty
	 */
	template <typename T>
	const auto& RandomChoice(const T& values);

	/**
	 * @brief Generate a random string
	 *
	 * @param length  The length of the string
	 * @param charset  The character set to choose characters from
	 * @return A random string
	 */
	std::string RandomString(std::size_t length, std::string_view charset = ALPHANUMERIC_CHARS);

	/**
	 * @brief Generate a random sequence of bytes
	 *
	 * @param length  Number of bytes to generate
	 * @return Random bytes
	 */
	std::vector<uint8_t> RandomBytes(std::size_t length);

	/**
	 * @brief Randomly shuffle a container
	 *
	 * @param values  The values to shuffle
	 */
	template <typename T>
	void Shuffle(T& values);

	/**
	 * @brief Randomly distribute N values such that they sum up to a certain amount
	 *
	 * @param amount  The total amount to sum to
	 * @param numValues  Number of values to generate
	 * @param min  The minimum of a single value
	 * @param max  The maximum of a single value
	 * @return The randomly distributed values
	 */
	std::vector<uint64_t>
	RandomlyDistribute(uint64_t amount, std::size_t numValues, uint64_t min, uint64_t max);

private:
	Xoshiro256PlusPlusGenerator _engine;

	/**
	 * @brief Construct a new Random Generator object
	 *
	 * @param seed  The seed for the underlying generator
	 */
	RandomGenerator(uint64_t seed);

	/**
	 * Disable copy and move constructors
	 */
	RandomGenerator(const RandomGenerator&) = delete;
	RandomGenerator(RandomGenerator&&) = delete;
	RandomGenerator& operator=(const RandomGenerator&) = delete;
	RandomGenerator& operator=(RandomGenerator&&) = delete;
};

template <typename T>
const auto& RandomGenerator::RandomChoice(const T& values)
{
	if (values.empty()) {
		throw std::logic_error("cannot randomly choose from empty values");
	}
	return values[RandomUInt(0, values.size() - 1)];
}

template <typename T>
void RandomGenerator::Shuffle(T& values)
{
	if (values.size() <= 1) {
		return;
	}

	// https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
	for (std::size_t i = values.size() - 1; i >= 1; i--) {
		std::size_t j = RandomUInt(0, i);
		std::swap(values[i], values[j]);
	}
}

} // namespace generator
