/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Statistical model for analyzing flow data.
 *
 * This file defines the structures and classes used for statistical analysis
 * of network flow data, including metrics, segments, rules, and reports.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "flowtable.h"
#include "ipaddr.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>

namespace ft::analyzer {

/**
 * @enum SMMetricType
 * @brief Types of metrics used in statistical analysis.
 */
enum class SMMetricType { PACKETS, BYTES, FLOWS };

/**
 * @struct SMMetric
 * @brief Represents a metric and its difference value.
 */
struct SMMetric {
	SMMetricType key; ///< Type of the metric (PACKETS, BYTES, FLOWS).
	double diff; ///< Difference value for the metric.
};

/**
 * @struct SMSegment
 * @brief Abstract base class for defining segments of flows.
 */
struct SMSegment {
	/**
	 * @brief Checks if a flow belongs to the segment.
	 * @param flow The flow to check.
	 * @return True if the flow belongs to the segment, false otherwise.
	 */
	virtual bool Contains(const Flow& flow) const = 0;

	/// Virtual destructor.
	virtual ~SMSegment() = default;
};

/**
 * @struct SMSubnetSegment
 * @brief Represents a segment defined by source and destination subnets.
 */
struct SMSubnetSegment final : public SMSegment {
	/**
	 * @brief Checks if a flow belongs to the subnet segment.
	 * @param flow The flow to check.
	 * @return True if the flow belongs to the segment, false otherwise.
	 */
	bool Contains(const Flow& flow) const override
	{
		bool srcIn = !source.has_value() || source->Contains(flow.src_ip);
		bool dstIn = !dest.has_value() || dest->Contains(flow.dst_ip);

		if (bidir) {
			bool srcInDst = !source.has_value() || source->Contains(flow.dst_ip);
			bool dstInSrc = !dest.has_value() || dest->Contains(flow.src_ip);
			return (srcIn && dstIn) || (srcInDst && dstInSrc);
		} else {
			return srcIn && dstIn;
		}
	}

	std::optional<IPNetwork> source; ///< Source subnet.
	std::optional<IPNetwork> dest; ///< Destination subnet.
	bool bidir; ///< Bidirectional flag.
};

/**
 * @struct SMTimeSegment
 * @brief Represents a segment defined by a time range.
 */
struct SMTimeSegment final : public SMSegment {
	/**
	 * @brief Checks if a flow belongs to the time segment.
	 * @param flow The flow to check.
	 * @return True if the flow belongs to the segment, false otherwise.
	 */
	bool Contains(const Flow& flow) const override
	{
		return start <= flow.start_time && (!end.has_value() || flow.end_time <= end);
	}

	std::optional<uint64_t> start; ///< Start time.
	std::optional<uint64_t> end; ///< End time.
};

/**
 * @struct SMAllSegment
 * @brief Represents a segment that includes all flows.
 */
struct SMAllSegment final : public SMSegment {
	/**
	 * @brief Checks if a flow belongs to the segment.
	 * @param flow The flow to check.
	 * @return True (all flows belong to this segment).
	 */
	bool Contains(const Flow&) const override { return true; }
};

/**
 * @struct SMComplementSegment
 * @brief Represents a segment that is the complement of another segment.
 */
struct SMComplementSegment final : public SMSegment {
	/**
	 * @brief Checks if a flow belongs to the complement segment.
	 * @param flow The flow to check.
	 * @return Throws a runtime error (complement segment is for report use only).
	 */
	bool Contains(const Flow&) const override
	{
		throw std::runtime_error("Complement segment is intended for use in report only!");
	}
};

/**
 * @struct SMTestAggregation
 * @brief Represents an aggregation of flows for a specific metric and segment.
 */
struct SMTestAggregation {
	/**
	 * @brief Constructs an SMTestAggregation.
	 * @param metric The metric to aggregate.
	 * @param segment The segment to aggregate flows for.
	 */
	explicit SMTestAggregation(SMMetric metric, std::shared_ptr<SMSegment> segment)
		: metric(metric)
		, segment(std::move(segment))
	{
	}

