"""
Author(s):  Jakub Rimek <jakub.rimek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Unit test of the orchestration configuration reader"""

import os
from typing import Dict

import pytest
from src.config.authentication import AuthenticationCfg
from src.config.collector import CollectorCfg
from src.config.common import InterfaceCfg
from src.config.config import Config, ConfigException
from src.config.generator import GeneratorCfg
from src.config.probe import ProbeCfg
from src.config.whitelist import WhitelistCfg

FILES_DIR = f"{os.getcwd()}/tools/ft-orchestration/tests/dev/config/files"


@pytest.fixture(name="create_config")
def fixture_create_config():
    """Tests the orchestration config reader"""
    return Config(
        f"{FILES_DIR}/authentication.yml",
        f"{FILES_DIR}/generators.yml",
        f"{FILES_DIR}/collectors.yml",
        f"{FILES_DIR}/probes.yml",
        f"{FILES_DIR}/whitelists.yml",
    )


def test_generators(create_config) -> None:
    """Checks the Generators config reader

    Example data:
        - alias: sw-gen-10G
            name: cesnet-generator-1.liberouter.org
            type: TcpReplay
            Interfaces:
        - name: eth2
            speed: 10
        - name: eth3
            speed: 10
            authentication: cesnet-general
            ansible-playbook-role: generator-software

        - alias: hw-gen-100G
            name: cesnet-generator-1.liberouter.org
            type: Replicator
        interfaces:
            - name: eth4
        speed: 100
            - name: eth5
        speed: 100
        authentication: cesnet-general
    """

    generators: Dict[str, GeneratorCfg] = create_config.generators

    assert len(generators) == 2
    assert list(generators.keys()) == ["sw-gen-10G", "hw-gen-100G"]
    gen_1: GeneratorCfg = generators["sw-gen-10G"]
    gen_2: GeneratorCfg = generators["hw-gen-100G"]

    # check gen_1
    assert gen_1.alias == "sw-gen-10G"
    assert gen_1.name == "cesnet-generator-1.liberouter.org"
    assert gen_1.type == "TcpReplay"
    assert gen_1.interfaces == [InterfaceCfg(name="eth2", speed=10), InterfaceCfg(name="eth3", speed=10)]
    assert gen_1.authentication == "cesnet-general"
    assert gen_1.ansible_playbook_role == "generator-software"

    # check gen_2
    # optional ansible-playbook-role
    assert gen_2.ansible_playbook_role is None


def test_generators_invalid() -> None:
    """Checks invalid Generators config reader"""

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators-invalid-1.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert "exception Failure parsing field `interfaces` in class `GeneratorCfg`" in str(err)

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators-invalid-2.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert "exception Failure parsing field `interfaces` in class `GeneratorCfg`. Expected a type" in str(err)


def test_authentications(create_config) -> None:
    """Checks the Authentications config reader

    Example data:
        - name: cesnet-general
            key-path: certs/cesnet-general.pem

        - name: flowmon-probes
            username: slowmon
            password: manyStars
    """

    authentications: Dict[str, AuthenticationCfg] = create_config.authentications

    assert len(authentications) == 3
    assert list(authentications.keys()) == ["cesnet-general", "flowmon-probes", "cesnet-agent"]
    auth_1: AuthenticationCfg = authentications["cesnet-general"]
    auth_2: AuthenticationCfg = authentications["flowmon-probes"]
    auth_3: AuthenticationCfg = authentications["cesnet-agent"]

    assert auth_1.key_path == "tools/ft-orchestration/tests/dev/config/files/cesnet-general.pem"
    assert auth_1.username is None
    assert auth_1.password is None

    assert auth_2.key_path is None
    assert auth_2.username == "slowmon"
    assert auth_2.password == "manyStars"

    assert auth_3.username == "xcesne00"
    assert auth_3.ssh_agent
    assert auth_3.key_path is None
    assert auth_3.password is None


