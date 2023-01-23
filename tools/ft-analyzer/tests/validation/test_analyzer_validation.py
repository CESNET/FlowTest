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
from ftanalyzer.reports import ValidationModelReport

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
) -> ValidationModelReport:
    """Run the validation process and return the validation report."""
    normalizer.set_key_fmt(key)
    norm_references = normalizer.normalize_validation(reference)
    norm_flows = normalizer.normalize(flows)
    key_fmt, _ = fields_db.get_key_formats(key)

    return ValidationModel(key_fmt, norm_references).validate(norm_flows, supported, special)


def sum_stats(stats: Dict[str, Dict[str, int]]) -> Tuple[int, ...]:
    """Summarize validation report statistics."""
    counter = collections.Counter()
    for stat in stats.values():
        counter.update(stat)

    return tuple(counter.values())


@pytest.fixture(name="setup", scope="module")
def init_db_normalizer():
    """Create fields database and initialize normalizer."""
    fields = FieldDatabase(os.path.join(BASE_PATH, FIELDS_DB))
    return Setup(Normalizer(fields), fields, SUPPORTED_FIELDS)


def test_basic_ok(setup) -> None:
    """Test basic validation with no errors."""
    key, references, flows = read_test_files("basic-ok.yml", "basic.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (24, 0, 0, 0)


def test_basic_wrong(setup) -> None:
    """Test basic validation where some flows have wrong field values."""
    key, references, flows = read_test_files("basic-wrong.yml", "basic.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("192.168.56.101", "192.168.56.102", 45422, 443, 6)]["errors"]
    for flow, res in flow_errors:
        if flow["packets"] == 6:
            assert res["missing"] == [("tcp_flags", 26), ("tcp_syn_size", 60)]
            assert res["wrong"] == [("ip_version", 420, 4)]

    flow_errors = report["flows"][("192.168.56.102", "192.168.56.101", 443, 45422, 6)]["errors"]
    for flow, res in flow_errors:
        if flow["packets"] == 4:
            assert res["missing"] == [("ttl", 64)]

    assert sum_stats(result.get_stats()) == (19, 3, 2, 0)


def test_basic_wrong_unsupported(setup) -> None:
    """Test basic validation where some flows have wrong field values, but those field values are not supported."""
    key, references, flows = read_test_files("basic-wrong.yml", "basic.yml")

    supported = [elem for elem in setup.supported if elem not in ["ttl", "tcp_flags", "ip_version", "tcp_syn_size"]]
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (8, 0, 0, 0)


def test_basic_unexpected(setup) -> None:
    """Test basic validation with unexpected or missing flows."""
    key, references, flows = read_test_files("basic-miss.yml", "basic.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, SUPPORTED_FIELDS, {})
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("192.168.56.103", "192.168.56.102", 443, 45422, 6)]["unexpected"]
    assert len(flow_errors) == 1

    flow_errors = report["flows"][("192.168.56.101", "192.168.56.102", 45422, 443, 6)]["missing"]
    assert len(flow_errors) == 1

    flow_errors = report["flows"][("192.168.56.102", "192.168.56.101", 443, 45422, 6)]["unexpected"]
    assert len(flow_errors) == 1

    assert sum_stats(result.get_stats()) == (18, 0, 0, 0)


def test_basic_biflow_reverse_ok(setup) -> None:
    """Test basic validation where input is a biflow in but described from opposite direction."""
    key, references, flows = read_test_files("basic-biflow-reverse-ok.yml", "basic.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (24, 0, 0, 0)


def test_custom_key_ok(setup) -> None:
    """Test situation where a custom key is used."""
    key, references, flows = read_test_files("custom-key-ok.yml", "custom-key.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (16, 0, 0, 0)


def test_custom_key_wrong(setup) -> None:
    """Test situation where some flows have wrong field values when a custom key is used."""
    key, references, flows = read_test_files("custom-key-wrong.yml", "custom-key.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("fe80::c001:2ff:fe40:0", "ff02::1:ffe4:0", 58)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 72:
            assert res["missing"] == [("icmp_type_code", 34560)]

    flow_errors = report["flows"][("fe80::c002:3ff:fee4:0", "fe80::c001:2ff:fe40:0", 58)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 72:
            assert res["missing"] == [("icmp_type_code", 34816)]
        if flow["bytes"] == 75:
            assert res["missing"] == [("icmp_type_code", 34560)]
            assert res["wrong"] == [("bytes", 75, 72)]

    flow_errors = report["flows"][("fe80::c001:2ff:fe40:0", "fe80::c002:3ff:fee4:0", 58)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 64:
            assert res["missing"] == [("icmp_type_code", 34816)]
            assert res["wrong"] == [("ip_version", 60, 6)]

    assert sum_stats(result.get_stats()) == (10, 4, 2, 0)


def test_any_direction_both(setup) -> None:
    """Test situation where any-directional fields are present in both flows."""
    key, references, flows = read_test_files("any-direction-both.yml", "any-direction.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (18, 0, 0, 0)


def test_any_direction_single(setup) -> None:
    """Test situation where any-directional fields are present in a single flow."""
    key, references, flows = read_test_files("any-direction-single.yml", "any-direction.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (18, 0, 0, 0)


def test_any_direction_none(setup) -> None:
    """Test situation where any-directional fields are not present in any flow."""
    key, references, flows = read_test_files("any-direction-none.yml", "any-direction.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("192.168.1.140", "174.143.213.184", 57678, 80, 6)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 1234:
            assert res["missing"] == [
                ("http_host", "packetlife.net"),
                ("http_url", "/images/layout/logo.png"),
                ("http_method", "GET"),
                ("http_status_code", 200),
                ("http_agent", "Wget/1.12 (linux-gnu)"),
                ("http_content_type", "image/png"),
            ]

    flow_errors = report["flows"][("174.143.213.184", "192.168.1.140", 80, 57678, 6)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 1234:
            assert res["missing"] == [
                ("http_host", "packetlife.net"),
                ("http_url", "/images/layout/logo.png"),
                ("http_method", "GET"),
                ("http_status_code", 200),
                ("http_agent", "Wget/1.12 (linux-gnu)"),
                ("http_content_type", "image/png"),
            ]

    assert sum_stats(result.get_stats()) == (6, 12, 0, 0)


def test_fixed_direction_ok(setup) -> None:
    """Test situation where fixed direction fields are present in the correct flow."""
    key, references, flows = read_test_files("fixed-direction-ok.yml", "fixed-direction.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (14, 0, 0, 0)


def test_fixed_direction_wrong(setup) -> None:
    """Test situation where fixed direction fields are present in the opposite flow."""
    key, references, flows = read_test_files("fixed-direction-wrong.yml", "fixed-direction.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("192.168.236.146", "192.168.93.24", 60643, 53, 17)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 67:
            assert res["missing"] == [
                ("dns_req_query_type", 1),
                ("dns_req_query_class", 1),
                ("dns_req_query_name", "ia-dns.net"),
            ]
            assert res["wrong"] == [("dns_flags", 33154, 256)]

    flow_errors = report["flows"][("192.168.93.24", "192.168.236.146", 53, 60643, 17)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 67:
            assert res["missing"] == [("dns_resp_rcode", 2)]
            assert res["wrong"] == [("dns_flags", 256, 33154)]

    assert sum_stats(result.get_stats()) == (8, 4, 2, 0)


def test_fixed_direction_biflow_ok(setup) -> None:
    """Test situation where fixed direction fields are all in a biflow."""
    key, references, flows = read_test_files("fixed-direction-biflow-ok.yml", "fixed-direction.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (14, 0, 0, 0)


def test_subfields_one_in_array_ok(setup) -> None:
    """Test situation where subfields are evaluated using OneInArray strategy."""
    key, references, flows = read_test_files("subfields-one-ok.yml", "subfields.yml")
    result = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "OneInArray"}
    )
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (19, 0, 0, 0)


def test_subfields_one_in_array_wrong(setup) -> None:
    """Test situation where incorrect subfields are evaluated using OneInArray strategy."""
    key, references, flows = read_test_files("subfields-one-wrong.yml", "subfields.yml")
    result = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "OneInArray"}
    )
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("192.168.197.92", "192.168.21.89", 53, 40980, 17)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 99:
            assert res["wrong"] == [
                ("dns_resp_rr.name", "unexpected.com", "newportconceptsla.com"),
                ("dns_resp_rr.type", 5, 1),
                ("dns_resp_rr.class", 2, 1),
                ("dns_resp_rr.data", "192.168.215.1", "192.168.73.162"),
            ]

    assert sum_stats(result.get_stats()) == (15, 0, 4, 0)


def test_subfields_full_array_ok(setup) -> None:
    """Test situation where subfields are evaluated using FullArray strategy."""
    key, references, flows = read_test_files("subfields-ok.yml", "subfields.yml")
    result = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "FullArray"}
    )
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (24, 0, 0, 0)


def test_subfields_full_array_wrong(setup) -> None:
    """Test situation where incorrect subfields are evaluated using FullArray strategy."""
    key, references, flows = read_test_files("subfields-wrong.yml", "subfields.yml")
    result = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "FullArray"}
    )
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("192.168.21.89", "192.168.197.92", 40980, 53, 17)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 99:
            assert res["wrong"] == [
                ("dns_resp_rr.name", "unexpected.com", "newportconceptsla.com"),
                ("dns_resp_rr.type", 5, 1),
                ("dns_resp_rr.class", 2, 1),
                ("dns_resp_rr.data", "192.168.215.1", "192.168.73.162"),
                ("dns_resp_rr.class", 2, 1),
            ]

    assert sum_stats(result.get_stats()) == (19, 0, 5, 0)


def test_subfields_missing(setup) -> None:
    """Test situation where some subfields are missing."""
    key, references, flows = read_test_files("subfields-missing.yml", "subfields.yml")
    result = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "FullArray"}
    )
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("192.168.21.89", "192.168.197.92", 40980, 53, 17)]["errors"]
    for flow, res in flow_errors:
        if flow["bytes"] == 99:
            assert res["missing"] == [
                ("dns_resp_rr.name", "newportconceptsla.com"),
                ("dns_resp_rr.type", 1),
                ("dns_resp_rr.class", 1),
                ("dns_resp_rr.ttl", 136),
                ("dns_resp_rr.data", "192.168.73.162"),
            ]

    assert sum_stats(result.get_stats()) == (19, 5, 0, 0)


def test_subfields_unexpected(setup) -> None:
    """Test situation where some subfields are unexpected."""
    key, references, flows = read_test_files("subfields-unexpected.yml", "subfields.yml")
    result = run_validation(
        key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {"dns_resp_rr": "FullArray"}
    )
    assert result.is_passing() is False

    report = result.get_report()
    flow_errors = report["flows"][("192.168.197.92", "192.168.21.89", 53, 40980, 17)]["errors"]
    for _, res in flow_errors:
        assert len(res["unexpected"]) == 1

    assert sum_stats(result.get_stats()) == (24, 0, 0, 1)


def test_multiple_flows_same_key(setup) -> None:
    """Test situation where are lots of flows with the same key."""
    key, references, flows = read_test_files("multi-same-key.yml", "multi-same-key.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (48, 0, 0, 0)


def test_empty_flows(setup) -> None:
    """Test situation where no flow was provided."""
    key, references, flows = read_test_files("empty.yml", "any-direction.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is False

    report = result.get_report()
    flow_missing = report["flows"][("192.168.1.140", "174.143.213.184", 57678, 80, 6)]["missing"]
    assert len(flow_missing) == 1
    flow_missing = report["flows"][("174.143.213.184", "192.168.1.140", 80, 57678, 6)]["missing"]
    assert len(flow_missing) == 1

    assert not sum_stats(result.get_stats())


def test_swapped_fields_single_flow(setup) -> None:
    """Test situation with fields that are reversed, but both always present (e.g., mac)."""

    key, references, flows = read_test_files("mac-single.yml", "mac.yml")
    result = run_validation(key, flows, references, setup.fields_db, setup.normalizer, setup.supported, {})
    assert result.is_passing() is True

    assert sum_stats(result.get_stats()) == (10, 0, 0, 0)
