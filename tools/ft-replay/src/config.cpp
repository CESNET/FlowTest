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
{
	SetDefaultValues();
}

void Config::Parse(int argc, char **argv)
{
	SetDefaultValues();

	const option *longOptions = GetLongOptions();
	const char *shortOptions = GetShortOptions();

	char c;
	while ((c = getopt_long(argc, argv, shortOptions, longOptions, nullptr)) != -1) {
		switch (c) {
		case 'c':
			_replicatorConfig = optarg;
			break;
		case 'r':
			_replayTimeMultiplier = std::atof(optarg);
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
		case 'p':
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

	_replayTimeMultiplier = 0;
	_vlanID = 0;
	_loopsCount = 1;
	_help = false;
}

const option *Config::GetLongOptions()
{
	static struct option long_options[] = {
		{"replicator-config", required_argument, nullptr, 'c'},
		{"replay-multiplier", required_argument, nullptr, 'r'},
		{"output-plugin", required_argument, nullptr, 'o'},
		{"vlan-id", required_argument, nullptr, 'v'},
		{"loops", required_argument, nullptr, 'l'},
		{"pcap", required_argument, nullptr, 'p'},
		{"help", no_argument, nullptr, 'h'},
		{nullptr, 0, nullptr, 0}
	};
	return long_options;
}

const char *Config::GetShortOptions()
{
	return "c:r:o:v:l:p:h";
}

void Config::Validate()
{
	if (_help) {
		return;
	}
	if (_replicatorConfig.empty()) {
		throw std::invalid_argument("Missing replicator config argument (-r)");
	}
	if (_outputPlugin.empty()) {
		throw std::invalid_argument("Missing output plugin params (-o)");
	}
	if (_pcapFile.empty()) {
		throw std::invalid_argument("Missing input pcap file argument (-p)");
	}
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
	std::cerr << "Usage: ./ft-replay [options] -c <replicator config file> -p <pcap file> -o <output plugin params>\n";
	std::cerr << "  --replicator-config, -c  ... The replicator config file\n";
	std::cerr << "  --replay-multiplier, -r  ... Replay speed multiplier. [0 - As fast as possible]\n";
	std::cerr << "  --output-plugin, -o      ... The output plugin specification\n";
	std::cerr << "  --vlan-id, -v            ... The vlan ID number\n";
	std::cerr << "  --pcap, -p               ... Input PCAP file\n";
	std::cerr << "  --loops, -o              ... Number of loops over PCAP file. [0 - infinite]\n";
	std::cerr << "  --help, -h               ... Show this help message\n";
}

} // namespace replay
