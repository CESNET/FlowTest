/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Genetic algorithm which accepts network profile, several configuration options
 * and runs the evolution process for a number of generations or until a viable
 * solution is found.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include "profile.h"
#include <memory>
#include <random>
#include <utility>

/**
 * @brief Structure representing an individual solution in the population.
 */
struct Individual {
	Individual(std::vector<bool> genes, Metrics met, MetricsDiff met_diff)
		: genotype(std::move(genes))
		, metrics(std::move(met))
		, diff(std::move(met_diff))
	{
	}
	Individual(const Individual&) = delete;
	Individual(Individual&&) noexcept = default;
	Individual& operator=(const Individual&) = delete;
	Individual& operator=(Individual&&) = default;

	/** Genotype of the individual. */
	std::vector<bool> genotype;
	/** Metrics of the individual. */
	Metrics metrics;
	/** Metric difference form the original profile. */
	MetricsDiff diff;
};

/**
 * @brief Class representing evolution process (genetic algorithm) to find
 * a suitable sample of the original network profile (subset of biflow records) with desired
 * size without changing key metrics of the original profile.
 */
class Evolution {
public:
	explicit Evolution(const EvolutionConfig& cfg, std::shared_ptr<Profile> profile)
		: _cfg(cfg)
		, _profile(std::move(profile))
		, _rnd(std::mt19937_64(cfg.seed))
	{
	}

	/**
	 * @brief Randomly create initial population. All individuals have fitness > 0.
	 */
	void CreateInitialPopulation();

	/**
	 * @brief Run the evolution process.
	 */
	void Run();

	/**
	 * @brief Get the acceptable solution after the evolution process finishes.
	 * @return Individual who satisfied the genetic algorithm criteria.
	 */
	[[nodiscard]] const Individual& GetSolution() const;

	/**
	 * @brief Dump the solution and metrics into files.
	 * @param solutionPath path to a file where the profile sample should be written
	 * @param metricsPath  path to a file where the profile sample metrics should be written
	 */
	void DumpSolution(std::string_view solutionPath, std::string_view metricsPath) const;

private:
	/**
	 * @brief Two-point crossover operator.
	 * @param parent1 first parent's genotype
	 * @param parent2 second parent's genotype
	 * @param rnd     RNG
	 * @return Children genotypes.
	 */
	std::pair<std::vector<bool>, std::vector<bool>> Crossover(
		const std::vector<bool>& parent1,
		const std::vector<bool>& parent2,
		std::mt19937_64& rnd);

	/**
	 * @brief Mutation operator performing shuffle of genes in a randomly selected set of genes.
	 * @param genotype genotype to be mutated
	 * @param rnd      RNG
	 */
	void Mutation(std::vector<bool>& genotype, std::mt19937_64& rnd) const;

	/**
	 * @brief Remove or add genes to satisfy the minimum and the maximum number of allowed genes.
	 * @param genotype genotype to be repaired
	 * @param rnd      RNG
	 */
	void Repair(std::vector<bool>& genotype, std::mt19937_64& rnd) const;

	/**
	 * @brief Recompute fitness statistics of the current population.
	 */
	void UpdateFitnessStats();

	/**
	 * @brief Thread-safe method for creating new offsprings (crossover, mutation, repair, compute
	 * fitness).
	 *
	 * @param start index of the first parent handled
	 * @param cnt   number of parents handled (must be divisible by 2)
	 * @param seed  seed for the RNG
	 * @return List of offsprings (same amount as provided parents).
	 */
	std::vector<Individual> CreateParallelOffsprings(size_t start, uint32_t cnt, uint64_t seed);

	/**
	 * @brief Stochastic universal selection.
	 *
	 * Select parents from the current population which will create next generation.
	 *
	 * @return Parents (as indexes of individuals in the current population).
	 */
	std::vector<size_t> Selection();

	/** Number of threads to be used when creating offsprings. */
	static constexpr auto WORKERS_COUNT {8};

	/** Evolution configuration. */
	EvolutionConfig _cfg;
	/** Maximum number of genes (biflows records) in the sample. */
	uint64_t _maxGenesCnt {};
	/** Minimum number of genes (biflows records) in the sample. */
	uint64_t _minGenesCnt {};
	/** ComputeFitness sum of all individuals in the current population. */
	double _totalFitness {};
	/** Average fitness of the current population. */
	double _avgFitness {};
	/**  Individual with the highest fitness (population index, fitness value). */
	std::pair<size_t, double> _best {0, 0};
	/** Original profile. */
	std::shared_ptr<Profile> _profile;
	/** Random number generator. */
	std::mt19937_64 _rnd;
	/** Current population. */
	std::vector<Individual> _fenotype;
	/** Previous population. */
	std::vector<size_t> _parentIndexes;
};
