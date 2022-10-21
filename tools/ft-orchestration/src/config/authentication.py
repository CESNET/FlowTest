"""
Author(s):  Jakub Rimek <jakub.rimek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Orchestration configuration entity - AuthenticationCfg"""
from os.path import exists
from typing import Optional

from dataclasses import dataclass
from dataclass_wizard import YAMLWizard


class AuthenticationCfgException(Exception):
    """Exception raised by the AuthenticationCfg class"""


@dataclass
class AuthenticationCfg(YAMLWizard):
    """AuthenticationCfg configuration entity.

    - name is mandatory
    - contains the key_path OR pair of the username and the password
    """

    name: str
    key_path: Optional[str] = None
    username: Optional[str] = None
    password: Optional[str] = None

    def check(self) -> None:
        """Check the configuration validity."""
        if self.key_path and self.password and self.password:
            raise AuthenticationCfgException(
                "AuthenticationCfg config file can not contain both of the key_path and the username/password."
            )
        if self.key_path:
            self._check_key_path()
        elif self.password and self.password:
            pass
        else:
            raise AuthenticationCfgException("Field key_path or both fields username and password must be present.")

    def _check_key_path(self) -> None:
        if not exists(self.key_path):
            raise AuthenticationCfgException(f"Key file {self.key_path} was not found.")
