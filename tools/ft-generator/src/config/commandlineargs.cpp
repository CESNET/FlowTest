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
	enum longOptsValues {
		OPT_SKIP_UNKNOWN = 256, // Value that cannot collide with chars
		OPT_NO_DISKSPACE_CHECK,
	};
	const option longOpts[]
		= {{"output", required_argument, nullptr, 'o'},
		   {"profiles", required_argument, nullptr, 'p'},
		   {"config", required_argument, nullptr, 'c'},
		   {"report", required_argument, nullptr, 'r'},
		   {"verbose", no_argument, nullptr, 'v'},
		   {"help", no_argument, nullptr, 'h'},
		   {"skip-unknown", no_argument, nullptr, OPT_SKIP_UNKNOWN},
		   {"no-diskspace-check", no_argument, nullptr, OPT_NO_DISKSPACE_CHECK},
		   {nullptr, 0, nullptr, 0}};
	const char* shortOpts = ":o:p:c:r:vh";

	int currentIdx = 0;
	optind = 0;
	opterr = 0; // Handle printing error messages ourselves

	int c;
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
		case OPT_SKIP_UNKNOWN:
			_skipUnknown = true;
			break;
		case OPT_NO_DISKSPACE_CHECK:
			_noDiskSpaceCheck = true;
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
	std::cerr << "Usage: ./ft-generator   [options] -p <profiles file> -o <pcap file>\n";
	std::cerr
		<< "  -c, --config=str      Optional configuration of generation process (YAML file)\n";
	std::cerr << "  -p, --profiles=str    Input CSV file with flow profiles\n";
	std::cerr << "  -o, --output=str      Output PCAP file with generated packets\n";
	std::cerr << "  -r, --report=str      Output CSV file of actually generated flows\n";
	std::cerr << "  -v, --verbose         Increase verbosity level. Can be used multiple times.\n";
	std::cerr << "  -h, --help            Show this help message\n";
	std::cerr << "  --skip-unknown        Skip unknown/unsupported profile records\n";
	std::cerr << "  --no-diskspace-check  Do not check available disk space before generating\n";
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
