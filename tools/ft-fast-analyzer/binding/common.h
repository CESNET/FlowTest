/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Python bindings for common structures used in the analyzer.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "flow.h"
#include "flowtable.h"
#include "ipaddr.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace ft::analyzer {

static inline void Common(const py::module& m)
{
	py::class_<IPNetwork>(m, "IPNetwork")
		.def(py::init<const std::string&, uint8_t>())
		.def(py::init<const std::string&>())
		.def("__str__", &IPNetwork::ToString);

	py::class_<IPAddr>(m, "IPAddr")
		.def(py::init<const std::string&>())
		.def("__str__", &IPAddr::ToString);
}

} // namespace ft::analyzer
