"""
Author(s):  Jakub Rimek <jakub.rimek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Orchestration configuration entity - AuthenticationCfg"""

from dataclasses import dataclass
from os.path import exists, expanduser, expandvars
from typing import Optional

from dataclass_wizard import YAMLWizard


class AuthenticationCfgException(Exception):
    """Exception raised by the AuthenticationCfg class"""


@dataclass
class AuthenticationCfg(YAMLWizard):
    """AuthenticationCfg configuration entity.

    - name is mandatory
    - contains the key_path OR pair of the username and the password
    - in key_path, username and password strings can be used environment variable, e.g. $CI_PASSWORD or ${$CI_USER}
    - key_path can contain ~ or ~user for expanding user home directory, e.g. "~/.ssh/${KEY_NAME}"
    """

    name: str
    key_path: Optional[str] = None
    username: Optional[str] = None
    password: Optional[str] = None
    ssh_agent: bool = False

    def check(self) -> None:
        """Check the configuration validity."""
        if self.key_path and self.password:
            raise AuthenticationCfgException(
                "AuthenticationCfg config file can not contain both of the key_path and the password."
            )
        if self.ssh_agent and (self.key_path or self.password):
            raise AuthenticationCfgException("Key path or password cannot be set if the ssh agent is used.")
        if not (self.ssh_agent or self.key_path or self.password):
            raise AuthenticationCfgException(
                "At least one authentication (SSH agent, key_path or password) must be present."
            )

        self._expand_environment()

        if self.key_path:
            self._check_key_path()

    def _check_key_path(self) -> None:
        if not exists(self.key_path):
            raise AuthenticationCfgException(f"Key file {self.key_path} was not found.")

    def _expand_environment(self):
        if self.username:
            self.username = expandvars(self.username)
        if self.password:
            self.password = expandvars(self.password)
        if self.key_path:
            self.key_path = expandvars(expanduser(self.key_path))
