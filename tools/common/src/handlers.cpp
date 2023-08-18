/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Auxiliary logger functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "handlers.h"

#include <stdexcept>
#include <string>

namespace ft {

void CliHandleInvalidOption(std::string_view opt)
{
	std::string err = "Invalid command line option '" + std::string(opt) + "'";
	throw std::invalid_argument(err);
}

void CliHandleMissingArgument(std::string_view opt)
{
	std::string err = "Missing required argument of '" + std::string(opt) + "'";
	throw std::invalid_argument(err);
}

void CliHandleUnimplementedOption(std::string_view opt)
{
	std::string err = "Unimplemented option '" + std::string(opt) + "'";
	throw std::runtime_error(err);
}

} // namespace ft
