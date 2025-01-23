/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Implementation of the biflow class.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "biflow.h"

Biflow::Biflow(std::string_view record)
{
	record = ConsumeField(record, start_time);
	record = ConsumeField(record, end_time);
	record = ConsumeField(record, l3_proto);
	record = ConsumeField(record, l4_proto);
	record = ConsumeField(record, src_port);
	record = ConsumeField(record, dst_port);
	record = ConsumeField(record, packets);
	record = ConsumeField(record, bytes);
	record = ConsumeField(record, packets_rev);
	record = ConsumeField(record, bytes_rev);

	if (!record.empty()) {
		throw std::runtime_error("Unexpected field: '" + std::string(record) + "'");
	}

	if (l3_proto != 4 and l3_proto != 6) {
		throw std::runtime_error(
			"L3 protocol value error. Expected 4 or 6, got: " + std::to_string(l3_proto));
	}

	if (packets + packets_rev == 0) {
		throw std::runtime_error("Sum of packets in a biflow record cannot be zero");
	}

	if (bytes + bytes_rev == 0) {
		throw std::runtime_error("Sum of bytes in a biflow record cannot be zero");
	}

	duration = end_time - start_time;
	packets_total = (packets + packets_rev);
	bytes_per_packet = static_cast<double>(bytes + bytes_rev) / packets_total;
}

template <typename T>
std::string_view Biflow::ConsumeField(std::string_view line, T& value)
{
	const auto nextFieldPos = line.find(',');
	std::string_view field = line.substr(0, nextFieldPos);

	FromString(field, value);
	if (nextFieldPos == std::string_view::npos) {
		return {};
	} else {
		return line.substr(nextFieldPos + 1);
	}
}

std::ostream& operator<<(std::ostream& os, const Biflow& f)
{
	os << TimestampToMilliseconds(f.start_time) << ',';
	os << TimestampToMilliseconds(f.end_time) << ',';
	os << static_cast<int>(f.l3_proto) << ',' << f.l4_proto << ',';
	os << f.src_port << ',' << f.dst_port << ',' << f.packets << ',' << f.bytes << ',';
	os << f.packets_rev << ',' << f.bytes_rev;
	return os;
}

double Biflow::PacketsInInterval(ft::Timestamp start, ft::Timestamp end) const
{
	if (end < start_time || start > end_time) {
		return 0.0;
	}

	if (duration.ToNanoseconds() == 0) {
		return packets_total;
	}

	/* Truncate interval by start or end time of biflow to count only relevant interval into
	 * interval length. */
	if (start < start_time) {
		start = start_time;
	}
	if (end > end_time) {
		end = end_time;
	}

	auto const intervalLen = end - start;
	return (static_cast<double>(intervalLen.ToNanoseconds()) / duration.ToNanoseconds())
		* packets_total;
}

double Biflow::BytesInInterval(ft::Timestamp start, ft::Timestamp end) const
{
	return PacketsInInterval(start, end) * bytes_per_packet;
}

void Biflow::CreateHistogram(ft::Timestamp start, ft::Timestamp interval, unsigned nOfBins)
{
	if (interval.ToNanoseconds() <= 0) {
		throw std::invalid_argument("interval must be positive number greater than zero");
	}
	if (start > start_time) {
		throw std::invalid_argument(
			"global start of histogram should be greater than biflow start");
	}

	start_window_idx = (start_time - start).ToNanoseconds() / interval.ToNanoseconds();
	end_window_idx = (end_time - start).ToNanoseconds() / interval.ToNanoseconds();
	if (end_window_idx > nOfBins) {
		throw std::invalid_argument("bins are not covering all the biflow (nOfBins is too small)");
	}

	pkt_hist.reserve(end_window_idx - start_window_idx + 1);
	auto actStart = start;
	for (unsigned i = 0; i < nOfBins; i++) {
		if (i >= start_window_idx && i <= end_window_idx) {
			pkt_hist.emplace_back(PacketsInInterval(actStart, actStart + interval));
		}
		actStart += interval;
	}
}

double Biflow::GetHistogramBin(unsigned idx) const
{
	if (pkt_hist.size() == 0) {
		std::runtime_error("packet histogram has not been created");
	}

	if (idx >= start_window_idx && idx <= end_window_idx) {
		return pkt_hist[idx - start_window_idx];
	}
	return 0.0;
}
