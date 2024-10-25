"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Tests for CollectorOutputMapper.
"""

import json
import os
import re
import shutil
from os import path
from typing import List, Union

import pytest
from src.collector.interface import CollectorOutputReaderInterface
from src.collector.mapper import (
    CollectorOutputMapper,
    Converters,
    MappingException,
    MappingFileReadException,
    MappingFormatException,
)

CONF_FILE = path.join(path.dirname(path.realpath(__file__)), "../../../../../conf/ipfixcol2/mapping.yaml")
TESTING_TMP_DIR = path.join(path.dirname(path.realpath(__file__)), "tests_tmp")


class ReaderStub(CollectorOutputReaderInterface):
    """Helper class used for serving testing data the same way as collector."""

    def __init__(self, flows: Union[List[str], List[dict]]):  # pylint: disable=super-init-not-called
        """Initialize the collector output reader.

        Parameters
        ----------
        flows : List[str] or List[dict]
            Startup arguments.
        """

        self._index = 0
        if all(isinstance(x, str) for x in flows):
            self._flows = [json.loads(x) for x in flows]
        elif all(isinstance(x, dict) for x in flows):
            self._flows = flows

    def __iter__(self):
        """Basic iterator.

        Returns
        -------
        CollectorOutputReaderInterface
            Object instance.
        """

        self._index = 0
        return self

    def __next__(self):
        """Read next flow entry from collector output.

        Returns
        -------
        dict
            JSON entry in form of dict.

        Raises
        ------
        StopIteration
            No more entries for processing.
        """

        if self._index > len(self._flows):
            raise StopIteration

        i = self._index
        self._index += 1

        return self._flows[i]


class EnsureFile:
    """Helper class providing testing temp file with teardown."""

    def __init__(self, filename: str) -> None:
        self._filename = path.join(TESTING_TMP_DIR, filename)

    def __enter__(self):
        os.makedirs(TESTING_TMP_DIR, exist_ok=True)
        open(self._filename, "w", encoding="utf-8").close()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        shutil.rmtree(TESTING_TMP_DIR)

    def write(self, __s: str) -> int:
        """Append to temp file."""
        with open(self._filename, "a", encoding="utf-8") as file:
            return file.write(__s)

    @property
    def name(self):
        """Get temp file path."""
        return self._filename


def test_mapper_simple():
    """Test mapping of basic attributes."""

    reader = ReaderStub(
        [
            """{
                "iana:octetDeltaCount": 3568720,
                "iana:packetDeltaCount": 2383,
                "iana:ipVersion": 4,
                "iana:sourceIPv4Address": "192.168.0.100",
                "iana:destinationIPv4Address": "192.168.0.101",
                "iana:ipClassOfService": 0,
                "iana:ipTTL": 62,
                "iana:protocolIdentifier": 6,
                "iana:tcpControlBits": 26,
                "iana:sourceTransportPort": 47200,
                "iana:destinationTransportPort": 20810
            }"""
        ]
    )
    mapper = CollectorOutputMapper(reader, CONF_FILE)
    yaml_flow, _, _ = next(iter(mapper))

    assert yaml_flow["bytes"] == 3568720
    assert yaml_flow["packets"] == 2383
    assert yaml_flow["ip_version"] == 4
    assert yaml_flow["src_ip"] == "192.168.0.100"
    assert yaml_flow["dst_ip"] == "192.168.0.101"
    assert yaml_flow["tos"] == 0
    assert yaml_flow["ttl"] == 62
    assert yaml_flow["protocol"] == 6
    assert yaml_flow["tcp_flags"] == 0x1A
    assert yaml_flow["src_port"] == 47200
    assert yaml_flow["dst_port"] == 20810


def test_mapper_nested():
    """Test mapping of nested attributes (dictionary)."""

    reader = ReaderStub(
        [
            """{
                "cesnet:DNSName": "test.cz",
                "cesnet:DNSRData": "abcdefghij",
                "flowmon:dnsCrrName": "example.com",
                "flowmon:dnsCrrType": 16,
                "flowmon:dnsCrrClass": 1,
                "cesnet:DNSRRTTL": 20
            }"""
        ]
    )

    mapper = CollectorOutputMapper(reader, CONF_FILE)
    yaml_flow, _, _ = next(iter(mapper))

    assert yaml_flow["dns_req_query_name"] == "test.cz"
    assert yaml_flow["dns_resp_rr"]["data"] == "abcdefghij"
    assert yaml_flow["dns_resp_rr"]["name"] == "example.com"
    assert yaml_flow["dns_resp_rr"]["type"] == 16
    assert yaml_flow["dns_resp_rr"]["class"] == 1
    assert yaml_flow["dns_resp_rr"]["ttl"] == 20


def test_converter_rstrip_values():
    """Test conversion of mapped values - in this case removal of '\x00' padding from flowmon fields."""

    reader = ReaderStub(
        [
            {
                "flowmon:dnsCrrName": "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:dnsQname": "newportconceptsla.com\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00",
                "flowmon:dnsCrrRdata": "0x676C6173746F6E2E6E65740000000000000000000000000000000000000000000000000000000"
                + "000000000000000000000000000000000000000000000000000",
            },
            {
                "flowmon:tlsAlpn": "http/1.1\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:tlsSni": "ja3er.com\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:tlsIssuerCn": "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:tlsSubjectCn": "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
            },
            {
                "flowmon:httpHost": "httpbin.org\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:httpUrl": "/status/418\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:httpUserAgent": "curl/7.43.0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:httpContentType": "text/html; charset=utf-8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:httpReferer": "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
            },
        ]
    )

    mapper = CollectorOutputMapper(reader, CONF_FILE)
    mapper_it = iter(mapper)
    dns_flow, _, _ = next(mapper_it)

    assert dns_flow["dns_resp_rr"]["name"] == ""
    assert dns_flow["dns_req_query_name"] == "newportconceptsla.com"
    # stripping hexadecimal numbers
    assert dns_flow["dns_resp_rr"]["flowmon_data"] == "0x676C6173746F6E2E6E6574"

    tls_flow, _, _ = next(mapper_it)

    assert tls_flow["tls_alpn"] == "http/1.1"
    assert tls_flow["tls_sni"] == "ja3er.com"
    assert tls_flow["tls_issuer_cn"] == ""
    assert tls_flow["tls_subject_cn"] == ""

    http_flow, _, _ = next(mapper_it)

    assert http_flow["http_host"] == "httpbin.org"
    assert http_flow["http_url"] == "/status/418"
    assert http_flow["http_agent"] == "curl/7.43.0"
    assert http_flow["http_content_type"] == "text/html; charset=utf-8"
    assert http_flow["http_referer"] == ""


def test_empty_flow():
    """Map flow without properties."""

    reader = ReaderStub([{}])

    mapper = CollectorOutputMapper(reader, CONF_FILE)
    flow, mapped_keys, unmapped_keys = next(iter(mapper))

    assert flow == {}
    assert len(mapped_keys) == 0
    assert len(unmapped_keys) == 0


def test_all_keys_mapped():
    """Ensure mapper recognizes known properties and maps them correctly."""

    reader = ReaderStub(
        [
            {
                "iana:tcpControlBits": 26,
                "iana:destinationIPv6Address": "7a72:8876:94d3:06a8:527a:367e:fe47:bb41",
                "iana:ipVersion": 6,
                "cesnet:DNSName": "example.org",
                "flowmon:httpContentType": "text/html; charset=utf-8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                + "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00",
                "flowmon:tlsJa3Fingerprint": "9a7b51089c089491dbc4879218db549c",
                "flowmon:dnsCrrRdata": "676f6f676c652d7075626c69632d646e732d622e676f6f676c652e636f6d",
            }
        ]
    )

    mapper = CollectorOutputMapper(reader, CONF_FILE)
    flow, mapped_keys, unmapped_keys = next(iter(mapper))

    assert len(unmapped_keys) == 0
    assert "iana:tcpControlBits" in mapped_keys
    assert "tcp_flags" in flow
    assert "iana:destinationIPv6Address" in mapped_keys
    assert "dst_ip" in flow
    assert "iana:ipVersion" in mapped_keys
    assert "ip_version" in flow
    assert "cesnet:DNSName" in mapped_keys
    assert "dns_req_query_name" in flow
    assert "flowmon:httpContentType" in mapped_keys
    assert "http_content_type" in flow
    assert "flowmon:tlsJa3Fingerprint" in mapped_keys
    assert "tls_ja3" in flow

    assert "flowmon:dnsCrrRdata" in mapped_keys
    assert "dns_resp_rr" in flow
    assert "flowmon_data" in flow["dns_resp_rr"]


def test_unknown_keys():
    "Ensure mapper recognizes unknown properties and reports them."

    reader = ReaderStub(
        [
            {
                "iana:tcpControlBits": 26,
                "iana:destinationIPv6Address": "7a72:8876:94d3:06a8:527a:367e:fe47:bb41",
                "iana:unknownProperty": 1234,
                "unknown:sourceIPv4Address": "192.168.1.1",
                "unknown:unknown": "abcdef",
                "iana:occtetDeltaCounts": 24,
            }
        ]
    )

    mapper = CollectorOutputMapper(reader, CONF_FILE)
    flow, mapped_keys, unmapped_keys = next(iter(mapper))

    assert "iana:tcpControlBits" in mapped_keys
    assert "iana:tcpControlBits" not in unmapped_keys
    assert "tcp_flags" in flow
    assert "iana:destinationIPv6Address" in mapped_keys
    assert "iana:destinationIPv6Address" not in unmapped_keys
    assert "dst_ip" in flow

    assert "iana:unknownProperty" in unmapped_keys
    assert "iana:unknownProperty" not in mapped_keys
    assert "unknown:sourceIPv4Address" in unmapped_keys
    assert "unknown:sourceIPv4Address" not in mapped_keys
    assert "unknown:unknown" in unmapped_keys
    assert "unknown:unknown" not in mapped_keys
    assert "iana:occtetDeltaCounts" in unmapped_keys
    assert "iana:occtetDeltaCounts" not in mapped_keys

    # in mapped flow are only two known properties
    assert len(flow) == len(mapped_keys) == 2

    # only unknown keys are in unmapped_keys array
    assert len(unmapped_keys) == 4


def test_bad_config_path():
    "Test raise of MappingFileReadException when conf file cannot be open."

    reader = ReaderStub([])

    with pytest.raises(MappingFileReadException):
        CollectorOutputMapper(reader, path.join(TESTING_TMP_DIR, "file-not-found.yaml"))


def test_bad_config_format():
    "Test raise of MappingFormatException when config is not in correct yaml format."

    with EnsureFile("bad_mapping.yaml") as file:
        file.write(
            """
            example:
                key1: val1: val2
            """
        )

        reader = ReaderStub([])

        with pytest.raises(MappingFormatException):
            CollectorOutputMapper(reader, file.name)


def test_config_missing_map_attribute():
    "Test raise of MappingFormatException when map attribute is missing in config file."

    with EnsureFile("bad_mapping.yaml") as file:
        file.write(
            """
            attribute1:
                map: __attribute_1__
            attribute2:
                converter: rstrip_zeroes
            """
        )

        reader = ReaderStub([])

        with pytest.raises(MappingFormatException, match="Missing map attribute for key 'attribute2'."):
            CollectorOutputMapper(reader, file.name)


def test_config_bad_map_type():
    "Test raise of MappingFormatException when map attribute is wrong type (must be string)."

    with EnsureFile("bad_mapping.yaml") as file:
        file.write(
            """
            attribute1:
                map: __attribute_1__
            attribute2:
                map: 526
            """
        )

        reader = ReaderStub([])

        with pytest.raises(
            MappingFormatException, match="Unexpected type of value of 'map' in property 'attribute2' mapping."
        ):
            CollectorOutputMapper(reader, file.name)


def test_config_bad_converter_type():
    "Test raise of MappingFormatException when converter attribute is wrong type (must be string)."

    with EnsureFile("bad_mapping.yaml") as file:
        file.write(
            """
            attribute1:
                map: __attribute_1__
            attribute2:
                map: __attribute_2__
                converter:
                    func: rstrip_zeroes
            """
        )

        reader = ReaderStub([])

        with pytest.raises(
            MappingFormatException, match="Unexpected type of value of 'converter' in property 'attribute2' mapping."
        ):
            CollectorOutputMapper(reader, file.name)


def test_config_unknown_converter():
    "Test raise of MappingFormatException when converter attribute has unrecognized converter function."

    with EnsureFile("bad_mapping.yaml") as file:
        file.write(
            """
            attribute1:
                map: __attribute_1__
            attribute2:
                map: __attribute_2__
                converter: unknown_converter_function
            """
        )

        reader = ReaderStub([])

        with pytest.raises(
            MappingFormatException,
            match="Converter method 'unknown_converter_function' from property 'attribute2' mapping not found.",
        ):
            CollectorOutputMapper(reader, file.name)


def test_config_unexpected_mapping_attribute():
    "Test raise of MappingFormatException when there is unexpected mapping attribute in config file."

    with EnsureFile("bad_mapping.yaml") as file:
        file.write(
            """
            attribute1:
                map: __attribute_1__
            attribute2:
                map: __attribute_2__
                note: Not allowed note...
            """
        )

        reader = ReaderStub([])

        with pytest.raises(
            MappingFormatException,
            match="Unexpected key 'note' in property 'attribute2' mapping. Allowed only 'map' and 'converter'.",
        ):
            CollectorOutputMapper(reader, file.name)


def test_config_mismatch_between_nested_and_simple_attribute():
    """Ensure mapper recognizes mismatch when mixing simple and nested (structured) attributes together."""

    with EnsureFile("mismatch_mapping.yaml") as file:
        file.write(
            """
            attribute1:
                map: dst_attrib
            attribute2:
                map: dst_attrib.a2
            attribute3:
                map: dst_attrib.a3
            """
        )

        reader = ReaderStub(
            [
                {"attribute1": "simple value"},
                {"attribute2": "inner value 2", "attribute3": "inner value 3"},
                {"attribute1": "simple value", "attribute3": "inner value 3"},
            ]
        )
        mapper = CollectorOutputMapper(reader, file.name)

    mapper_it = iter(mapper)
    next(mapper_it)
    next(mapper_it)

    with pytest.raises(
        MappingException, match=str(re.escape("Attribute must be either dict or simple value (key: 'dst_attrib')"))
    ):
        next(mapper_it)


def test_overriding_value():
    "Testing known behaviour: if more fields are mapped to the same attribute, last value is taken."

    reader = ReaderStub(
        [
            """{
                "iana:sourceIPv4Address": "11.11.11.11",
                "iana:sourceIPv6Address": "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"
            }"""
        ]
    )
    mapper = CollectorOutputMapper(reader, CONF_FILE)
    yaml_flow, _, _ = next(iter(mapper))

    assert len(yaml_flow) == 1
    assert yaml_flow["src_ip"] == "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"


def test_builtin_converters():
    "Test converters which are not used in ipfixcol2 mapping."

    with EnsureFile("converters_mapping.yaml") as file:
        file.write(
            """
            'iana:protocolIdentifier':
                map: protocol
                converter: protocol_identifier_to_number
            'iana:tcpControlBits':
                map: tcp_flags
                converter: flags_to_hex
            """
        )

        reader = ReaderStub(
            [
                {"iana:protocolIdentifier": "TCP"},
                {"iana:protocolIdentifier": "UDP"},
                {"iana:protocolIdentifier": "ICMP"},
                {"iana:protocolIdentifier": "NOTEXISTS"},
                {"iana:tcpControlBits": ".AP.S."},
                {"iana:tcpControlBits": ".....F"},
                {"iana:tcpControlBits": ".A..S."},
            ]
        )
        mapper = CollectorOutputMapper(reader, file.name)

    mapper_it = iter(mapper)

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["protocol"] == 6

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["protocol"] == 17

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["protocol"] == 1

    with pytest.raises(MappingException, match=str(re.escape("Unknown protocol with identifier 'NOTEXISTS'."))):
        next(mapper_it)

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tcp_flags"] == 26

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tcp_flags"] == 1

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tcp_flags"] == 18


def test_converter_tls_alg_nid_to_longname():
    """Test conversion of mapped values - in this case translation of tls algorithm nid to full algorithm name."""

    # example nids are taken from https://github.com/openssl/openssl/blob/master/crypto/objects/obj_dat.h#L1158
    reader = ReaderStub(
        [
            {"flowmon:tlsPublicKeyAlg": 0},
            {"flowmon:tlsPublicKeyAlg": 1},
            {"flowmon:tlsPublicKeyAlg": 2},
            {"flowmon:tlsPublicKeyAlg": 1247},
            {"flowmon:tlsPublicKeyAlg": 1119},
            {"flowmon:tlsPublicKeyAlg": 713},
            {"flowmon:tlsPublicKeyAlg": 165},
            {"flowmon:tlsPublicKeyAlg": 50100},
        ]
    )
    mapper = CollectorOutputMapper(reader, CONF_FILE)
    mapper_it = iter(mapper)

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tls_public_key_alg"] == "undefined"

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tls_public_key_alg"] == "RSA Data Security, Inc."

    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tls_public_key_alg"] == "RSA Data Security, Inc. PKCS"

    # last nid supported by pyopenssl in time writing the test
    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tls_public_key_alg"] == "id-ct-signedChecklist"

    # some random nids
    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tls_public_key_alg"] == "RSA-SHA3-512"
    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tls_public_key_alg"] == "secp224r1"
    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tls_public_key_alg"] == "Policy Qualifier User Notice"

    # undefined nid
    yaml_flow, _, _ = next(mapper_it)
    assert yaml_flow["tls_public_key_alg"] == ""


def test_converter_hex_to_int():
    """Test hex_to_int converter."""
    assert Converters.hex_to_int("0x000131") == 305
