"""
Author(s):  Dominik Tran <tran@cesnet.cz>
            Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Execute validation tests and report results.

Test executes following steps:
- Read "flowtest/testing/validation" directory to obtain tests,
- Read configuration files and get generator, probe and collector configuration.
- Launch tested probe.
- Send test-defined packets to probe via generator of type PcapPlayer.
- Stop probe to flush all flows from cache.
- Stop collector.
- Read flows received by collector and remap them.
- Compare received flows to reference flows.
- If flows match, report success. Otherwise report failure.
"""

import logging
import os
from typing import List, Optional

import pytest
import yaml
from ftanalyzer.fields import FieldDatabase
from ftanalyzer.models import ValidationModel
from ftanalyzer.normalizer import Normalizer
from ftanalyzer.reports.validation_model_report import ValidationModelReport
from lbr_testsuite.topology.topology import select_topologies
from scapy.utils import rdpcap
from src.collector.collector_builder import CollectorBuilder
from src.collector.interface import CollectorInterface
from src.collector.mapper import CollectorOutputMapper
from src.common.get_project_root import get_project_root
from src.generator.generator_builder import GeneratorBuilder
from src.generator.interface import PcapPlayer, PpsSpeed
from src.probe.interface import ProbeInterface
from src.probe.probe_builder import ProbeBuilder

PROJECT_ROOT = get_project_root()
VALIDATION_TESTS_DIR = os.path.join(PROJECT_ROOT, "testing/validation")
PCAP_DIR = os.path.join(PROJECT_ROOT, "pcap")
MAPPER_CONF = os.path.join(PROJECT_ROOT, "conf/ipfixcol2/mapping.yaml")
FIELD_DATABASE_CONF = os.path.join(PROJECT_ROOT, "conf/fields.yml")


def collect_validation_tests() -> List[str]:
    """Collect validation tests."""

    files = os.listdir(VALIDATION_TESTS_DIR)
    files = [f for f in files if ".yml" in f]

    tests = []
    for file in files:
        with open(f"{VALIDATION_TESTS_DIR}/{file}", "r", encoding="utf-8") as test_file:
            validation_test = yaml.safe_load(test_file)
            marks = [getattr(pytest.mark, prot) for prot in validation_test["test"]["required_protocols"]]
            tests.append(pytest.param(validation_test, marks=marks, id=file))

    return tests


def receive_flows(collector: CollectorInterface) -> List[dict]:
    """Receive flows from collector and remap items according to mapping configuration.

    Parameters
    ----------
    collector: CollectorInterface
        Collector to be stopped.

    Returns
    -------
    list
        List of remapped flows.
    """

    mapper = CollectorOutputMapper(collector.get_reader(), MAPPER_CONF)
    # mapped_keys (flow[1]) and unmapped_keys (flow[2]) will be inserted into ValidationModelReport in future
    return [flow[0] for flow in mapper]


def validate_flows(validation_test: dict, probe: ProbeInterface, received_flows: List[dict]) -> ValidationModelReport:
    """Validate received flows with reference flows.

    Parameters
    ----------
    validation_test: dict
        Test configuration.
    probe: ProbeInterface
        Probe object.
    received_flows: list
        List of received flows that will be validated.

    Returns
    -------
    ValidationModelReport
        Validation report.
    """

    ref_flows = validation_test.get("flows", None)
    supported_fields = probe.supported_fields()
    special_fields = probe.get_special_fields()

    fdb = FieldDatabase(FIELD_DATABASE_CONF)
    norm = Normalizer(fdb)
    key = validation_test.get("key", fdb.get_key_formats()[0])
    norm.set_key_fmt(key)

    try:
        ref_flows = norm.normalize_validation(ref_flows)
        received_flows = norm.normalize(received_flows)

    # Normalization can raise some exceptions
    # Log these exceptions to output so it's clear what caused it
    # This can be removed once all problems are fixed
    except (KeyError, ValueError) as err:
        logging.error("Error in flow normalization: %s", err)
        logging.error(received_flows)
        raise

    val_model = ValidationModel(key, ref_flows)
    report = val_model.validate(received_flows, supported_fields, special_fields)

    return report


def check_generator_stats(generator_instance: PcapPlayer, pcap_file: str, vlan: Optional[int]):
    """Ensure generator sends expected number of packets and bytes.

    Parameters
    ----------
    generator_instance: PcapPlayer
        tcpreplay that generated traffic.
    pcap_file: str
        Path to PCAP file that was generated.
    vlan: int, optional
        Vlan tag if added to sent packets.
    """

    pcap_stats = rdpcap(os.path.join(PCAP_DIR, pcap_file))
    pcap_packets = len(pcap_stats)
    pcap_bytes = sum(len(pkt) for pkt in pcap_stats)

    stats = generator_instance.stats()
    assert stats.packets == pcap_packets
    # tcpreplay includes Ethernet FSC field in bytes stats, Scapy does not
    expected_bytes = pcap_bytes
    if vlan is not None:
        expected_bytes += 4 * pcap_packets
    assert stats.bytes == expected_bytes


def stop_components(probe: ProbeInterface, collector: CollectorInterface):
    """Stop probe and collector to ensure data are flushed from cache.

    Parameters
    ----------
    probe: ProbeInterface
        Probe to be stopped.
    collector: CollectorInterface
        Collector to be stopped.
    """

    logging.info("\t- Stop probe.")
    probe.stop()

    logging.info("\t- Stop collector.")
    collector.stop()


select_topologies(["pcap_player"])


@pytest.mark.validation
@pytest.mark.parametrize(
    "test",
    collect_validation_tests(),
)
# pylint: disable=too-many-locals
def test_validation(
    request: pytest.FixtureRequest,
    test: dict,
    generator: GeneratorBuilder,
    device: ProbeBuilder,
    analyzer: CollectorBuilder,
):
    """Execute validation tests."""

    objects_to_cleanup = []

    def cleanup():
        for obj in objects_to_cleanup:
            obj.stop()
            obj.cleanup()

    request.addfinalizer(cleanup)

    # every validation test uses only 1 pcap
    pcap_file = test["test"]["pcaps"][0]
    required_protocols = test["test"]["required_protocols"]

    logging.info("\t- Starting collector...")
    collector_instance = analyzer.get()
    objects_to_cleanup.append(collector_instance)
    collector_instance.start()

    logging.info("\t- Starting probe...")
    probe_instance = device.get(required_protocols)
    objects_to_cleanup.append(probe_instance)
    probe_instance.start()

    logging.info("\t- Sending packets via generator to probe...")
    generator_instance = generator.get()
    generator_instance.start(os.path.join(PCAP_DIR, pcap_file), PpsSpeed(10))

    check_generator_stats(generator_instance, pcap_file, generator.get_vlan())
    stop_components(probe_instance, collector_instance)

    received_flows = receive_flows(collector_instance)
    report = validate_flows(test, probe_instance, received_flows)

    print("")
    report.print_report()

    if not report.is_passing():
        assert False, f"Validation of test {request.node.name} failed"
