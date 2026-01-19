/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Python bindings for ft-fast-analyzer library.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "common.h"
#include "smbindings.h"

#include <memory>
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

namespace ft::analyzer {

PYBIND11_MODULE(ft_fast_analyzer, m)
{
	m.doc() = R"pbdoc(
		 Flowtest Fast Analyzer
		 -----------------------
		 Python binding for ft-fast-analyzer library. Fast implementation
		 of statistical and precise flow analysis.
	 )pbdoc";

	Common(m);
	SMBindings(m);

#ifdef VERSION_INFO
	m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
	m.attr("__version__") = "dev";
#endif
}

} // namespace ft::analyzer
