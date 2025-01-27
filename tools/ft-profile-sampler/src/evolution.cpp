/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Genetic algorithm implementation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "evolution.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <iomanip>
#include <utility>

#define DIVIDE_ROUND_UP(a, b) (a + b - 1) / (b)

void Evolution::CreateInitialPopulation()
{
	uint64_t profileSize = _profile->GetSize();
	// count minimum and maximum number of genes to satisfy sampling range
	_minGenesCnt = floor(_cfg.minSampleSize * static_cast<double>(profileSize));
	_maxGenesCnt = ceil(_cfg.maxSampleSize * static_cast<double>(profileSize));

	std::uniform_int_distribution<uint64_t> geneCountDistrib(_minGenesCnt, _maxGenesCnt);
	std::uniform_int_distribution<uint64_t> geneDistrib(0, profileSize);
	_fenotype.reserve(_cfg.population);

	while (_fenotype.size() < _cfg.population) {
		std::vector<bool> genotype(profileSize, false);
		uint64_t geneCount = geneCountDistrib(_rnd);
		// pick random genes (with respect to the maximum and minimum number of genes)
		for (uint64_t j = 0; j < geneCount; j++) {
			uint64_t index = geneDistrib(_rnd);
			if (genotype[index]) {
				j--;
			} else {
				genotype[index] = true;
			}
		}

		// measure fitness and add the individual only if the fitness value is > 0
		auto metrics = _profile->GetGenotypeMetrics(genotype);
		if (metrics.second.fitness > 0) {
			_fenotype.emplace_back(
				std::move(genotype),
				std::move(metrics.first),
				std::move(metrics.second));
		}
	}
	UpdateFitnessStats();
}

void Evolution::Run()
{
	// run for specified number of generations or until an acceptable solution is found
	for (uint32_t generation = 0; generation < _cfg.generations; generation++) {
		std::vector<Individual> nextGen;

		// the +1 is necessary in case the population size is odd
		nextGen.reserve(_cfg.population + 1);

		// select parents which will produce offsprings
		_parentIndexes = Selection();

		// creating offsprings can be done in parallel
		// determine how many parents will be handled by each thread
		// must be a factor of 2 due to the crossover operator
		size_t batchStart = 0;
		size_t batchSize = DIVIDE_ROUND_UP(_parentIndexes.size(), _cfg.workersCount);
		if (batchSize % 2 == 1) {
			batchSize++;
		}

		// each worker needs its own seed so that the run of a genetic algorithm is replicable
		std::uniform_int_distribution<uint64_t> seedDistr(0, UINT64_MAX);
		std::vector<std::future<std::vector<Individual>>> threads;
		uint32_t batchCount = DIVIDE_ROUND_UP(_parentIndexes.size(), batchSize);
		for (uint32_t i = 0; i < batchCount; i++) {
			if (batchStart + batchSize > _parentIndexes.size()) {
				// shrink the last batch
				batchSize = _parentIndexes.size() - batchStart;
			}

			// start workers which create new offsprings
			threads.emplace_back(std::async(
				&Evolution::CreateParallelOffsprings,
				this,
				batchStart,
				batchSize,
				seedDistr(_rnd)));
			batchStart += batchSize;
		}

		// wait for all threads to finish and collect offsprings
		for (auto& fut : threads) {
			auto res = fut.get();
			nextGen.insert(
				nextGen.end(),
				std::make_move_iterator(res.begin()),
				std::make_move_iterator(res.end()));
		}

		// remove the last offspring in case the population size is odd
		if (_cfg.population % 2 == 1) {
			nextGen.pop_back();
		}

		// elitism - replace the last offspring with the best individual from the current population
		std::swap(nextGen[nextGen.size() - 1], _fenotype[_best.first]);

		// offsprings become the current population
		std::swap(nextGen, _fenotype);
		UpdateFitnessStats();
		if (_cfg.verbose) {
			std::cout << "Generation: " << generation << ", best solution: " << _best.second
					  << ", average solution: " << _avgFitness << '\n';
		}

		// check if any of the current solutions is acceptable (it may not be the best solution)
		for (size_t i = 0; i < _fenotype.size(); i++) {
			if (_fenotype[i].diff.IsAcceptable(_cfg.deviation)) {
				_best = {i, _fenotype[i].diff.fitness};
				if (_cfg.verbose) {
					std::cout << "Acceptable solution found with fitness: " << _best.second << '\n';
				}

				return;
			}
		}
	}
}

const Individual& Evolution::GetSolution() const
{
	return _fenotype[_best.first];
}

