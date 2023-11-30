/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <cstdint>
#include <ctime>
#include <optional>
#include <string>

namespace generator {
namespace config {

/**
 * @brief CommandLineArgs parser
 *
 */
class CommandLineArgs {
public:
	/**
	 * @brief Parse the config from the command line args
	 *
	 * @param argc  The argc
	 * @param argv  The argv
	 *
	 * @throws std::invalid_argument  When invalid command line arguments are provided
	 */
	void Parse(int argc, char** argv);

	/**
	 * @brief Get the Verbosity Level
	 *
	 * @return int
	 */
	int GetVerbosityLevel() const { return _verbosity; }

	/**
	 * @brief Get the Profiles File
	 *
	 * @return File path
	 */
	const std::string& GetProfilesFile() const { return _profilesFile; }

	/**
	 * @brief Get the Output File
	 *
	 * @return File path
	 */
	const std::string& GetOutputFile() const { return _outputFile; }

	/**
	 * @brief Get the Config File
	 *
	 * @return File path
	 */
	const std::string& GetConfigFile() const { return _configFile; }

	/**
	 * @brief Get the provided file path for the report file if provided
	 *
	 * @return File path
	 */
	const std::optional<std::string>& GetReportFile() const { return _reportFile; }

	/**
	 * @brief Get the random generator seed
	 *
	 * @return The seed
	 */
	uint64_t GetSeed() const { return _seed; }

	/**
	 * @brief Get the size of the prepare queue
	 *
	 * @return The size of the prepare queue
	 */
	std::size_t GetPrepareQueueSize() const { return _prepareQueueSize; }

	/**
	 * @brief Whether help should be printer
	 *
	 * @return true or false
	 */
	bool IsHelp() const { return _help; }

	/**
	 * @brief Whether unsupported profile records should be skipped/ignored.
	 */
	bool ShouldSkipUnknown() const { return _skipUnknown; };

	/**
	 * @brief Whether disk space check should be ignored.
	 */
	bool ShouldNotCheckDiskSpace() const { return _noDiskSpaceCheck; };

	/**
	 * @brief Whether flow collision checks should be performed
	 */
	bool ShouldNotCheckFlowCollisions() const { return _noFlowCollisionCheck; };

	/**
	 * @brief Print the usage message
	 *
	 */
	void PrintUsage();

private:
	std::string _profilesFile;
	std::string _outputFile;
	std::string _configFile;
	std::optional<std::string> _reportFile;
	uint64_t _seed = std::time(nullptr);
	std::size_t _prepareQueueSize = 128;
	int _verbosity = 0;
	bool _help = false;
	bool _skipUnknown = false;
	bool _noDiskSpaceCheck = false;
	bool _noFlowCollisionCheck = false;

	/**
	 * @brief Check validity of the arguments
	 *
	 * @throws std::invalid_argument  When the argument combination or their values are invalid
	 */
	void CheckValidity();
};

} // namespace config
} // namespace generator
