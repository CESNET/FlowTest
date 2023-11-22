/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Simple memory buffer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "buffer.h"

#include <pcapplusplus/PayloadLayer.h>

#include <cassert>
#include <cstring>
#include <numeric>
#include <stdexcept>

namespace generator {

void Buffer::WriteAt(std::size_t pos, const std::byte* data, std::size_t len)
{
	if (pos + len > _data.size()) {
		throw std::logic_error("write would happen out of buffer bounds");
	}

	std::memcpy(_data.data() + pos, data, len);
}

void Buffer::WriteAt(std::size_t pos, const char* data, std::size_t len)
{
	WriteAt(pos, reinterpret_cast<const std::byte*>(data), len);
}

const std::byte* Buffer::PtrAt(std::size_t pos) const
{
	return _data.data() + pos;
}

void Buffer::Append(const std::byte* data, std::size_t len)
{
	std::size_t end = _data.size();
	_data.resize(end + len);
	WriteAt(end, data, len);
}

void Buffer::Append(const char* data, std::size_t len)
{
	Append(reinterpret_cast<const std::byte*>(data), len);
}

std::unique_ptr<pcpp::Layer> Buffer::ToLayer() const
{
	return std::make_unique<pcpp::PayloadLayer>(
		reinterpret_cast<const uint8_t*>(_data.data()),
		_data.size(),
		true);
}

void Buffer::WriteAt(std::size_t pos, std::string_view value)
{
	WriteAt(pos, reinterpret_cast<const std::byte*>(value.data()), value.size());
}

void Buffer::Append(std::string_view value)
{
	Append(reinterpret_cast<const std::byte*>(value.data()), value.size());
}

Buffer Buffer::Concat(std::initializer_list<Buffer> buffers)
{
	std::size_t totalLen = std::accumulate(
		buffers.begin(),
		buffers.end(),
		std::size_t(0),
		[](std::size_t value, const Buffer& buffer) { return value + buffer.Length(); });

	Buffer result;
	result._data.reserve(totalLen);

	for (auto& buffer : buffers) {
		result.Append(buffer.PtrAt(0), buffer.Length());
	}

	return result;
}

std::vector<Buffer> Buffer::Split(std::size_t maxSize) const
{
	assert(maxSize > 0);

	if (Length() <= maxSize) {
		return {*this};
	}

	std::vector<Buffer> chunks;
	std::size_t pos = 0;

	while (pos < Length()) {
		std::size_t chunkSize = std::min(maxSize, Length() - pos);

		Buffer chunk;
		chunk.Append(PtrAt(pos), chunkSize);
		chunks.emplace_back(std::move(chunk));

		pos += chunkSize;
	}

	return chunks;
}

} // namespace generator
