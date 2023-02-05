/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "commandlineargs.h"

#include <getopt.h>

#include <cassert>
#include <iostream>
#include <stdexcept>

namespace generator {
namespace config {

void CommandLineArgs::Parse(int argc, char** argv)
{
	const option longOpts[]
		= {{"output", required_argument, nullptr, 'o'},
		   {"profiles", required_argument, nullptr, 'p'},
		   {"config", required_argument, nullptr, 'c'},
		   {"report", required_argument, nullptr, 'r'},
		   {"verbose", no_argument, nullptr, 'v'},
		   {"help", no_argument, nullptr, 'h'},
		   {nullptr, 0, nullptr, 0}};
	const char* shortOpts = ":o:p:c:r:vh";

	int currentIdx = 0;
	optind = 0;
	opterr = 0; // Handle printing error messages ourselves

	char c;
	while ((c = getopt_long(argc, argv, shortOpts, longOpts, nullptr)) != -1) {
		switch (c) {
		case 'o':
			_outputFile = optarg;
			break;
		case 'p':
			_profilesFile = optarg;
			break;
		case 'c':
			_configFile = optarg;
			break;
		case 'r':
			_reportFile = optarg;
			break;
		case 'v':
			_verbosity++;
			break;
		case 'h':
			_help = true;
			break;
		case '?':
			throw std::invalid_argument("Unknown option " + std::string(argv[currentIdx]));
		case ':':
			throw std::invalid_argument(
				"Missing argument for option " + std::string(argv[currentIdx]));
		default:
			assert(0 && "Unhandled option");
		}
		currentIdx = optind;
	}

	CheckValidity();
}

void CommandLineArgs::PrintUsage()
{
	std::cerr << "Usage: ./ft-generator [-v] -p <flow profile file> -o <output pcap file> -r "
				 "<output report file>\n";
	std::cerr << "  --profiles, -p  ... The flow profiles file in csv format\n";
	std::cerr << "  --output, -o    ... The output pcap file\n";
	std::cerr << "  --config, -c    ... The yaml config file\n";
	std::cerr
		<< "  --report, -r    ... The output csv file with the report of the generated flows\n";
	std::cerr << "  --verbose, -v   ... Verbosity level, specify multiple times for more verbose "
				 "logging\n";
	std::cerr << "  --help, -h      ... Show this help message\n";
}

void CommandLineArgs::CheckValidity()
{
	if (_help) {
		return;
	}

	if (_profilesFile.empty()) {
		throw std::invalid_argument("Missing required argument --profiles");
	}

	if (_outputFile.empty()) {
		throw std::invalid_argument("Missing required argument --output");
	}
}

} // namespace config
} // namespace generator
