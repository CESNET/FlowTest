"""
Author(s):  Jakub Rimek <jakub.rimek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Orchestration configuration entity - GeneratorCfg"""

from dataclasses import dataclass
from typing import Any, Dict, List, Optional

from dataclass_wizard import YAMLWizard
from src.config.authentication import AuthenticationCfg
from src.config.common import InterfaceCfg


class GeneratorCfgException(Exception):
    """Exception raised by the GeneratorCfg class"""


@dataclass
class GeneratorCfg(YAMLWizard):
    """GeneratorCfg configuration entity"""

    alias: str
    name: str
    type: str
    interfaces: List[InterfaceCfg]
    authentication: str
    connector: Optional[dict] = None
    ansible_playbook_role: Optional[str] = None
    ansible_extra_vars: Optional[dict[str, Any]] = None
    ansible_host_group: Optional[str] = None
    ansible_skip_tags: Optional[list[str]] = None

    def check(self, authentications: Dict[str, AuthenticationCfg]) -> None:
        """Check the configuration validity.

        Parameters
        ----------
        authentications
            Dictionary of the AuthenticationCfg objects
        """
        if not self.alias or not self.name or not self.type or not self.interfaces or not self.authentication:
            raise GeneratorCfgException("Mandatory field is empty")

        if self.authentication not in authentications:
            raise GeneratorCfgException(
                f"GeneratorCfg config error: AuthenticationCfg name {self.authentication} was not found in the "
                f"Authentications config file"
            )
