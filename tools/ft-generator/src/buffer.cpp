/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Simple memory buffer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "buffer.h"

#include <pcapplusplus/PayloadLayer.h>

#include <cstring>
#include <stdexcept>

namespace generator {

void Buffer::Write(const char* data, std::size_t len, std::size_t pos)
{
	if (pos > _data.size()) {
		throw std::logic_error("buffer position is out of bounds");
	}

	if (pos + len > _data.size()) {
		_data.resize(pos + len);
	}
	std::memcpy(_data.data() + pos, data, len);
}

void Buffer::Append(const char* data, std::size_t len)
{
	Write(data, len, _data.size());
}

std::unique_ptr<pcpp::Layer> Buffer::ToLayer() const
{
	return std::make_unique<pcpp::PayloadLayer>(
		reinterpret_cast<const uint8_t*>(_data.data()),
		_data.size(),
		true);
}

} // namespace generator