	SMMetric metric; ///< Metric to aggregate.
	std::shared_ptr<SMSegment> segment; ///< Segment to aggregate flows for.
	uint64_t acc = 0; ///< Accumulated value.
};

/**
 * @struct SMTestOutcome
 * @brief Represents the outcome of a statistical test.
 */
struct SMTestOutcome {
	/**
	 * @brief Default constructor.
	 */
	SMTestOutcome() = default;

	/**
	 * @brief Constructs an SMTestOutcome from two aggregations.
	 * @param val The value aggregation.
	 * @param ref The reference aggregation.
	 */
	SMTestOutcome(const SMTestAggregation& val, const SMTestAggregation& ref)
		: metric(val.metric)
		, segment(val.segment)
	{
		value = val.acc;
		reference = ref.acc;

		if (reference == 0) {
			throw std::invalid_argument("Reference cannot be 0 while computing diff.");
		}

		if (value > reference) {
			diff = (value - reference) / static_cast<double>(reference);
		} else {
			diff = (reference - value) / static_cast<double>(reference);
		}
	}

	SMMetric metric; ///< Metric of the test.
	std::shared_ptr<SMSegment> segment; ///< Segment of the test.
	uint64_t value; ///< Value from the test.
	uint64_t reference; ///< Reference value.
	double diff; ///< Difference between value and reference.
};

/**
 * @struct SMRule
 * @brief Represents a rule for statistical analysis.
 */
struct SMRule {
	/**
	 * @brief Constructs an SMRule with metrics and an all-segment.
	 * @param metrics The metrics to use in the rule.
	 */
	SMRule(const std::vector<SMMetric>& metrics)
		: metrics(metrics)
		, segment(std::make_shared<SMAllSegment>())
	{
	}

	/**
	 * @brief Constructs an SMRule with metrics and a subnet segment.
	 * @param metrics The metrics to use in the rule.
	 * @param subnetSegment The subnet segment to use in the rule.
	 */
	SMRule(const std::vector<SMMetric>& metrics, const SMSubnetSegment& subnetSegment)
		: metrics(metrics)
		, segment(std::make_shared<SMSubnetSegment>(subnetSegment))
	{
	}

	/**
	 * @brief Constructs an SMRule with metrics and a time segment.
	 * @param metrics The metrics to use in the rule.
	 * @param timeSegment The time segment to use in the rule.
	 */
	SMRule(const std::vector<SMMetric>& metrics, const SMTimeSegment& timeSegment)
		: metrics(metrics)
		, segment(std::make_shared<SMTimeSegment>(timeSegment))
	{
	}

	std::vector<SMMetric> metrics; ///< Metrics to use in the rule.
	std::shared_ptr<SMSegment> segment; ///< Segment to use in the rule.
};

/**
 * @struct StatisticalReport
 * @brief Represents a report of statistical tests.
 */
struct StatisticalReport {
	std::vector<SMTestOutcome> tests; ///< List of test outcomes.
};

/**
 * @class StatisticalModel
 * @brief Performs statistical analysis on network flow data.
 */
class StatisticalModel {
public:
	/**
	 * @brief Constructs a StatisticalModel.
	 * @param flowsPath Path to the flow data CSV file.
	 * @param refPath Path to the reference data CSV file.
	 * @param startTime Start time for the reference correction.
	 */
	StatisticalModel(std::string_view flowsPath, std::string_view refPath, uint64_t startTime);

	/**
	 * @brief Validates the flow data against a set of rules.
	 * @param rules The rules to validate against.
	 * @param checkComplement Whether to check complement flows (not covered by rules).
	 * @return A report containing the validation results.
	 */
	StatisticalReport Validate(const std::vector<SMRule>& rules, bool checkComplement);

private:
	/**
	 * @brief Aggregates flows based on a set of rules.
	 * @param flows The flow table to aggregate.
	 * @param tests The test aggregations to update.
	 * @return A pair containing the complement bytes and packets.
	 */
	std::pair<uint64_t, uint64_t>
	AggregateByRules(const FlowTable& flows, std::vector<SMTestAggregation>& tests) const;

protected:
	std::unique_ptr<FlowTable> _flows;
	std::unique_ptr<FlowTable> _ref;
};

} // namespace ft::analyzer
