/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "replicator-core/macAddress.hpp"

#include <chrono>
#include <cstdint>
#include <getopt.h>
#include <optional>
#include <set>
#include <string>
#include <variant>

namespace replay {

using namespace std::chrono_literals;

/**
 * @brief Command line arguments parser.
 *
 * Parse and validate user-specified command line arguments.
 */
class Config {
public:
	/**
	 * @brief Construct a Config object with the default values.
	 *
	 */
	Config();

	/**
	 * @brief Parse command line arguments.
	 * @param[in] argc Number of arguments
	 * @param[in] argv Array of arguments
	 *
	 * @throws std::invalid_argument  When invalid command line arguments are provided
	 */
	void Parse(int argc, char** argv);

	/**
	 * @brief Represents a rate limit value in packets per second (pps).
	 *
	 * This structure is used to specify a rate limit value in packets per second.
	 * The `value` member variable stores the rate limit value.
	 */
	struct RateLimitPps {
		uint64_t value; /**< The rate limit value in packets per second */
	};

	/**
	 * @brief Represents a rate limit value per unit of time.
	 *
	 * This structure is used to specify a rate limit value per unit of time. The `value`
	 * member variable stores the rate limit value, which represents the maximum allowed
	 * rate of tokens that can be consumed within one second. In the context of nanoseconds,
	 * the `NANOSEC_IN_SEC` constant is defined to be 1,000,000,000, indicating that there
	 * are one billion nanoseconds in a single second (1 ns = 10^-9 s).
	 * Adjusting the `value` based on this constant allows fine-grained control over the token
	 * consumption rate. For instance, setting `value` to 2 * NANOSEC_IN_SEC would effectively
	 * double the rate of token consumption, leading to accelerated processing.
	 */
	struct RateLimitTimeUnit {
		/** The representation of a time unit */
		static constexpr uint64_t NANOSEC_IN_SEC = std::chrono::nanoseconds(1s).count();

		uint64_t value; /** The rate limit value per unit of time (tokens per second). */
	};

	/**
	 * @brief Represents a rate limit value in megabits per second (Mbps).
	 *
	 * This structure is used to specify a rate limit value in megabits per second.
	 * The `value` member variable stores the rate limit value.
	 */
	struct RateLimitMbps {
		uint64_t value; /**< The rate limit value in megabits per second */
		/**
		 * @brief Converts the rate limit value to bytes per second.
		 *
		 * This function converts the rate limit value to bytes per second. It assumes that the rate
		 * limit value is originally specified in megabits per second (Mbps).
		 *
		 * @return The rate limit value converted to bytes per second.
		 */
		uint64_t ConvertToBytesPerSecond() const noexcept
		{
			static constexpr uint64_t BytesPerMegabit = 1000 * 1000 / 8;
			return value * BytesPerMegabit;
		}
	};

	/**
	 * @brief Represents a rate limit value.
	 *
	 * This variant type is used to represent a rate limit value, which can be specified
	 * either in packets per second (RateLimitPps), megabits per second (RateLimitMbps) or in Time
	 * Units per second. The variant type allows for flexibility in choosing the rate limit
	 * representation. The `std::monostate` represents the absence of a rate limit.
	 */
	using RateLimit = std::variant<std::monostate, RateLimitPps, RateLimitTimeUnit, RateLimitMbps>;

	/** @brief Get replicator config filename. */
	const std::string& GetReplicatorConfig() const;
	/** @brief Get threadManager cores. */
	const std::set<size_t>& GetThreadManagerCores() const;
	/** @brief Get Output plugin specification. */
	const std::string& GetOutputPluginSpecification() const;
	/** @brief Get input pcap filename. */
	const std::string& GetInputPcapFile() const;
	/** @brief Get Rate Limiter configuration. */
	RateLimit GetRateLimit() const;
	/** @brief Get the vlan ID. */
	uint16_t GetVlanID() const;
	/** @brief Get the number of replicator loops. */
	size_t GetLoopsCount() const;
	/** @brief Get flag whether check free ram */
	bool GetFreeRamCheck() const;
	/** @brief Get whether hardware offloads should be enabled. */
	bool GetHwOffloadsSupport() const;
	/** @brief Get the time multiplier. */
	float GetTimeMultiplier() const;
	/** @brief Get the address that should overwrite all source MAC addresses */
	std::optional<MacAddress> GetSrcMacAddress() const;
	/** @brief Get the address that should overwrite all destination MAC addresses */
	std::optional<MacAddress> GetDstMacAddress() const;

	/** @brief Whether help should be printer */
	bool IsHelp() const;
	/** @brief Print the usage message */
	void PrintUsage() const;

private:
	void SetDefaultValues();
	void SetRateLimit(RateLimit limit);
	void Validate();

	const option* GetLongOptions();
	const char* GetShortOptions();

	std::string _replicatorConfig;
	std::string _outputPlugin;
	std::string _pcapFile;
	bool _hwOffloadsSupport;
	std::set<size_t> _threadManagerCores;
	float _timeMultiplier;

	std::optional<RateLimit> _rateLimit;
	uint16_t _vlanID;
	size_t _loopsCount;
	bool _noFreeRamCheck;
	std::optional<MacAddress> _srcMac;
	std::optional<MacAddress> _dstMac;

	bool _help;
};

} // namespace replay
