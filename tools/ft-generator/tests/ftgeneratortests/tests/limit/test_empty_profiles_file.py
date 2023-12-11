"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains empty profile file test for ft-generator module.
"""

import subprocess
from pathlib import Path
from typing import Optional

from ftgeneratortests.src import GENERATOR_ERROR_RET_CODE, Generator, GeneratorConfig


def test_empty_profiles_file(ft_generator: Generator, custom_config: Optional[Path]):
    """Test verifies if empty profile file with no header can be used.
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
        ft_generator.generate(config, "")
    except subprocess.CalledProcessError as exc:
        if exc.returncode != GENERATOR_ERROR_RET_CODE:
            # Different error return code
            assert False
    else:
        # No error raised
        assert False
