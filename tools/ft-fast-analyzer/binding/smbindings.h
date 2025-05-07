/**
 * @file
 * @author Jan Sobol <sobol@cesnet.cz>
 * @brief Python bindings for statistical model and helper structures.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "statisticalmodel.h"

#include <memory>
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <vector>

namespace py = pybind11;

namespace ft::analyzer {

static inline void SMBindings(const py::handle& m)
{
	py::enum_<SMMetricType>(m, "SMMetricType")
		.value("PACKETS", SMMetricType::PACKETS)
		.value("BYTES", SMMetricType::BYTES)
		.value("FLOWS", SMMetricType::FLOWS);

	py::class_<SMMetric>(m, "SMMetric")
		.def(py::init<>())
		.def_readwrite("key", &SMMetric::key)
		.def_readwrite("diff", &SMMetric::diff);

	py::class_<SMSegment, std::shared_ptr<SMSegment>>(m, "SMSegment");
	py::class_<SMSubnetSegment, SMSegment, std::shared_ptr<SMSubnetSegment>>(m, "SMSubnetSegment")
		.def(py::init<>())
		.def_readwrite("source", &SMSubnetSegment::source)
		.def_readwrite("dest", &SMSubnetSegment::dest)
		.def_readwrite("bidir", &SMSubnetSegment::bidir);

	py::class_<SMTimeSegment, SMSegment, std::shared_ptr<SMTimeSegment>>(m, "SMTimeSegment")
		.def(py::init<>())
		.def_readwrite("start", &SMTimeSegment::start)
		.def_readwrite("end", &SMTimeSegment::end);

	py::class_<SMAllSegment, SMSegment, std::shared_ptr<SMAllSegment>>(m, "SMAllSegment")
		.def(py::init<>());

	py::class_<SMComplementSegment, SMSegment, std::shared_ptr<SMComplementSegment>>(
		m,
		"SMComplementSegment")
		.def(py::init<>());

	py::class_<SMTestOutcome>(m, "SMTestOutcome")
		.def(py::init<>())
		.def_readwrite("metric", &SMTestOutcome::metric)
		.def_readwrite("segment", &SMTestOutcome::segment)
		.def_readwrite("value", &SMTestOutcome::value)
		.def_readwrite("reference", &SMTestOutcome::reference)
		.def_readwrite("diff", &SMTestOutcome::diff);

	py::class_<SMRule>(m, "SMRule")
		.def(py::init<const std::vector<SMMetric>&>())
		.def(py::init<const std::vector<SMMetric>&, const SMSubnetSegment&>())
		.def(py::init<const std::vector<SMMetric>&, const SMTimeSegment&>())
		.def_readwrite("metrics", &SMRule::metrics)
		.def_readwrite("segment", &SMRule::segment);

	py::class_<StatisticalReport>(m, "StatisticalReport")
		.def(py::init<>())
		.def_readwrite("tests", &StatisticalReport::tests);

	py::class_<StatisticalModel>(m, "StatisticalModel")
		.def(py::init<std::string_view, std::string_view, uint64_t>())
		.def("Validate", &StatisticalModel::Validate);
}

} // namespace ft::analyzer
