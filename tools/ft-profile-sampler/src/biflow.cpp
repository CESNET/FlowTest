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
