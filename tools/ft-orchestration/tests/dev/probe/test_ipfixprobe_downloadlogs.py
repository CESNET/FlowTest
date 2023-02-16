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

from src.config.common import InterfaceCfg
from src.host.host import Host
from src.host.storage import RemoteStorage
from src.probe.probe_target import ProbeTarget
from src.probe.ipfixprobe import IpfixprobeRaw
from src.config.probe import ProbeCfg
from src.config.config import Config
from src.config.authentication import AuthenticationCfg

FILES_DIR = f"{os.getcwd()}/tools/ft-orchestration/conf/"


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
def fixture_get_probe(create_config):
    """Fixture to get probe"""
    auth: AuthenticationCfg = create_config.authentications["validation-xsobol02"]
    probe: ProbeCfg = create_config.probes["ipfixprobe-raw"]
    storage = RemoteStorage(probe.name, None, auth.username, auth.password)
    host = Host(probe.name, storage, auth.username, auth.password)
    return IpfixprobeRaw(
        host=host,
        target=ProbeTarget("127.0.0.1", 3000, "udp"),
        protocols=[],
        interfaces=[InterfaceCfg(name="eno1", speed=10)],
        verbose=True,
        sudo=True,
    )


@pytest.mark.skip("Need a specific ipfixprobe. Use it as a template.")
def test_download(probe):
    """Test download_logs and cleanup"""
    print(f"_log_file: {probe._log_file}")
    logs_gen = "logs/probe"
    probe.start()
    time.sleep(10)
    probe.stop()
    probe.download_logs("logs/ipfixprobe")
    probe.cleanup()
    log_path = os.path.join(logs_gen, "ipfixprobe.log")
    assert os.path.getsize(log_path), f"File {log_path} not found or empty!"
