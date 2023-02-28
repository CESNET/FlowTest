"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

"""
from typing import Any, Dict, List, Optional, Tuple

import collections
import os
import pytest
import yaml

from ftanalyzer.fields import FieldDatabase
from ftanalyzer.normalizer import Normalizer
from ftanalyzer.models import ValidationModel
from ftanalyzer.flow import ValidationField, ValidationStats

BASE_PATH = os.path.dirname(os.path.realpath(__file__))
FLOWS_PATH = os.path.join(BASE_PATH, "flows")
REF_PATH = os.path.join(BASE_PATH, "references")
FIELDS_DB = "fields.yml"
Setup = collections.namedtuple("Setup", ["normalizer", "fields_db", "supported"])

SUPPORTED_FIELDS = [
    "src_ip",
    "dst_ip",
    "src_port",
    "dst_port",
    "src_mac",
    "dst_mac",
    "protocol",
    "ip_version",
    "bytes",
    "packets",
    "tos",
    "ttl",
    "tcp_flags",
    "tcp_syn_size",
    "icmp_type_code",
    "tpc_syn_size",
    "http_host",
    "http_url",
    "http_method",
    "http_status_code",
    "http_agent",
    "http_content_type",
    "http_referer",
    "dns_id",
    "dns_flags",
    "dns_req_query_type",
    "dns_req_query_class",
    "dns_req_query_name",
    "dns_resp_rcode",
    "dns_resp_rr",
    "dns_resp_rr.name",
    "dns_resp_rr.type",
    "dns_resp_rr.class",
    "dns_resp_rr.ttl",
    "dns_resp_rr.data",
]


def read_test_files(
    flows: str, references: str
) -> Tuple[Optional[List[str]], List[Dict[str, Any]], List[Dict[str, Any]]]:
    """Load files with flows and references."""
    with open(os.path.join(REF_PATH, references), "r", encoding="ascii") as ref_stream:
        test_reference = yaml.safe_load(ref_stream)
    with open(os.path.join(FLOWS_PATH, flows), "r", encoding="ascii") as probe_stream:
        probe_data = yaml.safe_load(probe_stream)

    return (
        test_reference.get("key", None),
        test_reference["flows"],
        probe_data["flows"],
    )


def run_validation(
    key: Optional[List[str]],
    flows: List[Dict[str, Any]],
    reference: List[Dict[str, Any]],
    fields_db: FieldDatabase,
    normalizer: Normalizer,
    supported: List[str],
    special: Dict[str, str],
) -> "ValidationReport":
    """Run the validation process and return the validation report."""
    normalizer.set_key_fmt(key)
    norm_references = normalizer.normalize(reference, True)
    norm_flows = normalizer.normalize(flows)
    key_fmt, _ = fields_db.get_key_formats(key)
    assert len(normalizer.pop_skipped_fields()) == 0

    report = ValidationModel(key_fmt, norm_references).validate(norm_flows, supported, special)
    report.print_results()
    report.print_flows_stats()
    report.print_fields_stats()
    return report


@pytest.fixture(name="setup", scope="module")
def init_db_normalizer():
    """Create fields database and initialize normalizer."""
    fields = FieldDatabase(os.path.join(BASE_PATH, FIELDS_DB))
    return Setup(Normalizer(fields), fields, SUPPORTED_FIELDS)


def test_basic_ok(setup) -> None:
    """Test basic validation with no errors."""
    key, references, flows = read_test_files("basic-ok.yml", "basic.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=20)
    assert report.flows_stats == ValidationStats(ok=4)


def test_basic_wrong(setup) -> None:
    """Test basic validation where some flows have wrong field values."""
    key, references, flows = read_test_files("basic-wrong.yml", "basic.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is False

    result = report.get_result_by_key_and_field(("192.168.56.101", "192.168.56.102", 45422, 443, 6), "packets", 6)
    assert ValidationField(name="tcp_flags", expected=26) in result.missing
    assert ValidationField(name="ip_version", expected=4, observed=420) in result.errors

    result = report.get_result_by_key_and_field(("192.168.56.102", "192.168.56.101", 443, 45422, 6), "packets", 4)
    assert ValidationField(name="ttl", expected=64) in result.missing

    assert report.get_fields_summary_stats() == ValidationStats(ok=17, error=1, missing=2)
    assert report.flows_stats == ValidationStats(ok=2, error=2)


def test_basic_wrong_unsupported(setup) -> None:
    """Test basic validation where some flows have wrong field values, but those field values are not supported."""
    key, references, flows = read_test_files("basic-wrong-unsupported.yml", "basic.yml")

    supported = [elem for elem in setup.supported if elem not in ["ttl", "tcp_flags", "ip_version", "tcp_syn_size"]]
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, supported, {})
    assert report.is_passing() is True

    assert report.get_fields_summary_stats() == ValidationStats(ok=8, unchecked=4)
    assert report.flows_stats == ValidationStats(ok=4)


def test_basic_unexpected(setup) -> None:
    """Test basic validation with unexpected or missing flows."""
    key, references, flows = read_test_files("basic-miss.yml", "basic.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, SUPPORTED_FIELDS, {})
    assert report.is_passing() is False
    assert report.get_fields_summary_stats() == ValidationStats(ok=15)
    assert report.flows_stats == ValidationStats(ok=3, missing=1, unexpected=2)


def test_basic_biflow_reverse_ok(setup) -> None:
    """Test basic validation where input is a biflow in but described from opposite direction."""
    key, references, flows = read_test_files("basic-biflow-reverse-ok.yml", "basic.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=20)
    assert report.flows_stats == ValidationStats(ok=4)


def test_custom_key_ok(setup) -> None:
    """Test situation where a custom key is used."""
    key, references, flows = read_test_files("custom-key-ok.yml", "custom-key.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=16)
    assert report.flows_stats == ValidationStats(ok=4)


def test_custom_key_wrong(setup) -> None:
    """Test situation where some flows have wrong field values when a custom key is used."""
    key, references, flows = read_test_files("custom-key-wrong.yml", "custom-key.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is False

    result = report.get_result_by_key_and_field(("fe80::c001:2ff:fe40:0", "ff02::1:ffe4:0", 58), "bytes", 72)
    assert ValidationField(name="icmp_type_code", expected=34560) in result.missing

    result = report.get_result_by_key_and_field(("fe80::c002:3ff:fee4:0", "fe80::c001:2ff:fe40:0", 58), "bytes", 72)
    assert ValidationField(name="icmp_type_code", expected=34816) in result.missing

    result = report.get_result_by_key_and_field(("fe80::c002:3ff:fee4:0", "fe80::c001:2ff:fe40:0", 58), "bytes", 75)
    assert ValidationField(name="icmp_type_code", expected=34560) in result.missing
    assert ValidationField(name="bytes", expected=72, observed=75) in result.errors

    result = report.get_result_by_key_and_field(("fe80::c001:2ff:fe40:0", "fe80::c002:3ff:fee4:0", 58), "bytes", 64)
    assert ValidationField(name="icmp_type_code", expected=34816) in result.missing
    assert ValidationField(name="ip_version", expected=6, observed=60) in result.errors

    assert report.get_fields_summary_stats() == ValidationStats(ok=10, error=2, missing=4)
    assert report.flows_stats == ValidationStats(error=4)


def test_any_direction_both(setup) -> None:
    """Test situation where any-directional fields are present in both flows."""
    key, references, flows = read_test_files("any-direction-both.yml", "any-direction.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=18)
    assert report.flows_stats == ValidationStats(ok=2)


def test_any_direction_single(setup) -> None:
    """Test situation where any-directional fields are present in a single flow."""
    key, references, flows = read_test_files("any-direction-single.yml", "any-direction.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=18)
    assert report.flows_stats == ValidationStats(ok=2)


def test_any_direction_none(setup) -> None:
    """Test situation where any-directional fields are not present in any flow."""
    key, references, flows = read_test_files("any-direction-none.yml", "any-direction.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is False

    result = report.get_result_by_key_and_field(("192.168.1.140", "174.143.213.184", 57678, 80, 6), "bytes", 1234)
    assert ValidationField(name="http_host", expected="packetlife.net") in result.missing
    assert ValidationField(name="http_url", expected="/images/layout/logo.png") in result.missing
    assert ValidationField(name="http_method", expected="GET") in result.missing
    assert ValidationField(name="http_status_code", expected=200) in result.missing
    assert ValidationField(name="http_agent", expected="Wget/1.12 (linux-gnu)") in result.missing
    assert ValidationField(name="http_content_type", expected="image/png") in result.missing

    result = report.get_result_by_key_and_field(("174.143.213.184", "192.168.1.140", 80, 57678, 6), "bytes", 23041)
    assert ValidationField(name="http_host", expected="packetlife.net") in result.missing
    assert ValidationField(name="http_url", expected="/images/layout/logo.png") in result.missing
    assert ValidationField(name="http_method", expected="GET") in result.missing
    assert ValidationField(name="http_status_code", expected=200) in result.missing
    assert ValidationField(name="http_agent", expected="Wget/1.12 (linux-gnu)") in result.missing
    assert ValidationField(name="http_content_type", expected="image/png") in result.missing

    assert report.get_fields_summary_stats() == ValidationStats(ok=6, missing=12)
    assert report.flows_stats == ValidationStats(error=2)


def test_fixed_direction_ok(setup) -> None:
    """Test situation where fixed direction fields are present in the correct flow."""
    key, references, flows = read_test_files("fixed-direction-ok.yml", "fixed-direction.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=14)
    assert report.flows_stats == ValidationStats(ok=2)


def test_fixed_direction_wrong(setup) -> None:
    """Test situation where fixed direction fields are present in the opposite flow."""
    key, references, flows = read_test_files("fixed-direction-wrong.yml", "fixed-direction.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is False

    result = report.get_result_by_key_and_field(("192.168.236.146", "192.168.93.24", 60643, 53, 17), "bytes", 67)
    assert ValidationField(name="dns_req_query_type", expected=1) in result.missing
    assert ValidationField(name="dns_req_query_class", expected=1) in result.missing
    assert ValidationField(name="dns_req_query_name", expected="ia-dns.net") in result.missing
    assert ValidationField(name="dns_flags", expected=256, observed=33154) in result.errors

    result = report.get_result_by_key_and_field(("192.168.93.24", "192.168.236.146", 53, 60643, 17), "bytes", 67)
    assert ValidationField(name="dns_resp_rcode", expected=2) in result.missing
    assert ValidationField(name="dns_flags", expected=33154, observed=256) in result.errors

    assert report.get_fields_summary_stats() == ValidationStats(ok=8, error=2, missing=4, unchecked=4)
    assert report.flows_stats == ValidationStats(error=2)


def test_fixed_direction_biflow_ok(setup) -> None:
    """Test situation where fixed direction fields are all in a biflow."""
    key, references, flows = read_test_files("fixed-direction-biflow-ok.yml", "fixed-direction.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=14)
    assert report.flows_stats == ValidationStats(ok=2)


def test_subfields_one_in_array_ok(setup) -> None:
    """Test situation where subfields are evaluated using OneInArray strategy."""
    key, references, flows = read_test_files("subfields-one-ok.yml", "subfields.yml")
    report = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "OneInArray"}
    )
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=19)
    assert report.flows_stats == ValidationStats(ok=2)


def test_subfields_one_in_array_wrong(setup) -> None:
    """Test situation where incorrect subfields are evaluated using OneInArray strategy."""
    key, references, flows = read_test_files("subfields-one-wrong.yml", "subfields.yml")
    report = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "OneInArray"}
    )
    assert report.is_passing() is False

    result = report.get_result_by_key_and_field(("192.168.197.92", "192.168.21.89", 53, 40980, 17), "bytes", 99)
    assert (
        ValidationField(name="dns_resp_rr.name", expected="newportconceptsla.com", observed="unexpected.com")
        in result.errors
    )
    assert ValidationField(name="dns_resp_rr.type", expected=1, observed=5) in result.errors
    assert ValidationField(name="dns_resp_rr.class", expected=1, observed=2) in result.errors
    assert (
        ValidationField(name="dns_resp_rr.data", expected="192.168.73.162", observed="192.168.215.1") in result.errors
    )

    assert report.get_fields_summary_stats() == ValidationStats(ok=15, error=4)
    assert report.flows_stats == ValidationStats(ok=1, error=1)


def test_subfields_full_array_ok(setup) -> None:
    """Test situation where subfields are evaluated using FullArray strategy."""
    key, references, flows = read_test_files("subfields-ok.yml", "subfields.yml")
    report = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "FullArray"}
    )
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=24)
    assert report.flows_stats == ValidationStats(ok=2)


def test_subfields_full_array_wrong(setup) -> None:
    """Test situation where incorrect subfields are evaluated using FullArray strategy."""
    key, references, flows = read_test_files("subfields-wrong.yml", "subfields.yml")
    report = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "FullArray"}
    )
    assert report.is_passing() is False

    result = report.get_result_by_key_and_field(("192.168.197.92", "192.168.21.89", 53, 40980, 17), "bytes", 99)
    assert (
        ValidationField(name="dns_resp_rr.name", expected="newportconceptsla.com", observed="unexpected.com")
        in result.errors
    )
    assert ValidationField(name="dns_resp_rr.type", expected=1, observed=5) in result.errors
    assert ValidationField(name="dns_resp_rr.class", expected=1, observed=2) in result.errors
    assert (
        ValidationField(name="dns_resp_rr.data", expected="192.168.73.162", observed="192.168.215.1") in result.errors
    )

    assert report.get_fields_summary_stats() == ValidationStats(ok=19, error=5)
    assert report.flows_stats == ValidationStats(ok=1, error=1)


def test_subfields_missing(setup) -> None:
    """Test situation where some subfields are missing."""
    key, references, flows = read_test_files("subfields-missing.yml", "subfields.yml")
    report = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "FullArray"}
    )
    assert report.is_passing() is False

    result = report.get_result_by_key_and_field(("192.168.197.92", "192.168.21.89", 53, 40980, 17), "bytes", 99)
    assert ValidationField(name="dns_resp_rr.name", expected="newportconceptsla.com") in result.missing
    assert ValidationField(name="dns_resp_rr.type", expected=1) in result.missing
    assert ValidationField(name="dns_resp_rr.class", expected=1) in result.missing
    assert ValidationField(name="dns_resp_rr.ttl", expected=136) in result.missing
    assert ValidationField(name="dns_resp_rr.data", expected="192.168.73.162") in result.missing

    assert report.get_fields_summary_stats() == ValidationStats(ok=19, missing=5)
    assert report.flows_stats == ValidationStats(ok=1, error=1)


def test_subfields_unexpected(setup) -> None:
    """Test situation where some subfields are unexpected."""
    key, references, flows = read_test_files("subfields-unexpected.yml", "subfields.yml")
    report = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "FullArray"}
    )
    assert report.is_passing() is False

    unexpected_dns_rdata = {
        "dns_resp_rr.name": "newportconceptsla.com",
        "dns_resp_rr.type": 1,
        "dns_resp_rr.class": 1,
        "dns_resp_rr.ttl": 136,
        "dns_resp_rr.data": "192.168.215.192",
    }
    result = report.get_result_by_key_and_field(("192.168.197.92", "192.168.21.89", 53, 40980, 17), "bytes", 99)
    assert ValidationField(name="dns_resp_rr", observed=unexpected_dns_rdata) in result.unexpected

    assert report.get_fields_summary_stats() == ValidationStats(ok=24, unexpected=1)
    assert report.flows_stats == ValidationStats(ok=1, error=1)


def test_multiple_flows_same_key(setup) -> None:
    """Test situation where are lots of flows with the same key."""
    key, references, flows = read_test_files("multi-same-key.yml", "multi-same-key.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=48)
    assert report.flows_stats == ValidationStats(ok=12)


def test_empty_flows(setup) -> None:
    """Test situation where no flow was provided."""
    key, references, flows = read_test_files("empty.yml", "any-direction.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is False
    assert report.get_fields_summary_stats() == ValidationStats(ok=0)
    assert report.flows_stats == ValidationStats(ok=0, missing=2)


def test_swapped_fields_single_flow(setup) -> None:
    """Test situation with fields that are reversed, but both always present (e.g., mac)."""
    key, references, flows = read_test_files("mac-single.yml", "mac.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=10)
    assert report.flows_stats == ValidationStats(ok=2)


def test_alternative_field_values_correct(setup) -> None:
    """Test situation where a flow field can have multiple valid values."""
    key, references, flows = read_test_files("alternative-ok.yml", "alternative.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=4)
    assert report.flows_stats == ValidationStats(ok=1)


def test_alternative_field_values_wrong(setup) -> None:
    """Test situation where a flow field can have multiple valid values, but the tested is not present."""
    key, references, flows = read_test_files("alternative-wrong.yml", "alternative.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is False

    result = report.get_result_by_key_and_field(("192.168.0.113", "209.31.22.39", 3541, 80, 6), "packets", 6)
    assert (
        ValidationField(
            name="http_content_type", expected=["text/html", "text/html;charset=UTF-8"], observed="image/png"
        )
        in result.errors
    )

    assert report.get_fields_summary_stats() == ValidationStats(ok=3, error=1)
    assert report.flows_stats == ValidationStats(error=1)


def test_single_annotation(setup) -> None:
    """Test scenario when the annotation is done on a single flow direction."""
    key, references, flows = read_test_files("single-annotation.yml", "single-annotation.yml")
    report = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert report.is_passing() is True

    assert report.get_fields_summary_stats() == ValidationStats(ok=5)
    assert report.flows_stats == ValidationStats(ok=1)


def test_skipped_fields(setup) -> None:
    """Test normalizer to identify fields which were not present in the configuration file."""
    key, references, flows = read_test_files("basic-ok.yml", "basic.yml")
    flows[0]["unknown_field_probe"] = "hello"
    references[1]["unknown_field_annotation"] = "world"

    setup.normalizer.set_key_fmt(key)
    norm_references = setup.normalizer.normalize(references, True)
    norm_flows = setup.normalizer.normalize(flows)
    key_fmt, _ = setup.fields_db.get_key_formats(key)
    assert setup.normalizer.pop_skipped_fields() == {"unknown_field_probe", "unknown_field_annotation"}
    assert len(setup.normalizer.pop_skipped_fields()) == 0

    report = ValidationModel(key_fmt, norm_references).validate(norm_flows, setup.supported, {})
    assert report.is_passing() is True
    assert report.get_fields_summary_stats() == ValidationStats(ok=20)
    assert report.flows_stats == ValidationStats(ok=4)
