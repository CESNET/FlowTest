/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "commandlineargs.h"
#include "common.h"

#include <handlers.h>

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
		OPT_NO_FLOW_COLLISION_CHECK,
		OPT_SEED
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
		   {"no-collision-check", no_argument, nullptr, OPT_NO_FLOW_COLLISION_CHECK},
		   {"seed", required_argument, nullptr, OPT_SEED},
		   {nullptr, 0, nullptr, 0}};
	const char* shortOpts = ":o:p:c:r:vh";

	int currentIdx = 1;
	optind = 0;
	opterr = 0; // Handle printing error messages ourselves

	int opt;
	while ((opt = getopt_long(argc, argv, shortOpts, longOpts, nullptr)) != -1) {
		const char* current = argv[currentIdx];

		switch (opt) {
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
		case OPT_NO_FLOW_COLLISION_CHECK:
			_noFlowCollisionCheck = true;
			break;
		case OPT_SEED: {
			auto value = ParseValue<uint64_t>(optarg);
			if (!value) {
				throw std::invalid_argument("Invalid --seed value");
			}
			_seed = *value;
		} break;
		case '?':
			ft::CliHandleInvalidOption(current);
		case ':':
			ft::CliHandleMissingArgument(current);
		default:
			ft::CliHandleUnimplementedOption(current);
		}

		currentIdx = optind;
	}

	if (optind < argc) {
		ft::CliHandleInvalidOption(argv[optind]);
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
	std::cerr << "  --seed=value          Seed used for random generator\n";
	std::cerr << "  --skip-unknown        Skip unknown/unsupported profile records\n";
	std::cerr << "  --no-diskspace-check  Do not check available disk space before generating\n";
	std::cerr
		<< "  --no-collision-check  Do not check for flow collisions caused by address reuse\n";
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
