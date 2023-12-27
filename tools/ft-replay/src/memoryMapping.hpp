/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Wrapper over memory mapping (i.e. mmap)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <sys/mman.h>

namespace replay {

/**
 * @brief Simple RAII wrapper for mapped memory (using Linux mmap).
 */
class MemoryMapping {
public:
	/** @brief Default constructor of empty wrapper */
	MemoryMapping() = default;
	/**
	 * @brief Map file or device into memory.
	 *
	 * These parameters corresponds to parameter of Linux mmap() function. See its
	 * manual page for more details.
	 * @param[in] addr   Address hint at which to create mapping. Can be nullptr.
	 * @param[in] length Length of the mapping (must be greater than 0).
	 * @param[in] prot   Memory protection of the mapping (e.g. PROT_READ, PROT_WRITE, ...)
	 * @param[in] flags  Additional mapping flags (e.g. MAP_SHARED, MAP_PRIVATE)
	 * @param[in] fd     File descriptor of a file (or other object)
	 * @param[in] offset Offset in the file (or other object).
	 *
	 * @throw std::runtime_error if mapping fails.
	 */
	MemoryMapping(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
	/** @brief Destructor that automatically deletes the memory mapping. */
	~MemoryMapping();

	// Disable copy constructors
	MemoryMapping(const MemoryMapping&) = delete;
	MemoryMapping& operator=(const MemoryMapping&) = delete;
	// Enable move constructors
	MemoryMapping(MemoryMapping&& other) noexcept;
	MemoryMapping& operator=(MemoryMapping&& other) noexcept;

	/** @brief Test if the handler holds mapped memory. */
	bool IsValid() noexcept { return _data != MAP_FAILED; };
	/** @brief Get a pointer to the mapped area. */
	void* GetData() noexcept { return _data; };
	/** @brief Get length of the mapping. */
	size_t GetLength() noexcept { return _length; };

	/** @brief Delete the memory mapping. */
	void Reset() noexcept;

private:
	void* _data = MAP_FAILED;
	size_t _length = 0;
};

} // namespace replay
