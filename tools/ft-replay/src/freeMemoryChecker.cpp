/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief This file contains the implementation of the FreeMemoryChecker class.
 */

#include "freeMemoryChecker.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>

namespace replay {

bool FreeMemoryChecker::IsFreeMemoryForFile(
	const std::string& filename,
	size_t fileSizeOverheadPercentage) const
{
	uint64_t fileSize = GetFileSize(filename);
	uint64_t freeMemory = GetFreeMemory();
	return fileSize * (1 + fileSizeOverheadPercentage / 100.0) <= freeMemory;
}

uint64_t FreeMemoryChecker::GetFileSize(const std::string& filename) const
{
	struct stat statbuf;
	if (stat(filename.c_str(), &statbuf)) {
		throw std::runtime_error("Failed to get file size of " + filename + ".");
	}
	return statbuf.st_size;
}

uint64_t FreeMemoryChecker::GetFreeMemory() const
{
	return ParseMemAvailableLine(GetMemAvailableLine());
}

std::string FreeMemoryChecker::GetMemAvailableLine() const
{
	std::string_view keyword {"MemAvailable:"};
	std::string_view filename {"/proc/meminfo"};

	std::ifstream reader(filename.data());
	std::string line;

	while (std::getline(reader, line)) {
		if (line.rfind(keyword, 0) == 0) {
			// found
			return line;
		}
	}

	throw std::runtime_error(
		"Unable to locate '" + std::string(keyword) + "' in " + std::string(filename));
}

size_t FreeMemoryChecker::ParseMemAvailableLine(std::string_view line) const
{
	// Expects format: "MemAvailable:    2617540 kB"
	std::istringstream stream(line.data());
	std::string temp;
	size_t value;

	stream >> temp; // "MemAvailable:"
	stream >> value;
	stream >> temp; // "kB"

	if (temp != "kB" || !stream.eof()) {
		throw std::runtime_error("Unexpected format of /proc/meminfo file");
	}

	return value * 1024;
}

} // namespace replay
