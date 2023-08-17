/**
 * @file
 * @author Lukas Hutak <hutak@cesnet.cz>
 * @brief Auxiliary handler functions
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <string_view>

namespace ft {

/**
 * @brief Throw a properly formatted exception about invalid option.
 * @param[in] arg Option name
 * @throw std::invalid_argument
 */
[[noreturn]] void CliHandleInvalidOption(std::string_view opt);

/**
 * @brief Throw a properly formatted exception about missing required argument.
 * @param[in] arg Option name
 * @throw std::invalid_argument
 */
[[noreturn]] void CliHandleMissingArgument(std::string_view opt);

/**
 * @brief Throw a properly formatted exception about unimplemented option.
 * @param[in] arg Argument name
 * @throw std::runtime_error
 */
[[noreturn]] void CliHandleUnimplementedOption(std::string_view opt);

} // namespace ft
