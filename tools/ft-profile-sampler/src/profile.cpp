/**
 * @file
 * @author Tomas Jansky <Tomas.Jansky@progress.com>
 * @brief Network profile class implementation.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "profile.h"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <numeric>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

Profile::Profile(const EvolutionConfig& cfg, std::string_view path)
	: _cfg(cfg)
{
	// Open the file and obtain a file descriptor
	int fd = open(path.data(), O_RDONLY);
	if (fd == -1) {
		throw std::runtime_error("Failed to open file=" + std::string(path));
	}

	// Obtain the file size
	struct stat fileStat = {};
	if (fstat(fd, &fileStat) == -1) {
		close(fd);
		throw std::runtime_error("Failed to obtain file size.");
	}

	// Map the file into memory
	char* data = static_cast<char*>(mmap(nullptr, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
	if (data == MAP_FAILED) {
		close(fd);
		throw std::runtime_error("Failed to map file into memory.");
	}

	// Parse the header
	long headerLength = strlen(CSV_FORMAT);
	if (fileStat.st_size < headerLength) {
		munmap(data, fileStat.st_size);
		close(fd);
		throw std::runtime_error("Profile CSV file too short (or missing header).");
	}

	if (memcmp(data, CSV_FORMAT, headerLength) != 0) {
		std::string header(data, headerLength);
		munmap(data, fileStat.st_size);
		close(fd);
		throw std::runtime_error("Bad CSV header: " + header + ", expected: " + CSV_FORMAT);
	}

	ParseProfileFile(data + strlen(CSV_FORMAT) + 1, data + fileStat.st_size);

	// Unmap the file from memory and close the file descriptor
	munmap(data, fileStat.st_size);
	close(fd);

	// Compute Biflow histograms
	const ft::Timestamp startTime = std::accumulate(
		_rows.begin(),
		_rows.end(),
		_rows[0].start_time,
		[](ft::Timestamp ts, const Biflow& b2) { return std::min(ts, b2.start_time); });
	const ft::Timestamp endTime = std::accumulate(
		_rows.begin(),
		_rows.end(),
		_rows[0].end_time,
		[](ft::Timestamp ts, const Biflow& b2) { return std::max(ts, b2.end_time); });
	const auto duration = (endTime - startTime);
	auto nOfBins = duration.SecPart();
	if (duration.NanosecPart() > 0) {
		nOfBins++;
	}
	if (nOfBins <= 0) {
		nOfBins = 1;
	}
	_histSize = static_cast<unsigned>(std::ceil(static_cast<double>(nOfBins) / _cfg.windowLength));

	for (auto& biflow : _rows) {
		biflow.CreateHistogram(startTime, ft::Timestamp(_cfg.windowLength, 0), _histSize);
	}

	_metrics = Metrics(_rows, _cfg.protoThreshold, _cfg.portThreshold, {});
}

void Profile::ParseProfileFile(const char* start, const char* end)
{
	const char* lineStart = start;
	for (const char* p = start; p < end; p++) {
		if (*p == '\n') {
			std::string_view line(lineStart, static_cast<size_t>(p - lineStart));
			_rows.emplace_back(line);
			lineStart = p + 1;
		}
	}
}

size_t Profile::GetSize() const
{
	return _rows.size();
}

std::vector<Biflow> Profile::GetFlowSubset(const std::vector<bool>& genotype) const
{
	std::vector<Biflow> dataFrame;
	dataFrame.reserve(std::count(genotype.begin(), genotype.end(), true));

	uint64_t index = 0;
	for (const auto& flow : _rows) {
		if (genotype[index]) {
			dataFrame.emplace_back(flow);
		}
		index++;
	}

	return dataFrame;
}

std::pair<Metrics, MetricsDiff> Profile::GetGenotypeMetrics(const std::vector<bool>& genotype) const
{
	auto metrics = Metrics(_rows, 0, 0, genotype);
	auto diff = metrics.Diff(_metrics);
	return {std::move(metrics), std::move(diff)};
}

const Metrics& Profile::GetMetrics() const
{
	return _metrics;
}