std::pair<std::vector<bool>, std::vector<bool>> Evolution::Crossover(
	const std::vector<bool>& parent1,
	const std::vector<bool>& parent2,
	std::mt19937_64& rnd)
{
	// simple 2-point crossover
	// offsprings are created by swapping area of genes between parents
	// the swapped area is determined by 2 random points
	std::uniform_int_distribution<int64_t> dist(0, static_cast<int64_t>(parent1.size()));
	int64_t pointA = dist(rnd);
	int64_t pointB = dist(rnd);
	if (pointA > pointB) {
		std::swap(pointA, pointB);
	}

	auto childA = parent1;
	auto childB = parent2;
	std::swap_ranges(childA.begin() + pointA, childA.begin() + pointB, childB.begin() + pointA);
	return {childA, childB};
}

void Evolution::Mutation(std::vector<bool>& genotype, std::mt19937_64& rnd) const
{
	// determine how many genes should be mutated (shuffled)
	double mutationPressure
		= (_avgFitness < _cfg.mutationCtrl) ? _cfg.mutationHigh : _cfg.mutationLow;
	int64_t geneShuffleSize = floor(static_cast<double>(genotype.size()) * mutationPressure);
	std::uniform_int_distribution<int64_t> distr(
		0,
		static_cast<int64_t>(genotype.size()) - geneShuffleSize);
	int64_t shuffleStart = distr(rnd);
	std::shuffle(
		genotype.begin() + shuffleStart,
		genotype.begin() + shuffleStart + geneShuffleSize,
		rnd);
}

void Evolution::Repair(std::vector<bool>& genotype, std::mt19937_64& rnd) const
{
	uint64_t geneCnt = std::count(genotype.begin(), genotype.end(), true);
	std::uniform_int_distribution<uint64_t> distr(0, genotype.size());
	while (geneCnt < _minGenesCnt) {
		size_t index = distr(rnd);
		if (not genotype[index]) {
			genotype[index] = true;
			geneCnt++;
		}
	}

	while (geneCnt > _maxGenesCnt) {
		size_t index = distr(rnd);
		if (genotype[index]) {
			genotype[index] = false;
			geneCnt--;
		}
	}
}

std::vector<size_t> Evolution::Selection()
{
	uint32_t parentsCount = _fenotype.size();
	// the number of parents must be even
	if (parentsCount % 2 == 1) {
		parentsCount++;
	}

	// the step used to advance the wheel
	double pointDistance = _totalFitness / static_cast<double>(parentsCount);
	// select the first parent at a random position of the first step
	std::uniform_real_distribution<double> dis(0.0, pointDistance);
	double currentPosition = dis(_rnd);
	double fitnessSum = _fenotype[0].diff.fitness;
	size_t index = 0;

	std::vector<size_t> parents;
	while (parents.size() < parentsCount) {
		// sum fitness of subsequent individuals until it reaches the next point on the wheel
		while (fitnessSum < currentPosition) {
			index++;
			fitnessSum += _fenotype[index].diff.fitness;
		}

		// pick the individual as a parent and advance to the next position on the roulette wheel
		parents.emplace_back(index);
		currentPosition += pointDistance;
	}

	// shuffle the parent vector to ensure random pairing
	std::shuffle(parents.begin(), parents.end(), _rnd);
	return parents;
}

void Evolution::UpdateFitnessStats()
{
	_totalFitness = 0;
	_best = {0, 0};
	for (size_t i = 0; i < _fenotype.size(); i++) {
		_totalFitness += _fenotype[i].diff.fitness;
		if (_fenotype[i].diff.fitness > _best.second) {
			_best.first = i;
			_best.second = _fenotype[i].diff.fitness;
		}
	}
	_avgFitness = _totalFitness / _cfg.population;
}

std::vector<Individual>
Evolution::CreateParallelOffsprings(size_t start, uint32_t cnt, uint64_t seed)
{
	std::vector<Individual> offsprings;
	std::mt19937_64 rnd(seed);
	for (size_t i = start; i < start + cnt; i += 2) {
		auto children = Crossover(
			_fenotype[_parentIndexes[i]].genotype,
			_fenotype[_parentIndexes[i + 1]].genotype,
			rnd);
		Mutation(children.first, rnd);
		Mutation(children.second, rnd);

		Repair(children.first, rnd);
		Repair(children.second, rnd);

		auto metricsA = _profile->GetGenotypeMetrics(children.first);
		auto metricsB = _profile->GetGenotypeMetrics(children.second);
		offsprings.emplace_back(
			std::move(children.first),
			std::move(metricsA.first),
			std::move(metricsA.second));
		offsprings.emplace_back(
			std::move(children.second),
			std::move(metricsB.first),
			std::move(metricsB.second));
	}

	return offsprings;
}

