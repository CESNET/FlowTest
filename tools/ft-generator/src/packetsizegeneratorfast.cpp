/**
 *
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Packet size value generator
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "packetsizegeneratorfast.h"
#include "randomgenerator.h"

#include <glpk.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <numeric>

namespace generator {

static constexpr double MAX_DIFF_RATIO = 0.01;

static std::vector<double> FindOptimalProbabilities(
	uint64_t desiredSum,
	uint64_t numPkts,
	const std::vector<double>& means,
	std::vector<double>& probs)
{
	/**
	 * Minimize:
	 *     - abs(p0 - desired_p0) + abs(p1 - desired_p1) + ... + abs(pn - desired_pn)
	 *
	 * Subject to:
	 *     - desired_sum * (1 - alpha)
	 *           <= p0 * x0 * pkt_count + p1 * x1 * pkt_count + ... + pn * xn * pkt_count
	 *           <= desired_sum * (1 + alpha)
	 *         where alpha is specified by the MAX_DIFF_RATIO constant
	 *     - p0 + p1 + ... + pn = 1.0
	 *     - 0.0 <= {p0, p1, ..., pn} <= 1.0
	 *
	 * Note: As 'abs' is not a linear function, it has to be "emulated" using helper variables and a
	 * set of 2 constraints (see
	 * https://optimization.cbe.cornell.edu/index.php?title=Optimization_with_absolute_values)
	 *
	 *     abs(p0 - desired_p0) + abs(p1 - desired_p1) + ... + abs(pn - desired_pn)
	 *
	 *     ... becomes ...
	 *
	 *     e0 + e1 + ... + en
	 *
	 *     ... with constraints ...
	 *
	 *     p0 - desired_p0 <= e0
	 *     -(p0 - desired_p0) <= e0
	 *     ...
	 *
	 * Variables:
	 *     - pI are the output probabilities for inidividual intervals
	 *     - desired_pI are the ideal probabilities we'd like to achieve
	 *     - desired_sum is the sum of bytes we want to achieve
	 *     - pkt_count is the number of packets
	 *     - xI is the average value for the Ith interval
	 */

	assert(means.size() == probs.size());
	const int nProbs = probs.size();

	// WARNING: GLPK indexing starts from 1, not 0 !!!

	// Create problem instance
	std::unique_ptr<glp_prob, decltype(&glp_free)> lp(glp_create_prob(), &glp_free);
	glp_set_obj_dir(lp.get(), GLP_MIN); // Our goal is to minimize the objective function

	/**
	 * Rows represent constraints... The number of rows we need:
	 * 2N ... for error variables to "emulate" abs(pn - desired_pn) as it is non-linear
	 * 1 ... sum to 1
	 * 1 ... desired sum
	 */
	glp_add_rows(lp.get(), nProbs * 2 + 2);

	// Set constraint that the probabilities sum to one
	glp_set_row_name(lp.get(), 1, "sum_to_one_constr");
	glp_set_row_bnds(lp.get(), 1, GLP_FX, 1.0, 1.0);

	/**
	 * Set constraint for the sum we want to reach
	 * (1 - alpha) * desired_sum <= p0 * x0 * pkt_count + ... <= (1 + alpha) * desired_sum
	 * where alpha is specified by the MAX_DIFF_RATIO constant
	 */
	glp_set_row_name(lp.get(), 2, "desired_sum_constr");
	glp_set_row_bnds(
		lp.get(),
		2,
		GLP_DB,
		desiredSum * (1.0 - MAX_DIFF_RATIO),
		desiredSum * (1.0 + MAX_DIFF_RATIO));

	// Setup constraints for the helper error variables "emulating" absolute value
	for (int i = 0; i < nProbs; i++) {
		glp_set_row_name(lp.get(), 3 + 2 * i, ("abs1_constr_e" + std::to_string(i)).c_str());
		glp_set_row_bnds(lp.get(), 3 + 2 * i, GLP_UP, 0.0, probs[i]);

		glp_set_row_name(lp.get(), 3 + 2 * i + 1, ("abs2_constr_e" + std::to_string(i)).c_str());
		glp_set_row_bnds(lp.get(), 3 + 2 * i + 1, GLP_UP, 0.0, -probs[i]);
	}

	/**
	 * Columns represent variables in the equation... The number of columns we need:
	 * N ... p
	 * N ... e
	 * The rest are constant literals (in the scope of the solution)
	 */
	glp_add_cols(lp.get(), 2 * nProbs);

	// p variables
	for (int i = 0; i < nProbs; i++) {
		glp_set_col_name(lp.get(), i + 1, ("p" + std::to_string(i)).c_str());
		glp_set_col_bnds(lp.get(), i + 1, GLP_DB, 0.0, 1.0); // 0 <= p <= 1
	}
	// e variables
	for (int i = 0; i < nProbs; i++) {
		glp_set_col_name(lp.get(), nProbs + i + 1, ("e" + std::to_string(i)).c_str());
		glp_set_col_bnds(lp.get(), nProbs + i + 1, GLP_DB, 0.0, 1.0); // 0 <= e <= 1
	}

	// Objective function coefficients - all ones for e (e1 + e2 + ... + en), zeros elsewhere
	for (int i = 0; i < nProbs; i++) {
		glp_set_obj_coef(lp.get(), i + 1, 0.0);
	}
	for (int i = 0; i < nProbs; i++) {
		glp_set_obj_coef(lp.get(), nProbs + i + 1, 1.0);
	}

	/**
	 * Here we set up the coefficient for the rows and columns
	 *
	 * Those are the literal constants that do not change during the solution,
	 * which are also the desired probabilities, the desired sum, and the number of packets!
	 */
	std::vector<int> indices = {0}; // Filler values because indexing starts from 1
	std::vector<double> values = {0};
	for (int i = 0; i < nProbs; i++) {
		indices.push_back(i + 1);
		values.push_back(1.0);
	}
	// Set coefficients for the sum to one constraint of p values - all 1s
	glp_set_mat_row(lp.get(), 1, indices.size() - 1, indices.data(), values.data());

	// Set coefficients for the sum constraint (the `x0 * pkt_count` part, p0 is our variable)
	indices = {0};
	values = {0};
	for (int i = 0; i < nProbs; i++) {
		indices.push_back(i + 1);
		values.push_back(means[i] * numPkts);
	}
	glp_set_mat_row(lp.get(), 2, indices.size() - 1, indices.data(), values.data());

	/**
	 * Coefficients for the e variable constraints
	 *
	 * p0 - desired_p0 <= e0
	 *  -(p0 - desired_p0) <= e0
	 *
	 * is represented as
	 *
	 * p0 - e0 <= desired_p0     ... coefficients (1, -1)
	 *  -p0 + e0 <= -desired_p0  ... coefficients (-1, -1)
	 */
	for (int i = 0; i < nProbs; i++) {
		// The starting 0 in indices and values are filler
		indices = {0, i + 1, i + nProbs + 1};

		values = {0, 1, -1};
		glp_set_mat_row(lp.get(), 3 + 2 * i, indices.size() - 1, indices.data(), values.data());

		values = {0, -1, -1};
		glp_set_mat_row(lp.get(), 3 + 2 * i + 1, indices.size() - 1, indices.data(), values.data());
	}

	// Parameters for the runtime of the solver
	glp_smcp params;
	glp_init_smcp(&params);
	// GLPK logs the state of the computation to stdout by default, disable it
	params.msg_lev = GLP_MSG_OFF;
	// Run the solver using the simplex method
	glp_simplex(lp.get(), &params);

	auto status = glp_get_status(lp.get());
	if (status != GLP_OPT && status != GLP_FEAS) {
		// Could not find a solution, return empty result
		return {};
	}

	// Collect values from the p variables, which are our resulting probabilities
	std::vector<double> result(nProbs);
	for (int i = 0; i < nProbs; i++) {
		double p = glp_get_col_prim(lp.get(), i + 1);
		result[i] = p;
	}
	return result;
}

