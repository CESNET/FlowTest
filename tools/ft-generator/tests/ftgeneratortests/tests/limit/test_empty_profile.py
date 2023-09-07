"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains empty profile test for ft-generator module.
"""

import subprocess
from pathlib import Path
from typing import Optional

from ftgeneratortests.src import GENERATOR_ERROR_RET_CODE, Generator, GeneratorConfig


def create_profiles_str() -> str:
    """Function creates custom profiles.

    Returns
    -------
    str
        Profiles file text.
    """

    text = (
        "START_TIME,END_TIME,L3_PROTO,L4_PROTO,SRC_PORT,DST_PORT,PACKETS,BYTES,PACKETS_REV,BYTES_REV\n"
        "0,28354,4,6,58428,1443,318,427066,100,20000\n"
        ",,,,,,,,,\n"
        "0,28354,6,17,58428,1443,318,427066,100,20000"
    )

    return text


def test_empty_profile(ft_generator: Generator, custom_config: Optional[Path]):
    """Test verifies if empty profile can be generated.
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
        ft_generator.generate(config, create_profiles_str())
    except subprocess.CalledProcessError as exc:
        if exc.returncode != GENERATOR_ERROR_RET_CODE:
            # Different error return code
            assert False
    else:
        # No error raised
        assert False
