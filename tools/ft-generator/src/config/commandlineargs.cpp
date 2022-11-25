/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "commandlineargs.h"

#include <getopt.h>

#include <iostream>
#include <stdexcept>

namespace generator {
namespace config {

void CommandLineArgs::Parse(int argc, char** argv)
{
	const option longOpts[] = {
		{"output", optional_argument, nullptr, 'o'},
		{"profiles", optional_argument, nullptr, 'p'},
		{"config", optional_argument, nullptr, 'c'},
		{"verbose", no_argument, nullptr, 'v'},
		{"help", no_argument, nullptr, 'h'},
		{nullptr, 0, nullptr, 0}
	};
	const char* shortOpts = "o:p:c:vh";

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
		case 'v':
			_verbosity++;
			break;
		case 'h':
			_help = true;
			break;
		}
	}

	CheckValidity();
}

void CommandLineArgs::PrintUsage()
{
	std::cerr << "Usage: ./ft-generator [-v] -p <flow profile file> -o <output pcap file>\n";
	std::cerr << "  --profiles, -p  ... The flow profiles file in csv format\n";
	std::cerr << "  --output, -o    ... The output pcap file\n";
	std::cerr << "  --config, -c    ... The yaml config file\n";
	std::cerr << "  --verbose, -v   ... Verbosity level, specify multiple times for more verbose logging\n";
	std::cerr << "  --help, -h      ... Show this help message\n";
}

void CommandLineArgs::CheckValidity()
{
	if (_help) {
		return;
	}

	if (_profilesFile.empty()) {
		throw std::invalid_argument("Missing flow profiles file argument (-p)");
	}

	if (_outputFile.empty()) {
		throw std::invalid_argument("Missing pcap output file argument (-o)");
	}
}

} // namespace config
} // namespace generator
