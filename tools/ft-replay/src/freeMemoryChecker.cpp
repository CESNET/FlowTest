/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief This file contains the implementation of the FreeMemoryChecker class.
 */

#include "freeMemoryChecker.hpp"

#include <stdexcept>
#include <sys/stat.h>
#include <sys/sysinfo.h>

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
	struct sysinfo info;
	if (sysinfo(&info)) {
		throw std::runtime_error("Failed to get sysinfo.");
	}
	return info.freeram;
}

} // namespace replay