def test_authentications_invalid() -> None:
    """Checks invalid Authentications config reader"""

    try:
        Config(
            f"{FILES_DIR}/authentication-invalid-1.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert str(err) == (
            "Validation error: AuthenticationCfg config file can not contain both of the key_path and the password."
        )

    try:
        Config(
            f"{FILES_DIR}/authentication-invalid-2.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert (
            str(err)
            == "Validation error: At least one authentication (SSH agent, key_path or password) must be present."
        )

    try:
        Config(
            f"{FILES_DIR}/authentication-invalid-3.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert str(err) == "Validation error: Key path or password cannot be set if the ssh agent is used."


def test_collectors(create_config) -> None:
    """Checks the Collectors config reader

    Example data:
        - alias: ipfixcol-1
            name: cesnet-collector-1.liberouter.org
            type: Ipfixcol2
            authentication: cesnet-general

        - alias: flowsen-1
            name: cesnet-collector-1.liberouter.org
            type: Flowsen
            authentication: flowmon-probes
            ansible-playbook-role: collector-flowsen
    """

    collectors: Dict[str, CollectorCfg] = create_config.collectors

    assert len(collectors) == 2
    assert list(collectors.keys()) == ["ipfixcol-1", "flowsen-1"]
    coll_1: CollectorCfg = collectors["ipfixcol-1"]
    coll_2: CollectorCfg = collectors["flowsen-1"]

    assert coll_1.alias == "ipfixcol-1"
    assert coll_1.name == "cesnet-collector-1.liberouter.org"
    assert coll_1.type == "Ipfixcol2"
    assert coll_1.authentication == "cesnet-general"
    assert coll_1.ansible_playbook_role is None

    assert coll_2.alias == "flowsen-1"
    assert coll_2.name == "cesnet-collector-1.liberouter.org"
    assert coll_2.type == "Flowsen"
    assert coll_2.authentication == "flowmon-probes"
    assert coll_2.ansible_playbook_role == "collector-flowsen"


def test_collectors_invalid() -> None:
    """Checks invalid Collectors config reader"""

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors-invalid-1.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert (
            "exception Failure calling constructor method of class `CollectorCfg`. Missing values for required "
            "dataclass fields."
        ) in str(err)

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors-invalid-2.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert str(err) == (
            "Validation error: CollectorCfg config error: AuthenticationCfg name cesnet-general-invalid was not found "
            "in the Authentications config file"
        )


def test_probes(create_config) -> None:
    """Checks the Probes config reader

    Example data:
        - alias: ipfix-probe-10G
            name: cesnet-probe-1.liberouter.org
            type: IpfixprobeRaw
            interfaces:
                - name: eth2
                    speed: 10
                    mac: 00:00:00:00:00:00
                - name: eth3
                    speed: 10
                    mac: 01:01:01:01:01:01
            authentication: cesnet-general
            protocols: [ http, tls, dns, trill ]
            ansible-playbook-role: probe-ipfixprobe

        - alias: flowmonexp-10G
            name: cesnet-probe-2.liberouter.org
            type: FlowmonProbe
            interfaces:
                - name: eth2
                    speed: 10
                    mac: 00:00:00:00:00:00
                - name: eth3
                    speed: 10
                    mac: 01:01:01:01:01:01
            authentication: flowmon-probes
            protocols: [ http, tls, dns, vxlan, gre ]
            connector:
                input-plugin-type: [ dpdk, rawnetcap ]
    """

    probes: Dict[str, ProbeCfg] = create_config.probes

    assert len(probes) == 2
    assert list(probes.keys()) == ["ipfix-probe-10G", "flowmonexp-10G"]
    probe_1: ProbeCfg = probes["ipfix-probe-10G"]
    probe_2: ProbeCfg = probes["flowmonexp-10G"]

    assert probe_1.alias == "ipfix-probe-10G"
    assert probe_1.name == "cesnet-probe-1.liberouter.org"
    assert probe_1.type == "IpfixprobeRaw"
    assert probe_1.interfaces == [
        InterfaceCfg(name="eth2", speed=10, mac="00:00:00:00:00:00"),
        InterfaceCfg(name="eth3", speed=10, mac="01:01:01:01:01:01"),
    ]
    assert probe_1.authentication == "cesnet-general"
    assert probe_1.protocols == ["http", "tls", "dns", "trill"]
    assert probe_1.connector is None
    assert probe_1.ansible_playbook_role == "probe-ipfixprobe"

    assert probe_2.alias == "flowmonexp-10G"
    assert probe_2.name == "cesnet-probe-2.liberouter.org"
    assert probe_2.type == "FlowmonProbe"
    assert probe_2.interfaces == [
        InterfaceCfg(name="eth2", speed=10, mac="00:00:00:00:00:00"),
        InterfaceCfg(name="eth3", speed=10, mac="01:01:01:01:01:01"),
    ]
    assert probe_2.authentication == "flowmon-probes"
    assert probe_2.protocols == ["http", "tls", "dns", "vxlan", "gre"]
    assert probe_2.connector == {"input-plugin-type": ["dpdk", "rawnetcap"]}
    assert probe_2.ansible_playbook_role is None


def test_probes_invalid() -> None:
    """Checks invalid Probes config reader"""
    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes-invalid-1.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert (
            "exception Failure calling constructor method of class `InterfaceCfg`. Missing values for required "
            "dataclass fields."
        ) in str(err)

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes-invalid-2.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert (
            "exception Failure calling constructor method of class `ProbeCfg`. Missing values for required "
            "dataclass fields."
        ) in str(err)

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes-invalid-3.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert "Validation error: Mac address cannot be empty in probe interface" in str(err)

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes-invalid-4.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert "Validation error: Mac address cannot be empty in probe interface" in str(err)

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes-invalid-bad-whitelist.yml",
            f"{FILES_DIR}/whitelists.yml",
        )
        assert False
    except ConfigException as err:
        assert "Validation error: Whitelist 'not-existing-whitelist' was not found in whitelists config" in str(err)


