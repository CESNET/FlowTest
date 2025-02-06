/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Statistical model for analyzing flow data.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "statisticalmodel.h"
#include <future>

namespace ft::analyzer {

StatisticalModel::StatisticalModel(
	std::string_view flowsPath,
	std::string_view refPath,
	uint64_t startTime)
{
	auto flowsTableFuture
		= std::async([flowsPath]() { return std::make_unique<FlowTable>(flowsPath); });
	auto refTableFuture = std::async(
		[refPath, startTime]() { return std::make_unique<FlowTable>(refPath, startTime); });

	_flows = flowsTableFuture.get();
	_ref = refTableFuture.get();
}

StatisticalReport StatisticalModel::Validate(const std::vector<SMRule>& rules, bool checkComplement)
{
	StatisticalReport report;

	std::vector<SMTestAggregation> flowsAggr;
	for (const auto& rule : rules) {
		for (const auto& metric : rule.metrics) {
			flowsAggr.emplace_back(metric, rule.segment);
		}
	}
	std::vector<SMTestAggregation> refAggr(flowsAggr);

	const auto [complementBytes, complementPackets] = AggregateByRules(*_flows, flowsAggr);
	AggregateByRules(*_ref, refAggr);

	for (size_t i = 0; i < flowsAggr.size(); ++i) {
		report.tests.emplace_back(flowsAggr[i], refAggr[i]);
	}

	if (checkComplement) {
		SMTestOutcome cp;
		cp.segment = std::make_shared<SMComplementSegment>();
		cp.metric = {SMMetricType::PACKETS, 0};
		cp.value = complementPackets;
		cp.reference = 0;
		cp.diff = cp.value != cp.reference ? 1.0 : 0.0;
		report.tests.emplace_back(cp);

		SMTestOutcome cb;
		cb.segment = std::make_shared<SMComplementSegment>();
		cb.metric = {SMMetricType::BYTES, 0};
		cb.value = complementBytes;
		cb.reference = 0;
		cb.diff = cb.value != cb.reference ? 1.0 : 0.0;
		report.tests.emplace_back(cb);
	}

	return report;
}

std::pair<uint64_t, uint64_t> StatisticalModel::AggregateByRules(
	const FlowTable& flows,
	std::vector<SMTestAggregation>& tests) const
{
	uint64_t complementBytes = 0;
	uint64_t complementPackets = 0;

	for (const auto& flow : flows.GetFlows()) {
		bool hit = false;
		for (auto& test : tests) {
			if (test.segment->Contains(flow)) {
				hit = true;
				if (test.metric.key == SMMetricType::BYTES) {
					test.acc += flow.bytes;
				} else if (test.metric.key == SMMetricType::PACKETS) {
					test.acc += flow.packets;
				} else {
					// FLOWS
					++test.acc;
				}
			}
		}
		if (!hit) {
			complementBytes += flow.bytes;
			complementPackets += flow.packets;
		}
	}

	return {complementBytes, complementPackets};
}

} // namespace ft::analyzer
