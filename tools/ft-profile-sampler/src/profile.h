/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Network profile class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "metrics.h"

/**
 * @brief Class representing the input network environment profile
 * consisting of individual biflow records.
 */
class Profile {
public:
	static constexpr auto CSV_FORMAT
		= "START_TIME,END_TIME,L3_PROTO,L4_PROTO,SRC_PORT,DST_PORT,"
		  "PACKETS,BYTES,PACKETS_REV,BYTES_REV";

	/**
	 * @brief Initialize the profile from the input CSV file containing biflow records.
	 * @param path path to the profile file.
	 */
	explicit Profile(std::string_view path);

	/**
	 * @brief Get number of biflow records in the profile.
	 * @return number of biflow records
	 */
	[[nodiscard]] size_t GetSize() const;

	/**
	 * @brief Get subset of biflow records based on the provided indexes (genotype).
	 * @param genotype list of indications which biflows should be present in the subset
	 * @return subset of the original profile
	 */
	[[nodiscard]] std::vector<Biflow> GetFlowSubset(const std::vector<bool>& genotype) const;

	/**
	 * @brief Compute metrics for a given genotype in relation to the original profile.
	 * @param genotype genotype to be used
	 * @return Metrics, MetricsDiff objects
	 */
	[[nodiscard]] std::pair<Metrics, MetricsDiff>
	GetGenotypeMetrics(const std::vector<bool>& genotype) const;

	/**
	 * @brief Get Metrics object of the original profile.
	 * @return original profile Metrics object
	 */
	[[nodiscard]] const Metrics& GetMetrics() const;

	~Profile() = default;

private:
	/**
	 * @brief Parse the input file and create a biflow record from each line.
	 * @param start beginning of the file
	 * @param end   end of the file
	 */
	void ParseProfileFile(const char* start, const char* end);

	/** Biflow records in the original profile. */
	std::vector<Biflow> _rows;
	/** Metrics object of the original profile. */
	Metrics _metrics;
};
