#include "config.h"

#include <getopt.h>

#include <iostream>
#include <stdexcept>

namespace generator {

void Config::Parse(int argc, char** argv)
{
	const option longOpts[] = {
		{"output", optional_argument, nullptr, 'o'},
		{"profiles", optional_argument, nullptr, 'p'},
		{"verbose", no_argument, nullptr, 'v'},
		{"help", no_argument, nullptr, 'h'},
		{nullptr, 0, nullptr, 0}
	};
	const char* shortOpts = "o:p:vh";

	char c;
	while ((c = getopt_long(argc, argv, shortOpts, longOpts, nullptr)) != -1) {
		switch (c) {
		case 'o':
			_outputFile = optarg;
			break;
		case 'p':
			_profilesFile = optarg;
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

void Config::PrintUsage()
{
	std::cerr << "Usage: ./ft-generator [-v] -p <flow profile file> -o <output pcap file>\n";
	std::cerr << "  --profiles, -p  ... The flow profiles file in csv format\n";
	std::cerr << "  --output, -o    ... The output pcap file\n";
	std::cerr << "  --verbose, -v   ... Verbosity level, specify multiple times for more verbose logging\n";
	std::cerr << "  --help, -h      ... Show this help message\n";
}

void Config::CheckValidity()
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

} // namespace generator
