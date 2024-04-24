"""
Author(s):  Jakub Rimek <jakub.rimek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Orchestration configuration entity - CollectorCfg"""

from dataclasses import dataclass
from typing import Any, Dict, Optional

from dataclass_wizard import YAMLWizard
from src.config.authentication import AuthenticationCfg


class CollectorCfgException(Exception):
    """Exception raised by the CollectorCfg class"""


@dataclass
class CollectorCfg(YAMLWizard):
    """CollectorCfg configuration entity"""

    alias: str
    name: str
    type: str
    authentication: str
    ansible_playbook_role: Optional[str] = None
    ansible_extra_vars: Optional[dict[str, Any]] = None
    ansible_host_group: Optional[str] = None
    ansible_skip_tags: Optional[list[str]] = None

    def check(self, authentication: Dict[str, AuthenticationCfg]) -> None:
        """Check the configuration validity.

        Parameters
        ----------
        authentication
            Dictionary of the AuthenticationCfg objects
        """
        if not self.alias or not self.name or not self.authentication or not self.type:
            raise CollectorCfgException("Mandatory field is empty")

        if self.authentication not in authentication:
            raise CollectorCfgException(
                f"CollectorCfg config error: AuthenticationCfg name {self.authentication} was not found in the "
                f"Authentications config file"
            )
