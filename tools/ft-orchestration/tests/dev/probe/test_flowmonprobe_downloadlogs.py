# pylint: disable=protected-access
"""
Author(s): Vaclav Seidler <Vaclav.Seidler@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Test for download_logs and cleanup.
"""

import os
import time

import pytest
from src.config.authentication import AuthenticationCfg
from src.config.common import InterfaceCfg
from src.config.config import Config
from src.config.generator import GeneratorCfg
from src.config.probe import ProbeCfg
from src.generator.tcpreplay import TcpReplay
from src.host.host import Host
from src.host.storage import RemoteStorage
from src.probe.flowmon_probe import FlowmonProbe
from src.probe.probe_target import ProbeTarget

FILES_DIR = f"{os.getcwd()}/tools/ft-orchestration/conf/"
PCAP_FILE = f"{os.getcwd()}/tools/ft-orchestration/tests/dev/tcpreplay/NTP_sync.pcap"

PROBE_INTERFACE = "eth2"

LOG_FILES = [
    f"tmp_probe_{PROBE_INTERFACE}.json",
    "flowmonexp.log",
    "flowmonexp_init.log",
]
LOG_DEBUG_FILES = [
    "probe_output.json",
    "flowmonexp_debug.log",
]
LOG_ALL = LOG_FILES + LOG_DEBUG_FILES


@pytest.fixture(name="create_config")
def fixture_create_config():
    """Tests the orchestration config reader"""
    return Config(
        f"{FILES_DIR}/authentications.yml",
        f"{FILES_DIR}/generators.yml",
        f"{FILES_DIR}/collectors.yml",
        f"{FILES_DIR}/probes.yml",
    )


@pytest.fixture(name="probe")
def fixture_probe(create_config):
    """Fixture to get probe"""
    auth: AuthenticationCfg = create_config.authentications["validation-auth"]
    probe: ProbeCfg = create_config.probes["validation-probe-flowmon"]
    storage = RemoteStorage(probe.name, None, auth.username, auth.password)
    host = Host(probe.name, storage, auth.username, auth.password)
    return FlowmonProbe(
        host=host,
        target=ProbeTarget("127.0.0.1", 3000, "udp"),
        protocols=[],
        interfaces=[InterfaceCfg(name=PROBE_INTERFACE, speed=10)],
        verbose=True,
        active_timeout=10,
        inactive_timeout=3,
    )


@pytest.fixture(name="gen")
def fixture_gen(create_config):
    """Fixture to get generator"""
    auth: AuthenticationCfg = create_config.authentications["validation-xsobol02"]
    gen: GeneratorCfg = create_config.generators["validation-gen"]
    storage = RemoteStorage(gen.name, None, auth.username, auth.password)
    host = Host(gen.name, storage, auth.username, auth.password)
    return TcpReplay(host=host, add_vlan=90)


@pytest.mark.skip("Need a specific Flowmon and a generator. Use it as a template.")
def test_download(probe, gen):
    """Test download_logs and cleanup"""
    print(f"probe _log_file: {probe._remote_dir}")
    print(f"generator _log_file: {gen._remote_log_file}")
    logs_probe = "logs/flowmonprobe"
    probe.start()
    gen.add_interface("ens2f1", "40:a6:b7:30:6e:c4")
    gen.start(PCAP_FILE)
    time.sleep(10)
    probe.stop()
    probe.download_logs(logs_probe)
    probe.cleanup()
    gen.cleanup()
    for probe_files in LOG_ALL:
        log_path = os.path.join(logs_probe, probe_files)
        assert os.path.getsize(log_path), f"File {log_path} not found or empty!"