def test_file_not_found() -> None:
    """Checks when the config files are not found"""

    try:
        Config(
            "file_not_found.yml", "file_not_found.yml", "file_not_found.yml", "file_not_found.yml", "file_not_found.yml"
        )
        assert False
    except ConfigException as err:
        assert str(err) == (
            "Error reading config file file_not_found.yml, code 2, exception [Errno 2] No such file or "
            "directory: 'file_not_found.yml'"
        )


def test_whitelists(create_config) -> None:
    """Check reading whitelists

    Example data:
        - name: flowmon-whitelist
            items:
                validation:
                    - test1.yml
                    - test2.yml
                    - test3.yml
                simulation:
                    - test4.yml
                    - test5.yml

        - name: flowmon-whitelist-via-switch
            include: flowmon-whitelist
            items:
                validation:
                    - vlan.yml
                    - mac.yml: "Mac addresses are changed when testing via switch."
    """

    whitelists: Dict[str, WhitelistCfg] = create_config.whitelists

    assert len(whitelists) == 3
    assert list(whitelists.keys()) == ["flowmon-whitelist", "flowmon-whitelist-via-switch", "inherited"]
    wl_1: WhitelistCfg = whitelists["flowmon-whitelist"]
    wl_2: WhitelistCfg = whitelists["flowmon-whitelist-via-switch"]
    wl_3: WhitelistCfg = whitelists["inherited"]

    # check wl_1
    assert wl_1.name == "flowmon-whitelist"
    assert wl_1.get_items("validation") == {"test1.yml": None, "test2.yml": None, "test3.yml": None}

    # check wl_2
    assert wl_2.name == "flowmon-whitelist-via-switch"
    assert wl_2.get_items("validation") == {
        "test1.yml": None,
        "test2.yml": None,
        "test3.yml": None,
        "vlan.yml": None,
        "mac.yml": "Mac addresses are changed when testing via switch.",
    }

    # check wl_3
    assert wl_3.name == "inherited"
    assert wl_3.get_items("validation") == {
        "test1.yml": None,
        "test2.yml": None,
        "test3.yml": None,
        "vlan.yml": None,
        "mac.yml": "Mac addresses are changed when testing via switch.",
        "last_inherited.yml": "Because",
    }


def test_whitelists_invalid() -> None:
    """Checks invalid whitelists reading"""

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists-invalid-circular.yml",
        )
        assert False
    except ConfigException as err:
        assert "Validation error: WhitelistCfg config error: Circular dependency in whitelist" in str(err)

    try:
        Config(
            f"{FILES_DIR}/authentication.yml",
            f"{FILES_DIR}/generators.yml",
            f"{FILES_DIR}/collectors.yml",
            f"{FILES_DIR}/probes.yml",
            f"{FILES_DIR}/whitelists-invalid-include.yml",
        )
        assert False
    except ConfigException as err:
        assert (
            "Validation error: WhitelistCfg config error: Referenced whitelist 'not-existing-whitelist' was not found."
        ) in str(err)
