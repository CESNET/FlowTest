"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains zero profile test for ft-generator module.
"""

import subprocess
from pathlib import Path
from typing import Optional

from ftgeneratortests.src import (
    GENERATOR_ERROR_RET_CODE,
    Flow,
    FlowCache,
    Generator,
    GeneratorConfig,
)


def create_profiles() -> FlowCache:
    """Function creates custom profiles.

    Returns
    -------
    FlowCache
        FlowCache with created profiles.
    """

    flow_cache = FlowCache()
    flow_cache.add(
        Flow(
            src_port=58428,
            dst_port=1443,
            l3_proto=4,
            l4_proto=1,
            start_time=0,
            end_time=28354,
            packets=1,
            packets_rev=0,
            bytes=120,
            bytes_rev=0,
        ),
        key=1,
    )
    flow_cache.add(Flow(), key=2)
    flow_cache.add(
        Flow(
            src_port=63172,
            dst_port=80,
            l3_proto=4,
            l4_proto=6,
            start_time=191855,
            end_time=191880,
            packets=4,
            packets_rev=3,
            bytes=454,
            bytes_rev=403,
        ),
        key=3,
    )

    return flow_cache


def test_zero_profile(ft_generator: Generator, custom_config: Optional[Path]):
    """Test verifies if profile with all attributes set to 0 can be generated.
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
        if exc.returncode != GENERATOR_ERROR_RET_CODE:
            # Different error return code
            assert False
    else:
        # No error raised
        assert False
