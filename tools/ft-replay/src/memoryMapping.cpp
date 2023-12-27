/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Wrapper over memory mapping (i.e. mmap)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "memoryMapping.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace replay {

MemoryMapping::MemoryMapping(void* addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void* ret;

	ret = mmap(addr, length, prot, flags, fd, offset);
	if (ret == MAP_FAILED) {
		std::string errnoCode = std::to_string(errno);
		throw std::runtime_error("mmap() has failed: " + errnoCode);
	}

	_data = ret;
	_length = length;
}

MemoryMapping::~MemoryMapping()
{
	Reset();
}

MemoryMapping::MemoryMapping(MemoryMapping&& other) noexcept
	: _data(other._data)
	, _length(other._length)
{
	other._data = MAP_FAILED;
	other._length = 0;
}

MemoryMapping& MemoryMapping::operator=(MemoryMapping&& other) noexcept
{
	if (this != &other) {
		Reset();

		_data = std::exchange(other._data, MAP_FAILED);
		_length = std::exchange(other._length, 0);
	}

	return *this;
}

void MemoryMapping::Reset() noexcept
{
	if (IsValid()) {
		munmap(_data, _length);
		_data = MAP_FAILED;
		_length = 0;
	}
}

} // namespace replay
