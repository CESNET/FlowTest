"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tests for rewriting utils with scapy.
"""

import logging
import tempfile
from os import path

from scapy.utils import rdpcap
from src.generator.scapy_rewriter import RewriteRules, rewrite_pcap

PCAP_DIR = path.dirname(path.realpath(__file__))
PCAP_REF_DIR = path.join(path.dirname(path.realpath(__file__)), "ref")


def compare_ref(pcap: str, ref: str) -> bool:
    """Compare content of two pcaps.

    Parameters
    ----------
    pcap : str
        Path to pcap.
    ref : str
        Path to reference pcap.

    Returns
    -------
    bool
        True if pcaps are the same.
    """

    for packet1, packet2 in zip(rdpcap(pcap), rdpcap(ref)):
        if bytes(packet1) != bytes(packet2):
            logging.getLogger().error("%s != %s", str(packet1), str(packet2))
            return False
    return True


def test_scapy_rewriter_edit_dst_mac():
    """Test of destination mac replace feature."""

    rules = RewriteRules(edit_dst_mac="aa:bb:cc:dd:00:01")

    with tempfile.TemporaryDirectory() as temp_dir:
        pcap_path = rewrite_pcap(
            path.join(PCAP_DIR, "ipv6-routing-header.pcap"), rules, path.join(temp_dir, "out.pcap")
        )
        assert compare_ref(pcap_path, path.join(PCAP_REF_DIR, "ipv6-routing-header-dst-mac.pcap"))
    assert not path.exists(temp_dir)


def test_scapy_rewriter_edit_vlan():
    """Test of add vlan tag feature."""

    rules = RewriteRules(add_vlan=90)

    with tempfile.TemporaryDirectory() as temp_dir:
        pcap_path = rewrite_pcap(
            path.join(PCAP_DIR, "ipv6-routing-header.pcap"), rules, path.join(temp_dir, "out.pcap")
        )
        assert compare_ref(pcap_path, path.join(PCAP_REF_DIR, "ipv6-routing-header-vlan.pcap"))
    assert not path.exists(temp_dir)


def test_scapy_rewriter_edit_dst_mac_add_vlan():
    """Test both add vlan tag and dst mac replace features."""

    rules = RewriteRules(edit_dst_mac="aa:bb:cc:dd:00:01", add_vlan=90)

    with tempfile.TemporaryDirectory() as temp_dir:
        pcap_path = rewrite_pcap(
            path.join(PCAP_DIR, "ipv6-routing-header.pcap"), rules, path.join(temp_dir, "out.pcap")
        )
        assert compare_ref(pcap_path, path.join(PCAP_REF_DIR, "ipv6-routing-header-dst-mac-vlan.pcap"))
    assert not path.exists(temp_dir)
