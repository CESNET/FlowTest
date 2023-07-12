/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Tool for generating network traffic pcaps from flow descriptions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config/commandlineargs.h"
#include "config/config.h"
#include "flowprofile.h"
#include "generator.h"
#include "generators/addressgenerators.h"
#include "logger.h"
#include "pcapwriter.h"
#include "trafficmeter.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace generator;

int main(int argc, char* argv[])
{
	config::CommandLineArgs args;
	config::Config config;

	try {
		args.Parse(argc, argv);
	} catch (const std::invalid_argument& ex) {
		std::cerr << ex.what() << "\n";
		args.PrintUsage();
		return EXIT_FAILURE;
	}

	if (args.IsHelp()) {
		args.PrintUsage();
		return EXIT_SUCCESS;
	}

	ft::LoggerInit();
	if (args.GetVerbosityLevel() >= 3) {
		spdlog::set_level(spdlog::level::trace);
	} else if (args.GetVerbosityLevel() == 2) {
		spdlog::set_level(spdlog::level::debug);
	} else if (args.GetVerbosityLevel() == 1) {
		spdlog::set_level(spdlog::level::info);
	} else if (args.GetVerbosityLevel() == 0) {
		spdlog::set_level(spdlog::level::err);
	}

	auto logger = ft::LoggerGet("main");

	if (!args.GetConfigFile().empty()) {
		try {
			config = config::Config::LoadFromFile(args.GetConfigFile());
		} catch (const config::ConfigError& error) {
			error.PrintPrettyError(args.GetConfigFile(), std::cerr);
			return EXIT_FAILURE;
		} catch (const std::runtime_error& error) {
			logger->error(error.what());
			return EXIT_FAILURE;
		}
	}

	try {
		TrafficMeter trafficMeter;
		FlowProfileReader profileReader(args.GetProfilesFile(), args.ShouldSkipUnknown());
		PcapWriter pcapWriter(args.GetOutputFile());
		Generator generator(profileReader, trafficMeter, config, args);

		while (auto packet = generator.GenerateNextPacket()) {
			logger->debug("Generating packet");
			pcapWriter.WritePacket(packet->_data, packet->_size, packet->_time);
		}

		logger->info("Finished!");

		if (auto reportFile = args.GetReportFile()) {
			logger->info("Writing report of generated flows to \"{}\"", *reportFile);
			trafficMeter.WriteReportCsv(*reportFile);
		}

	} catch (const std::runtime_error& error) {
		logger->critical(error.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
