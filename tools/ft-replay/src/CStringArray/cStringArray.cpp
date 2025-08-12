/**
 * @file
 * @author Samuel Luptak <xluptas00@stud.fit.vutbr.cz>
 * @brief CStringArray class implementation (used in dpdk plugins)
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cStringArray.hpp"
#include <algorithm>
#include <memory.h>
#include <memory>
#include <string>
#include <vector>

void CStringArray::Push(const std::string& s)
{
	std::unique_ptr<char[]> chArray = std::make_unique<char[]>(s.size() + 1);
	char* argPtr = chArray.get();
	std::copy_n(s.c_str(), s.size() + 1, argPtr);

	_storage.emplace_back(std::move(chArray));
	_chars.push_back(argPtr);
}

char** CStringArray::GetData()
{
	return _chars.data();
}

int CStringArray::GetSize()
{
	return _chars.size();
}