PacketSizeGeneratorFast::PacketSizeGeneratorFast(
	const std::vector<IntervalInfo>& intervals,
	uint64_t numPkts,
	uint64_t numBytes)
	: _intervals(intervals)
	, _numPkts(numPkts)
	, _numBytes(numBytes)
	, _availCount(intervals.size(), 0)
	, _assignedCount(intervals.size(), 0)
{
	assert(!_intervals.empty());
}

void PacketSizeGeneratorFast::GetValueExact(uint64_t value)
{
	_logger->trace("GetValueExact({})", value);

	unsigned int idx;

	if (value < _intervals.front()._from) {
		// Lower than the lowest value, choose the left bound
		idx = 0;
	} else if (value > _intervals.back()._to) {
		// Higher than the highest value, choose the right bound
		idx = _intervals.size() - 1;
	} else {
		// Find the interval this value belongs to
		for (idx = 0; idx < _intervals.size(); idx++) {
			const auto& interval = _intervals[idx];
			if (value >= interval._from && value <= interval._to) {
				break;
			}
		}

		if (idx == _intervals.size()) {
			throw std::logic_error(
				"Interval for specified value not found, but it should have been. This means that "
				"invalid intervals have been provided and is likely a bug.");
		}
	}

	if (_availCount[idx] > 0) {
		_availCount[idx]--;
	}
	_assignedCount[idx]++;
	_assignedPkts++;
	_assignedBytes += value;
}

