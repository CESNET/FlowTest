"""
Author(s):  Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains generator configuration class.
"""

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional, TextIO, Union

import yaml
from dataclass_wizard import YAMLWizard
from src.generator.ft_generator import FtGeneratorConfig


@dataclass
class GeneratorConfig(FtGeneratorConfig, YAMLWizard, key_transform="SNAKE"):
    """Children class of FtGeneratorConfig.
    Contains functions to load and write configuration to YAML file.
    """

    @staticmethod
    def _custom_dump(data: Any, stream: TextIO = None, **kwds) -> Optional[str]:
        """
        Serialize a Python object into a YAML stream.
        If stream is None, return the produced string instead.

        None attributes from dicts are removed.
        """

        def remove_none_attrib(node):
            if isinstance(node, dict):
                res = {}
                for key, value in node.items():
                    if value is None:
                        continue
                    res[key] = remove_none_attrib(value)
                return res
            if isinstance(node, list):
                return list(map(remove_none_attrib, node))
            return node

        return yaml.safe_dump(remove_none_attrib(data), stream, default_flow_style=False, **kwds)

    @staticmethod
    def read_from_file(file: Union[Path, str]):
        """Function reads generator configuration from YAML file.

        Parameters
        ----------
        file : Union[Path, str]
            Path to file.

        Returns
        -------
        GeneratorConfig
            Instance of GeneratorConfig loaded from file.

        Raises
        ------
        FileNotFoundError
            Raised when file cannot be found.
        """

        if not isinstance(file, Path):
            file = Path(file)
        if not file.exists():
            raise FileNotFoundError("GeneratorConfig::read_from_file(): File not found: ", file.as_posix())
        return GeneratorConfig.from_yaml_file(file.as_posix())

    def write_to_file(self, file: Union[Path, str]):
        """Function writes generator configuration to YAML file.

        Parameters
        ----------
        file : Union[Path, str]
            Path to file.
        """

        if not isinstance(file, Path):
            file = Path(file)
        if not self.encapsulation and not self.ipv4 and not self.ipv6 and not self.mac:
            file.touch(exist_ok=True)
        else:
            self.to_yaml_file(file, encoder=GeneratorConfig._custom_dump)
