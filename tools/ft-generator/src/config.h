/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <string>

namespace generator {

/**
 * @brief Config parser
 *
 */
class Config {
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
	int GetVerbosityLevel() const
	{
		return _verbosity;
	}

	/**
	 * @brief Get the Profiles File
	 *
	 * @return File path
	 */
	const std::string& GetProfilesFile() const
	{
		return _profilesFile;
	}

	/**
	 * @brief Get the Output File
	 *
	 * @return File path
	 */
	const std::string& GetOutputFile() const
	{
		return _outputFile;
	}

	/**
	 * @brief Whether help should be printer
	 *
	 * @return true or false
	 */
	bool IsHelp() const
	{
		return _help;
	}

	/**
	 * @brief Print the usage message
	 *
	 */
	void PrintUsage();

private:
	std::string _profilesFile;
	std::string _outputFile;
	int _verbosity = 0;
	bool _help = false;

	/**
	 * @brief Check validity of the arguments
	 *
	 * @throws std::invalid_argument  When the argument combination or their values are invalid
	 */
	void CheckValidity();
};

} // namespace generator
