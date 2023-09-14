/**
 * @file
 * @author Michal Sedlak <sedlakm@cesnet.cz>
 * @brief Simple memory buffer
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <pcapplusplus/Layer.h>

#include <cassert>
#include <memory>
#include <type_traits>
#include <vector>

namespace generator {

/**
 * @brief A simple memory buffer class
 */
class Buffer {
public:
	/**
	 * @brief Get the length of the buffer
	 */
	std::size_t Length() const { return _data.size(); }

	/**
	 * @brief Write data at position and expand the buffer if necessary
	 *
	 * @param data  The data to write
	 * @param len  Length of the data
	 * @param pos  Position in the buffer to start writing from
	 *
	 * @throw std::logic_error if pos is out of buffer bounds
	 */
	void Write(const char* data, std::size_t len, std::size_t pos);

	/**
	 * @brief View data at buffer position as a different type
	 *
	 * @param pos The position
	 *
	 * @warning Bound checking is not performed. If pos is out of bounds,
	 *          behavior is undefined.
	 */
	template <typename T>
	T& View(std::size_t pos);

	/**
	 * @brief Append data to the end of the buffer
	 *
	 * @param data The data to append
	 * @param len The length of the data
	 */
	void Append(const char* data, std::size_t len);

	/**
	 * @brief Append a value to the end of the buffer
	 *
	 * @param value The value to append (must be trivially copyable)
	 */
	template <typename T>
	std::enable_if_t<std::is_trivially_copyable_v<T>, void> Append(const T& value);

	/**
	 * @brief Create a pcpp::Layer from the contents of the buffer
	 *
	 * @return The pcpp::Layer object
	 */
	std::unique_ptr<pcpp::Layer> ToLayer() const;

private:
	std::vector<char> _data;
};

template <typename T>
T& Buffer::View(std::size_t pos)
{
	assert(pos + sizeof(T) <= _data.size());
	return *reinterpret_cast<T*>(_data.data() + pos);
}

template <typename T>
std::enable_if_t<std::is_trivially_copyable_v<T>, void> Buffer::Append(const T& value)
{
	Append(reinterpret_cast<const char*>(&value), sizeof(T));
}

} // namespace generator
