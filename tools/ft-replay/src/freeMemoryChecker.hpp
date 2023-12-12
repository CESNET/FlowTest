/**
 * @file
 * @author Pavel Siska <siska@cesnet.cz>
 * @brief This file contains the declaration of the FreeMemoryChecker class.
 */

#pragma once

#include <stdint.h>
#include <string>

namespace replay {

/**
 * @brief A class for checking free memory.
 */
class FreeMemoryChecker {
public:
	FreeMemoryChecker() = default;

	/**
	 * @brief Check if there is enough free memory to accommodate a file with an optional overhead
	 * percentage.
	 *
	 * This function calculates whether there is enough free memory to accommodate a file of a
	 * certain size, taking into account an optional overhead percentage. If the available free
	 * memory is greater than or equal to the calculated required memory, it returns true;
	 * otherwise, it returns false.
	 *
	 * @throw std::runtime_error if an error occurs while getting the file size or free memory.
	 *
	 * @param filename The path to the file to check.
	 * @param fileSizeOverheadPercentage The optional overhead percentage to consider.
	 * @return True if there is enough free memory; otherwise, false.
	 */
	bool
	IsFreeMemoryForFile(const std::string& filename, size_t fileSizeOverheadPercentage = 0) const;

private:
	uint64_t GetFileSize(const std::string& filename) const;
	uint64_t GetFreeMemory() const;

	std::string GetMemAvailableLine() const;
	size_t ParseMemAvailableLine(std::string_view line) const;
};

} // namespace replay
