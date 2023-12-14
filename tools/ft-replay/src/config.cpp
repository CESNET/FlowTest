/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.hpp"

#include <handlers.h>

#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace replay {

enum longOptsValues {
	OPT_SRC_MAC = 256, // Value that cannot collide with chars
	OPT_DST_MAC
};

Config::Config()
{
	SetDefaultValues();
}

void Config::Parse(int argc, char** argv)
{
	SetDefaultValues();

	const option* longOptions = GetLongOptions();
	const char* shortOptions = GetShortOptions();

	int currentIdx = 1;
	optind = 0;
	opterr = 0; // Handle printing error messages ourselves

	int opt;
	while ((opt = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1) {
		const char* current = argv[currentIdx];

		switch (opt) {
		case 'c':
			_replicatorConfig = optarg;
			break;
		case 'd':
			_hwOffloadsSupport = false;
			break;
		case 'x': {
			float value = std::atof(optarg);
			if (value <= 0.0) {
				throw std::runtime_error("Option -x cannot be zero or negative.");
			}
			_timeMultiplier = 1.0 / value;
			uint64_t timeTokensLimit = RateLimitTimeUnit::NANOSEC_IN_SEC;
			SetRateLimit(RateLimitTimeUnit {timeTokensLimit});
			break;
		}
		case 't':
			SetRateLimit(std::monostate());
			break;
		case 'p':
			SetRateLimit(RateLimitPps {std::stoull(optarg)});
			break;
		case 'M':
			SetRateLimit(RateLimitMbps {std::stoull(optarg)});
			break;
		case 'o':
			_outputPlugin = optarg;
			break;
		case 'v':
			_vlanID = std::stoul(optarg);
			break;
		case 'l':
			_loopsCount = std::stoull(optarg);
			if (!_loopsCount) {
				_loopsCount = std::numeric_limits<size_t>::max();
			}
			break;
		case 'i':
			_pcapFile = optarg;
			break;
		case 'n':
			_noFreeRamCheck = true;
			break;
		case 'h':
			_help = true;
			break;
		case OPT_SRC_MAC:
			_srcMac.emplace(std::string(optarg));
			break;
		case OPT_DST_MAC:
			_dstMac.emplace(std::string(optarg));
			break;
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

	Validate();
}

void Config::SetDefaultValues()
{
	_replicatorConfig.clear();
	_outputPlugin.clear();
	_pcapFile.clear();

	_timeMultiplier = 1.0;
	_hwOffloadsSupport = true;
	_rateLimit = std::nullopt;
	_vlanID = 0;
	_loopsCount = 1;
	_noFreeRamCheck = false;
	_srcMac = std::nullopt;
	_dstMac = std::nullopt;

	_help = false;
}

const option* Config::GetLongOptions()
{
	static struct option longOptions[]
		= {{"input", required_argument, nullptr, 'i'},
		   {"output", required_argument, nullptr, 'o'},
		   {"config", required_argument, nullptr, 'c'},
		   {"disable-hw-offload", no_argument, nullptr, 'd'},
		   {"multiplier", required_argument, nullptr, 'x'},
		   {"pps", required_argument, nullptr, 'p'},
		   {"mbps", required_argument, nullptr, 'M'},
		   {"topspeed", no_argument, nullptr, 't'},
		   {"vlan-id", required_argument, nullptr, 'v'},
		   {"loop", required_argument, nullptr, 'l'},
		   {"help", no_argument, nullptr, 'h'},
		   {"no-freeram-check", no_argument, nullptr, 'n'},
		   {"src-mac", required_argument, nullptr, OPT_SRC_MAC},
		   {"dst-mac", required_argument, nullptr, OPT_DST_MAC},
		   {nullptr, 0, nullptr, 0}};
	return longOptions;
}

const char* Config::GetShortOptions()
{
	return ":i:o:c:dx:p:M:tv:l:hn";
}

void Config::Validate()
{
	if (_help) {
		return;
	}
	if (_outputPlugin.empty()) {
		throw std::invalid_argument("Missing output plugin params (-o)");
	}
	if (_pcapFile.empty()) {
		throw std::invalid_argument("Missing input pcap file argument (-i)");
	}
}

void Config::SetRateLimit(Config::RateLimit limit)
{
	if (!_rateLimit.has_value()) {
		_rateLimit = limit;
		return;
	}

	throw std::invalid_argument("Options -x, -t, -p, and -M are mutually exclusive.");
}

const std::string& Config::GetReplicatorConfig() const
{
	return _replicatorConfig;
}

const std::string& Config::GetOutputPluginSpecification() const
{
	return _outputPlugin;
}

const std::string& Config::GetInputPcapFile() const
{
	return _pcapFile;
}

bool Config::GetHwOffloadsSupport() const
{
	return _hwOffloadsSupport;
}

Config::RateLimit Config::GetRateLimit() const
{
	if (_rateLimit.has_value()) {
		return _rateLimit.value();
	}

	return RateLimitTimeUnit {RateLimitTimeUnit::NANOSEC_IN_SEC};
}

uint16_t Config::GetVlanID() const
{
	return _vlanID;
}

size_t Config::GetLoopsCount() const
{
	return _loopsCount;
}

bool Config::GetFreeRamCheck() const
{
	return !_noFreeRamCheck;
}

float Config::GetTimeMultiplier() const
{
	return _timeMultiplier;
}

std::optional<MacAddress> Config::GetSrcMacAddress() const
{
	return _srcMac;
}

std::optional<MacAddress> Config::GetDstMacAddress() const
{
	return _dstMac;
}

bool Config::IsHelp() const
{
	return _help;
}

void Config::PrintUsage() const
{
	std::cout << "Usage: ./ft-replay [options] -i <pcap file> -o <output plugin params>\n";
	std::cout << "  -i, --input=str           Input PCAP file\n";
	std::cout << "  -o, --output=str          The output plugin specification\n";
	std::cout << "  -c, --config=str          The replicator config file\n";
	std::cout << "  -d, --disable-hw-offload  Disable hardware offloading\n";
	std::cout << "  -x. --multiplier=num      Modify replay speed to a given multiple.\n";
	std::cout << "  -p, --pps=num             Replay packets at a given packets/sec\n";
	std::cout << "  -M, --mbps=num            Replay packets at a given mbps\n";
	std::cout << "  -t, --topspeed            Replay packets as fast as possible\n";
	std::cout << "  -v, --vlan-id=num         The vlan ID number\n";
	std::cout << "  -l, --loop=num            Number of loops over PCAP file. [0 = infinite]\n";
	std::cout << "  -n, --no-freeram-check    Disable verification of free RAM resources\n";
	std::cout << "  -h, --help                Show this help message\n";
	std::cout << "  --src-mac=mac             Rewrite all source MAC addresses\n";
	std::cout << "  --dst-mac=mac             Rewrite all destination MAC addresses\n";
}

} // namespace replay
