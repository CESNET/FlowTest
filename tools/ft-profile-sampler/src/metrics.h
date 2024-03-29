/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Network profile metrics to evaluate quality of the profile sample with respect to
 * the original network profile.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "biflow.h"
#include <cmath>
#include <map>
#include <optional>
#include <vector>

/**
 * @brief Structure containing distribution of a biflow average packet lengths.
 */
struct PacketSizeDistribution {
	/**
	 * @brief Compute fitness.
	 * @return fitness value
	 */
	[[nodiscard]] double ComputeFitness() const
	{
		return pow(small, 2) + pow(medium, 2) + pow(large, 2) + pow(huge, 2);
	}

	/**
	 * @brief Check if all relative differences are below acceptable threshold.
	 * @param deviation acceptable deviation
	 * @return true - is acceptable, false otherwise
	 */
	[[nodiscard]] bool IsAcceptable(double deviation) const
	{
		return (
			small <= deviation && medium <= deviation && large <= deviation && huge <= deviation);
	}

	/** Representation packets with size <= 128 bytes. */
	double small {};
	/** Representation packets with size 129 - 512 bytes. */
	double medium {};
	/** Representation packets with size 513 - 1024 bytes. */
	double large {};
	/** Representation packets with size > 1024 bytes. */
	double huge {};
};

/**
 * @brief Contain relative difference between individual key metrics of two Metrics objects.
 */
struct MetricsDiff {
	MetricsDiff() = default;
	MetricsDiff(const MetricsDiff&) = delete;
	MetricsDiff(MetricsDiff&&) noexcept = default;
	MetricsDiff& operator=(const MetricsDiff&) = delete;
	MetricsDiff& operator=(MetricsDiff&&) = default;

	/**
	 * @brief Compute fitness.
	 * @return fitness value
	 */
	void ComputeFitness();

	/**
	 * @brief Check if all relative differences are below acceptable threshold.
	 * @param deviation acceptable deviation
	 * @return true - is acceptable, false otherwise
	 */
	[[nodiscard]] bool IsAcceptable(double deviance) const;

	/** Fitness value computed based on the relative differences of key metrics. */
	double fitness {};
	/** Relative difference of packets to bytes ratio. */
	double pktsBtsRatio {};
	/** Relative difference of biflows to packets ratio. */
	double bflsPktsRatio {};
	/** Relative difference of biflows to bytes ratio. */
	double bflsBtsRatio {};
	/** Relative difference in IPv4 representation. */
	double ipv4 {};
	/** Relative difference in IPv6 representation. */
	double ipv6 {};
	/** Relative difference in representation of individual L4 protocols. */
	std::map<uint8_t, double> protos;
	/** Relative difference in representation of individual ports. */
	std::map<uint16_t, double> ports;
	/** Relative difference of average packet size distribution. */
	PacketSizeDistribution avgPktSize;
};

/**
 * @brief Structure containing key metrics of profile or profile sample.
 */
struct Metrics {
	/**
	 * @brief Compute metrics from the provided biflows.
	 * @param data list of biflows
	 * @param protoThreshold threshold for proportional representation of protocols to be included
	 * in metrics
	 * @param portThreshold threshold for proportional representation of ports to be included in
	 * metrics
	 * @param filter compute the profile only from the specified biflow subset (optional)
	 */
	Metrics(
		const std::vector<Biflow>& data,
		double protoThreshold,
		double portThreshold,
		std::optional<const std::vector<bool>> filter);
	Metrics() = default;
	Metrics(const Metrics&) = delete;
	Metrics(Metrics&&) noexcept = default;
	Metrics& operator=(const Metrics&) = delete;
	Metrics& operator=(Metrics&&) = default;

	/**
	 * @brief Compute relative difference in key metrics against the reference.
	 * @param ref reference metrics object
	 * @return Key metrics difference.
	 */
	[[nodiscard]] MetricsDiff Diff(const Metrics& ref) const;

	/** Number of packets in the profile (or sample). */
	uint64_t packetsCnt {};
	/** Number of bytes in the profile (or sample). */
	uint64_t bytesCnt {};
	/** Average packet length in a biflow distribution. */
	PacketSizeDistribution pktSizes;
	/** IPv4 representation. */
	double ipv4 {};
	/** IPv6 representation. */
	double ipv6 {};
	/** Packets to bytes ratio. */
	double pktsBtsRatio {};
	/** Biflows to packets ratio. */
	double bflsPktsRatio {};
	/** Biflows to bytes ratio. */
	double bflsBtsRatio {};
	/** Representation of individual L4 protocols. */
	std::map<uint8_t, double> protos;
	/** Representation of individual ports. */
	std::map<uint16_t, double> ports;
};
