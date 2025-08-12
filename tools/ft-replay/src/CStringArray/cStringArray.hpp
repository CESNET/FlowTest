/**
 * @file
 * @author Samuel Luptak <xluptas00@stud.fit.vutbr.cz>
 * @brief CStringArray class header (used in dpdk plugins)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

/**
 * class CStringArray is used to convert
 * C++ strings (std::string) into C strings (char *)
 * and store them.
 */

class CStringArray {
public:
	/**
	 * @brief Converts std::string into char * and saves the result
	 *
	 * @param[in] s std::string that gets converted into char *
	 *
	 */
	void Push(const std::string& s);

	/**
	 * @brief Returns the stored C strings
	 *
	 * @return Array of C strings
	 */
	char** GetData();

	/**
	 * @brief Returns the number of stored C strings
	 *
	 * @return Number of stored C strings
	 */
	int GetSize();

private:
	std::vector<std::unique_ptr<char[]>> _storage;
	std::vector<char*> _chars;
};
