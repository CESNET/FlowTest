"""
Author(s):  Jakub Rimek <jakub.rimek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Orchestration configuration entity - ProbeCfg"""

from typing import Optional, List, Dict

from dataclasses import dataclass
from dataclass_wizard import YAMLWizard

from src.config.authentication import AuthenticationCfg
from src.config.common import InterfaceCfg
from src.config.whitelist import WhitelistCfg


class ProbeCfgException(Exception):
    """Exception raised by the ProbeCfg class"""


@dataclass
class ProbeCfg(YAMLWizard):
    """ProbeCfg configuration entity"""

    alias: str
    name: str
    type: str
    interfaces: List[InterfaceCfg]
    authentication: str
    tags: List[str]
    connector: Optional[dict] = None
    ansible_playbook_role: Optional[str] = None
    tests_whitelist: Optional[str] = None

    def check(self, authentications: Dict[str, AuthenticationCfg], whitelists: Dict[str, WhitelistCfg]) -> None:
        """Check the configuration validity.

        Parameters
        ----------
        authentications
            Dictionary of the AuthenticationCfg objects
        """
        if not self.alias or not self.name or not self.type or not self.interfaces or not self.authentication:
            raise ProbeCfgException("Mandatory field is empty")

        if not all(x.mac for x in self.interfaces):
            raise ProbeCfgException("Mac address cannot be empty in probe interface")

        if self.authentication not in authentications:
            raise ProbeCfgException(
                f"ProbeCfg config error: AuthenticationCfg name {self.authentication} was not found in the "
                f"Authentications config file"
            )

        if self.tests_whitelist and self.tests_whitelist not in whitelists:
            raise ProbeCfgException(f"Whitelist '{self.tests_whitelist}' was not found in whitelists config")
