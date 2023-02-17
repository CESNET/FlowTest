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

from src.host.host import Host
from src.host.storage import RemoteStorage
from src.config.config import Config
from src.config.authentication import AuthenticationCfg
from src.generator.tcpreplay import TcpReplay

FILES_DIR = f"{os.getcwd()}/tools/ft-orchestration/conf/"
PCAP_FILE = f"{os.getcwd()}/tools/ft-orchestration/tests/dev/tcpreplay/NTP_sync.pcap"


@pytest.fixture(name="create_config")
def fixture_create_config():
    """Tests the orchestration config reader"""
    return Config(
        f"{FILES_DIR}/authentications.yml",
        f"{FILES_DIR}/generators.yml",
        f"{FILES_DIR}/collectors.yml",
        f"{FILES_DIR}/probes.yml",
    )


@pytest.fixture(name="gen")
def fixture_get_generator(create_config):
    """Fixture to get generator"""
    auth: AuthenticationCfg = create_config.authentications["validation-xsobol02"]
    gen = create_config.generators["validation-gen"]
    storage = RemoteStorage(gen.name, None, auth.username, auth.password)
    host = Host(gen.name, storage, auth.username, auth.password)
    return TcpReplay(
        host=host,
        verbose=True,
    )


@pytest.mark.skip("Need a specific tcpreplay generator. Use it as a template.")
def test_download(gen):
    """Test download_logs and cleanup"""
    print(f"_log_file: {gen._log_file}")
    logs_gen = "logs/generator"
    gen.add_interface("ens2f1", "40:a6:b7:30:6e:c4")
    gen.start(PCAP_FILE)
    time.sleep(3)
    gen.stop()
    gen.download_logs()
    gen.cleanup()
    log_path = os.path.join(logs_gen, "tcpreplay.log")
    assert os.path.getsize(log_path), f"File {log_path} not found or empty!"
