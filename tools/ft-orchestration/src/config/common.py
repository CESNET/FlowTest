"""
Author(s):  Jakub Rimek <jakub.rimek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Common classes used by the orchestration configuration"""

from dataclasses import dataclass


@dataclass
class InterfaceCfg:
    """Network interface class, used by the GeneratorCfg and ProbeCfg classes"""

    name: str
    speed: int
