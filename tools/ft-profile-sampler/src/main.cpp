/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Parse command line arguments and run the genetic algorithm.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "evolution.h"
#include <getopt.h>

using namespace std;

template <>
void FromString(std::string_view str, double& value)
{
	value = std::stod(str.data(), nullptr);
}

void PrintUsage()
{
	std::cerr << "Usage: ./ft-profile-sampler [-q] -u <max relative sample size> -l <min relative "
				 "sample size> "
				 "-i <profile path> -o <sample path> -m <sample metrics path> [-d <acceptable "
				 "deviation>] [-s <seed>] [-g <generations count>] [-p <population size>]\n";
	std::cerr << "  --max-sampling, -u VALUE  Maximum sampling value (mandatory, must be between 0 "
				 "and 1)\n";
	std::cerr << "  --min-sampling, -l VALUE  Minimum sampling value (mandatory, must be between 0 "
				 "and 1)\n";
	std::cerr << "  --input, -i FILE          Path to a CSV file containing the input profile "
				 "(mandatory)\n";
	std::cerr << "  --output, -o FILE         Path to a file where the sample should be written "
				 "(mandatory)\n";
	std::cerr << "  --metrics, -m FILE        Path to a file where metrics of the result should be "
				 "written (mandatory)\n";
	std::cerr << "  --deviation, -d VALUE     Acceptable deviation (%) of each key metric "
				 "from the original profile metric (default: 0.5)\n";
	std::cerr << "  --seed, -s VALUE          Seed for the random number generator to reproduce "
				 "specific run\n";
	std::cerr << "  --generations, -g VALUE   Number of generations (default: 500)\n";
	std::cerr << "  --population, -p VALUE    Population size (default: 16)\n";
	std::cerr
		<< "  --port-limit, -t VALUE    Omit ports which proportional representation "
		   "in the profile is less than a threshold when calculating fitness (default: 0.005).\n";
	std::cerr
		<< "  --proto-limit, -r VALUE   Omit protocols which proportional representation "
		   "in the profile is less than a threshold when calculating fitness (default: 0.005).\n";
	std::cerr << "  --quiet, -q               Do not print any runtime information\n";
	std::cerr << "  --help, -h                Show this help message\n";
}

int main(int argc, char* argv[])
{
	std::ios::sync_with_stdio(false);
	const option longOpts[]
		= {{"max-sampling", required_argument, nullptr, 'u'},
		   {"min-sampling", required_argument, nullptr, 'l'},
		   {"deviation", required_argument, nullptr, 'd'},
		   {"input", required_argument, nullptr, 'i'},
		   {"output", required_argument, nullptr, 'o'},
		   {"metrics", required_argument, nullptr, 'm'},
		   {"seed", required_argument, nullptr, 's'},
		   {"generations", required_argument, nullptr, 'g'},
		   {"population", required_argument, nullptr, 'p'},
		   {"port-limit", required_argument, nullptr, 't'},
		   {"proto-limit", required_argument, nullptr, 'r'},
		   {"quiet", no_argument, nullptr, 'q'},
		   {"help", no_argument, nullptr, 'h'},
		   {nullptr, 0, nullptr, 0}};

	const char* shortOpts = "u:t:r:l:d:i:o:m:s:g:p:qh";

	EvolutionConfig cfg;
	optind = 0;

	string_view profilePath;
	string_view profileSamplePath;
	string_view profileMetricsPath;

	char c;
	try {
		while ((c = getopt_long(argc, argv, shortOpts, longOpts, nullptr)) != -1) {
			switch (c) {
			case 'u':
				FromString(optarg, cfg.maxSampleSize);
				break;
			case 'l':
				FromString(optarg, cfg.minSampleSize);
				break;
			case 'd':
				FromString(optarg, cfg.deviation);
				break;
			case 'i':
				profilePath = optarg;
				break;
			case 'o':
				profileSamplePath = optarg;
				break;
			case 'm':
				profileMetricsPath = optarg;
				break;
			case 's':
				FromString(optarg, cfg.seed);
				break;
			case 'g':
				FromString(optarg, cfg.generations);
				break;
			case 'p':
				FromString(optarg, cfg.population);
				break;
			case 'r':
				FromString(optarg, cfg.protoThreshold);
				break;
			case 't':
				FromString(optarg, cfg.portThreshold);
				break;
			case 'q':
				cfg.verbose = false;
				break;
			case 'h':
				PrintUsage();
				exit(0);
			default:
				exit(1);
			}
		}
	} catch (const runtime_error& e) {
		cerr << "Argument " << argv[optind - 2] << "=" << argv[optind - 1]
			 << " parsing error: " << e.what() << endl;
		exit(1);
	}

	if (profilePath.empty() || profileSamplePath.empty() || profileMetricsPath.empty()) {
		cerr << "The following options are mandatory: -l, -u, -i, -o, -m" << endl;
		exit(1);
	}

	if (cfg.maxSampleSize <= cfg.minSampleSize and cfg.minSampleSize > 0
		and cfg.maxSampleSize <= 1) {
		cerr << "Maximum sample size: " << cfg.maxSampleSize
			 << " must be higher than minimum sample size: " << cfg.minSampleSize << endl;
		cerr << "Both values must be between 0 (excluded) and 1 (included)." << endl;
		exit(1);
	}

	if (cfg.protoThreshold < 0 or cfg.protoThreshold > 1) {
		cerr << "Protocol proportional representation limit sample size must be between 0 and 1."
			 << endl;
		exit(1);
	}

	if (cfg.portThreshold < 0 or cfg.portThreshold > 1) {
		cerr << "Protocol proportional representation limit sample size must be between 0 and 1."
			 << endl;
		exit(1);
	}

	try {
		if (cfg.verbose) {
			cout << "Loading profile ..." << endl;
		}

		auto profile = make_shared<Profile>(cfg, profilePath);
		auto evolution = Evolution(cfg, profile);
		if (cfg.verbose) {
			cout << "Creating initial population ..." << endl;
		}

		evolution.CreateInitialPopulation();
		if (cfg.verbose) {
			cout << "Starting genetic algorithm ..." << endl;
		}

		evolution.Run();
		evolution.DumpSolution(profileSamplePath, profileMetricsPath);
	} catch (const runtime_error& e) {
		cerr << "Unexpected error: " << e.what() << endl;
		exit(1);
	}

	return 0;
}
