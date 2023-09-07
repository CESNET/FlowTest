"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains one reverse packet test test for ft-generator module.
"""

import signal
import subprocess
from pathlib import Path
from typing import Optional

from ftgeneratortests.src import Flow, FlowCache, Generator, GeneratorConfig


def create_profiles() -> FlowCache:
    """Function creates custom profiles.

    Returns
    -------
    FlowCache
        FlowCache with created profiles.
    """

    flow = Flow(
        src_port=58428,
        dst_port=1443,
        l3_proto=4,
        l4_proto=1,
        start_time=0,
        end_time=28354,
        packets=0,
        packets_rev=1,
        bytes=0,
        bytes_rev=120,
    )
    flow_cache = FlowCache()
    flow_cache.add(flow, key=1)

    return flow_cache


def test_one_rev_packet(ft_generator: Generator, custom_config: Optional[Path]):
    """Test verifies if one reverse flow packet and no forward flow packet can be generated.
    Ft-generator should fail.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    custom_config: Optional[Path]
        Path to custom configuration file for ft-generator.
        None if not provided.
    """

    if custom_config:
        config = GeneratorConfig.read_from_file(custom_config)
    else:
        config = GeneratorConfig()

    try:
        ft_generator.generate(config, create_profiles())
    except subprocess.CalledProcessError as exc:
        if exc.returncode != -signal.SIGABRT:
            # Different error return code
            assert False
    else:
        # No error raised
        assert False
