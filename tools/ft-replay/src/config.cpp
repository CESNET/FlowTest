/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief Config parsing from command line args
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "config.hpp"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace replay {

Config::Config()
	: _exclusiveOption(std::nullopt)
{
	SetDefaultValues();
}

void Config::CheckExclusiveOption(char option)
{
	if (_exclusiveOption && _exclusiveOption.value() != option) {
		throw std::runtime_error("Options -x, -t, -p, and -M are mutually exclusive.");
	}
	_exclusiveOption = option;
}

void Config::Parse(int argc, char** argv)
{
	SetDefaultValues();

	const option* longOptions = GetLongOptions();
	const char* shortOptions = GetShortOptions();

	char c;
	while ((c = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1) {
		switch (c) {
		case 'c':
			_replicatorConfig = optarg;
			break;
		case 'x':
			CheckExclusiveOption(c);
			_replayTimeMultiplier = std::atof(optarg);
			if (!_replayTimeMultiplier) {
				throw std::runtime_error("Option -x cannot be zero.");
			}
			break;
		case 't':
			CheckExclusiveOption(c);
			_replayTimeMultiplier = 0;
			break;
		case 'p':
			CheckExclusiveOption(c);
			SetRateLimit(RateLimitPps {std::stoull(optarg)});
			break;
		case 'M':
			CheckExclusiveOption(c);
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
		case 'h':
			_help = true;
			break;
		}
	}
	Validate();
}

void Config::SetDefaultValues()
{
	_replicatorConfig.clear();
	_outputPlugin.clear();
	_pcapFile.clear();

	_rateLimit = std::monostate();
	_replayTimeMultiplier = 1;
	_vlanID = 0;
	_loopsCount = 1;
	_help = false;
	_exclusiveOption.reset();
}

const option* Config::GetLongOptions()
{
	static struct option longOptions[]
		= {{"input", required_argument, nullptr, 'i'},
		   {"output", required_argument, nullptr, 'o'},
		   {"config", required_argument, nullptr, 'c'},
		   {"multiplier", required_argument, nullptr, 'x'},
		   {"pps", required_argument, nullptr, 'p'},
		   {"mbps", required_argument, nullptr, 'M'},
		   {"topspeed", no_argument, nullptr, 't'},
		   {"vlan-id", required_argument, nullptr, 'v'},
		   {"loop", required_argument, nullptr, 'l'},
		   {"help", no_argument, nullptr, 'h'},
		   {nullptr, 0, nullptr, 0}};
	return longOptions;
}

const char* Config::GetShortOptions()
{
	return "i:o:c:x:p:M:tv:l:h";
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
	if (std::holds_alternative<std::monostate>(_rateLimit)) {
		_rateLimit = limit;
		return;
	}

	throw std::invalid_argument("Multiple rate limits are not allowed");
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

Config::RateLimit Config::GetRateLimit() const
{
	return _rateLimit;
}

float Config::GetReplayTimeMultiplier() const
{
	return _replayTimeMultiplier;
}

uint16_t Config::GetVlanID() const
{
	return _vlanID;
}

size_t Config::GetLoopsCount() const
{
	return _loopsCount;
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
	std::cout << "  -c, --config=str,         The replicator config file\n";
	std::cout << "  -x. --multiplier=num      Modify replay speed to a given multiple.\n";
	std::cout << "  -p, --pps=num             Replay packets at a given packets/sec\n";
	std::cout << "  -M, --mbps=num            Replay packets at a given mbps\n";
	std::cout << "  -t, --topspeed            Replay packets as fast as possible\n";
	std::cout << "  -v, --vlan-id=num         The vlan ID number\n";
	std::cout << "  -l, --loop=num            Number of loops over PCAP file. [0 = infinite]\n";
	std::cout << "  -h, --help                Show this help message\n";
}

} // namespace replay
