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
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace generator {

/**
 * @brief An uint16 stored in big endian
 */
struct UInt16Be {
	uint16_t _value; //< The value (big endian)

	/**
	 * @brief Constructor from a uint16_t
	 *
	 * @param value  The value in host endianity
	 */
	UInt16Be(uint16_t value)
		: _value(htobe16(value))
	{
	}
};

/**
 * @brief An uint24 stored in big endian
 */
struct UInt24Be {
	static constexpr unsigned int MAX = (1 << 24) - 1; //< The maximum possible value

	uint8_t _value[3]; //< The value (big endian)

	/**
	 * @brief Constructor from an unsigned int
	 *
	 * @param value  The value in host endianity
	 *
	 * @throws overflow_error when the supplied value cannot fit into 24 bits
	 */
	UInt24Be(unsigned int value)
		: _value {
			  static_cast<uint8_t>((value & 0xff0000) >> 16),
			  static_cast<uint8_t>((value & 0xff00) >> 8),
			  static_cast<uint8_t>(value & 0xff)}
	{
		if (value > UInt24Be::MAX) {
			throw std::overflow_error("value is greater than the maximum possible value");
		}
	}
};

/**
 * @brief A simple memory buffer class
 */
class Buffer {
public:
	static Buffer Concat(std::initializer_list<Buffer> buffers);

	/**
	 * @brief Get the length of the buffer
	 */
	std::size_t Length() const { return _data.size(); }

	/**
	 * @brief Write data at position
	 *
	 * @param pos  Position in the buffer to start writing from
	 * @param data  The data to write
	 * @param len  Length of the data
	 *
	 * @throw std::logic_error if pos is out of buffer bounds
	 */
	void WriteAt(std::size_t pos, const std::byte* data, std::size_t len);

	/**
	 * @brief Write data at position
	 *
	 * @param pos  Position in the buffer to start writing from
	 * @param data  The data to write
	 * @param len  Length of the data
	 *
	 * @throw std::logic_error if pos is out of buffer bounds
	 */
	void WriteAt(std::size_t pos, const char* data, std::size_t len);

	/**
	 * @brief Write trivially copiable value at position
	 *
	 * @param pos  Position in the buffer to start writing from
	 * @param value  The value to write
	 *
	 * @throw std::logic_error if pos is out of buffer bounds
	 */
	template <typename T>
	std::enable_if_t<std::is_trivially_copyable_v<T>, void>
	WriteAt(std::size_t pos, const T& value);

	/**
	 * @brief Write vector of trivially copiable values at position
	 *
	 * @param pos  Position in the buffer to start writing from
	 * @param value  The values to write
	 *
	 * @throw std::logic_error if pos is out of buffer bounds
	 */
	template <typename T>
	std::enable_if_t<std::is_trivially_copyable_v<T>, void>
	WriteAt(std::size_t pos, const std::vector<T>& value);

	/**
	 * @brief Write string at position
	 *
	 * @param pos  Position in the buffer to start writing from
	 * @param value  The string
	 *
	 * @throw std::logic_error if pos is out of buffer bounds
	 */
	void WriteAt(std::size_t pos, std::string_view value);

	/**
	 * @brief Get pointer to data at buffer position
	 *
	 * @param pos The position
	 *
	 * @warning Bound checking is not performed. If pos is out of bounds,
	 *          behavior is undefined.
	 */
	const std::byte* PtrAt(std::size_t pos) const;

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
	void Append(const std::byte* data, std::size_t len);

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
	 * @brief Append a value to the end of the buffer
	 *
	 * @param value The value to append (vector of trivially copiable values)
	 */
	template <typename T>
	std::enable_if_t<std::is_trivially_copyable_v<T>, void> Append(const std::vector<T>& value);

	/**
	 * @brief Append string value to the end of the buffer
	 *
	 * @param value The value to append
	 */
	void Append(std::string_view value);

	/**
	 * @brief Create a pcpp::Layer from the contents of the buffer
	 *
	 * @return The pcpp::Layer object
	 */
	std::unique_ptr<pcpp::Layer> ToLayer() const;

	/**
	 * @brief Split the buffer into chunks
	 *
	 * @param maxSize The maximum size of one chunk
	 *
	 * @return The chunks
	 */
	std::vector<Buffer> Split(std::size_t maxSize) const;

private:
	std::vector<std::byte> _data;
};

template <typename T>
T& Buffer::View(std::size_t pos)
{
	assert(pos + sizeof(T) <= _data.size());
	return *reinterpret_cast<T*>(_data.data() + pos);
}

template <typename T>
std::enable_if_t<std::is_trivially_copyable_v<T>, void>
Buffer::WriteAt(std::size_t pos, const T& value)
{
	WriteAt(pos, reinterpret_cast<const std::byte*>(&value), sizeof(T));
}

template <typename T>
std::enable_if_t<std::is_trivially_copyable_v<T>, void>
Buffer::WriteAt(std::size_t pos, const std::vector<T>& value)
{
	WriteAt(pos, reinterpret_cast<const char*>(value.data()), sizeof(T) * value.size());
}

template <typename T>
std::enable_if_t<std::is_trivially_copyable_v<T>, void> Buffer::Append(const T& value)
{
	Append(reinterpret_cast<const std::byte*>(&value), sizeof(T));
}

template <typename T>
std::enable_if_t<std::is_trivially_copyable_v<T>, void> Buffer::Append(const std::vector<T>& value)
{
	Append(reinterpret_cast<const std::byte*>(value.data()), sizeof(T) * value.size());
}

} // namespace generator
