/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Defines the FlowTable class for managing and processing network flow records.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "flowtable.h"
#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace ft::analyzer {

FlowTable::FlowTable(std::string_view path, uint64_t startTime)
{
	int fd = -1;
	char* data = nullptr;
	struct stat fileStat = {};

	try {
		// Open the file and obtain a file descriptor
		fd = open(path.data(), O_RDONLY);
		if (fd == -1) {
			throw std::runtime_error(
				"Failed to open file: \"" + std::string(path) + "\", error: \""
				+ std::string(std::strerror(errno)) + "\".");
		}

		// Obtain the file size
		if (fstat(fd, &fileStat) == -1) {
			throw std::runtime_error(
				"Failed to obtain file size, error: \"" + std::string(std::strerror(errno))
				+ "\".");
		}

		// Map the file into memory
		data = static_cast<char*>(mmap(nullptr, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
		if (data == MAP_FAILED) {
			throw std::runtime_error(
				"Failed to map file into memory, error: \"" + std::string(std::strerror(errno))
				+ "\".");
		}

		// Parse the header
		long headerLength = strlen(CSV_FORMAT);
		if (fileStat.st_size < headerLength) {
			throw std::runtime_error("FlowTable CSV file too short (or missing header).");
		}

		if (memcmp(data, CSV_FORMAT, headerLength) != 0) {
			std::string header(data, headerLength);
			throw std::runtime_error(
				"Bad CSV header: " + header + ", expected: " + CSV_FORMAT + ".");
		}

		ParseProfileFile(data + strlen(CSV_FORMAT) + 1, data + fileStat.st_size);
	} catch (...) {
		// Unmap the file from memory and close the file descriptor
		if (data != nullptr && data != MAP_FAILED) {
			munmap(data, fileStat.st_size);
		}
		if (fd != -1) {
			close(fd);
		}
		throw;
	}
	// Unmap the file from memory and close the file descriptor
	munmap(data, fileStat.st_size);
	close(fd);

	// Correct time by generator start, relevant for reference
	if (startTime > 0) {
		for (auto& flow : _flows) {
			flow.start_time += startTime;
			flow.end_time += startTime;
		}
	}
}

const std::vector<Flow>& FlowTable::GetFlows() const
{
	return _flows;
}

void FlowTable::ParseProfileFile(const char* start, const char* end)
{
	size_t counter = 0;
	const char* lineStart = start;

	_logger->info("Start parsing flows...");
	const char* p;
	for (p = start; p < end; p++) {
		if (*p == '\n') {
			std::string_view line(lineStart, static_cast<size_t>(p - lineStart));
			_flows.emplace_back(line);
			lineStart = p + 1;

			counter++;
		}
	}
	if (p != lineStart) {
		throw std::runtime_error("Error parsing last line in CSV file (line must end with \\n).");
	}
	_logger->info("Parsed {} flows.", counter);
}

} // namespace ft::analyzer