std::ostream& operator<<(std::ostream& os, const EvolutionConfig& cfg)
{
	os << "SAMPLING: " << cfg.minSampleSize << " - " << cfg.maxSampleSize << '\n';
	os << "GENERATIONS: " << cfg.generations << '\n';
	os << "POPULATION: " << cfg.population << '\n';
	os << "MAX DEVIATION: " << cfg.deviation << '\n';
	os << "SEED: " << cfg.seed << "\n\n";
	return os;
}

void Evolution::DumpSolution(std::string_view solutionPath, std::string_view metricsPath) const
{
	const auto& best = GetSolution();
	auto flows = _profile->GetFlowSubset(best.genotype);
	std::ofstream sampleFile(solutionPath.data());
	sampleFile << Profile::CSV_FORMAT << '\n';
	for (const auto& flow : flows) {
		sampleFile << flow << '\n';
	}

	const auto& origMetrics = _profile->GetMetrics();
	std::ofstream metricsFile(metricsPath.data());
	metricsFile << _cfg;

	metricsFile << std::fixed << std::setprecision(6);
	metricsFile << "PACKETS: " << best.metrics.packetsCnt << "\n";
	metricsFile << "BYTES: " << best.metrics.bytesCnt << "\n";
	metricsFile << "FITNESS: " << best.diff.fitness << "\n\n";

	metricsFile << "METRIC\t\tORIGINAL\tSOLUTION\tDIFF (%)\n";
	metricsFile << "PKTS/BTS\t" << origMetrics.pktsBtsRatio << '\t' << best.metrics.pktsBtsRatio
				<< '\t' << best.diff.pktsBtsRatio << '\n';
	metricsFile << "FLOWS/PKTS\t" << origMetrics.bflsPktsRatio << '\t' << best.metrics.bflsPktsRatio
				<< '\t' << best.diff.bflsPktsRatio << '\n';
	metricsFile << "FLOWS/BTS\t" << origMetrics.bflsBtsRatio << '\t' << best.metrics.bflsBtsRatio
				<< '\t' << best.diff.bflsBtsRatio << "\n\n";

	metricsFile << "L3 PROTO\tORIGINAL\tSOLUTION\tDIFF (%)\n";
	metricsFile << "IPv4\t\t" << origMetrics.ipv4 << '\t' << best.metrics.ipv4 << '\t'
				<< best.diff.ipv4 << '\n';
	metricsFile << "IPv6\t\t" << origMetrics.ipv6 << '\t' << best.metrics.ipv6 << '\t'
				<< best.diff.ipv6 << "\n\n";

	metricsFile << "L4 PROTO\tORIGINAL\tSOLUTION\tDIFF (%)\n";
	for (const auto& proto : best.diff.protos) {
		uint8_t name = proto.first;
		metricsFile << static_cast<int>(name) << "\t\t\t" << origMetrics.protos.at(name) << '\t'
					<< best.metrics.protos.at(name) << '\t' << proto.second << '\n';
	}
	metricsFile << '\n';

	metricsFile << "PORTS\t\tORIGINAL\tSOLUTION\tDIFF (%)\n";
	for (const auto& port : best.diff.ports) {
		uint16_t name = port.first;
		metricsFile << name << "\t\t\t" << origMetrics.ports.at(name) << '\t'
					<< best.metrics.ports.at(name) << '\t' << port.second << '\n';
	}
	metricsFile << '\n';

	metricsFile << "AVG PKT SIZE\tORIGINAL\tSOLUTION\tDIFF (%)\n";
	metricsFile << "(0, 128]"
				<< "\t\t" << origMetrics.pktSizes.small << '\t' << best.metrics.pktSizes.small
				<< '\t' << best.diff.avgPktSize.small << '\n';
	metricsFile << "(128, 512]"
				<< "\t\t" << origMetrics.pktSizes.medium << '\t' << best.metrics.pktSizes.medium
				<< '\t' << best.diff.avgPktSize.medium << '\n';
	metricsFile << "(512, 1024]"
				<< "\t\t" << origMetrics.pktSizes.large << '\t' << best.metrics.pktSizes.large
				<< '\t' << best.diff.avgPktSize.large << '\n';
	metricsFile << "(1024, 9000]"
				<< "\t" << origMetrics.pktSizes.huge << '\t' << best.metrics.pktSizes.huge << '\t'
				<< best.diff.avgPktSize.huge << "\n";
}