void PacketSizeGeneratorFast::PlanRemaining()
{
	std::vector<double> probs;
	std::vector<double> means;
	for (const auto& interval : _intervals) {
		probs.push_back(interval._probability);
		means.push_back(interval._from * 0.5 + interval._to * 0.5);
	}

	for (unsigned int i = 0; i < _assignedCount.size(); i++) {
		if (_assignedCount[i] > 0) {
			// Calculate how much from the certain interval was already assigned and adjust the base
			// probabilities accordingly
			double adjustment = _assignedCount[i] / (probs[i] * _numPkts);
			probs[i] = std::max<double>(0.0, probs[i] - adjustment);
		}
	}

	// Try to find best interval probabilities for the remainder of the unassigned packets/bytes
	uint64_t remBytes = _numBytes >= _assignedBytes ? _numBytes - _assignedBytes : 0;
	uint64_t remPkts = _numPkts >= _assignedPkts ? _numPkts - _assignedPkts : 0;
	auto solution = FindOptimalProbabilities(remBytes, remPkts, means, probs);
	if (solution.empty()) {
		// No solution found, set the lowest/highest interval to all remaining packets
		for (auto& count : _availCount) {
			count = 0;
		}

		uint64_t remAvg = remPkts > 0 ? remBytes / remPkts : 0;
		/**
		 * If the average is inside the interval bounds (i.e. higher or equal than the first
		 * interval and lower or equal than the last interval), the solution should always be found.
		 * Assert that the average is out of those bounds, otherwise our assumption doesn't hold up,
		 * and there might be an error somewhere.
		 */
		assert(remAvg < _intervals.front()._to || remAvg > _intervals.back()._from);

		if (remAvg < _intervals.front()._to) {
			_availCount.front() = remPkts;
		} else {
			_availCount.back() = remPkts;
		}
		_logger->trace("No solution found... RemAvg={}", remAvg);
		return;
	}

	// We found optimal probabilities, assign available counts based on them
	assert(_availCount.size() == solution.size());
	for (unsigned int i = 0; i < _availCount.size(); i++) {
		_availCount[i] = solution[i] * remPkts;
	}

	_logger->trace("Solution:");
	for (unsigned int i = 0; i < _availCount.size(); i++) {
		auto& iv = _intervals[i];
		_logger->trace("  {}-{}: {}", iv._from, iv._to, _availCount[i]);
	}
}

uint64_t PacketSizeGeneratorFast::GetValue()
{
	uint64_t totalCount = std::accumulate(_availCount.begin(), _availCount.end(), 0);
	auto& rng = RandomGenerator::GetInstance();
	unsigned int idx;

	if (totalCount == 0) {
		// All bins empty, choose one completely randomly
		idx = rng.RandomUInt(0, _intervals.size() - 1);

	} else {
		// Randomly choose bin with respect to their remaining counts
		int64_t rand = rng.RandomUInt(0, totalCount - 1);
		int64_t sum = 0;
		for (idx = 0; idx < _availCount.size(); idx++) {
			sum += _availCount[idx];
			if (sum > rand) {
				break;
			}
		}
	}

	// Pick a value from the interval and update the counters
	uint64_t value = rng.RandomUInt(_intervals[idx]._from, _intervals[idx]._to);
	if (_availCount[idx] > 0) {
		_availCount[idx]--;
	}
	_assignedPkts++;
	_assignedBytes += value;

	_logger->trace(
		"GetValue() -> {}, desiredSum={} desiredPkts={} assignedSum={} assignedPkts={}",
		value,
		_numBytes,
		_numPkts,
		_assignedBytes,
		_assignedPkts);

	return value;
}

void PacketSizeGeneratorFast::PrintReport()
{
	double dBytes = _numBytes == 0
		? 0.0
		: std::abs(int64_t(_numBytes) - int64_t(_assignedBytes)) / double(_numBytes);

	double dPkts = _numPkts == 0
		? 0.0
		: std::abs(int64_t(_numPkts) - int64_t(_assignedPkts)) / double(_numPkts);

	_logger->debug(
		"[Bytes] target={} actual={} (diff={:.2f}%)  [Pkts] target={} actual={} "
		"(diff={:.2f}%)",
		_numBytes,
		_assignedBytes,
		dBytes * 100.0,
		_numPkts,
		_assignedPkts,
		dPkts * 100.0);
}

} // namespace generator
