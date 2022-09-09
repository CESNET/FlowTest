/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>
#include <string>

#include <getopt.h>

namespace replay {

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
	void Parse(int argc, char **argv);

	/** @brief Get replicator config filename. */
	const std::string& GetReplicatorConfig() const;
	/** @brief Get Output plugin specification. */
	const std::string& GetOutputPluginSpecification() const;
	/** @brief Get input pcap filename. */
	const std::string& GetInputPcapFile() const;

	/** @brief Get the pcap replay time multiplier. */
	float GetReplayTimeMultiplier() const;
	/** @brief Get the vlan ID. */
	uint16_t GetVlanID() const;
	/** @brief Get the number of replicator loops. */
	size_t GetLoopsCount() const;

	/** @brief Whether help should be printer */
	bool IsHelp() const;
	/** @brief Print the usage message */
	void PrintUsage() const;

private:
	void SetDefaultValues();
	void Validate();

	const option *GetLongOptions();
	const char *GetShortOptions();

	std::string _replicatorConfig;
	std::string _outputPlugin;
	std::string _pcapFile;

	float _replayTimeMultiplier;
	uint16_t _vlanID;
	size_t _loopsCount;

	bool _help;
};

} // namespace replay
