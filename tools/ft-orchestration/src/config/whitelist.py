"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Orchestration configuration entity - WhitelistCfg.
"""

from dataclasses import dataclass
from typing import Dict, List, Optional, Union

from dataclass_wizard import YAMLWizard


class WhitelistCfgException(Exception):
    """Exception raised by the WhitelistCfg class"""


@dataclass
class WhitelistCfg(YAMLWizard):
    """WhitelistCfg configuration entity.

    - name is mandatory
    - include another whitelist for inheritance
    - items defines tests which are expected to fail in a test group (e.g. validation)
    """

    name: str
    include: Optional[str] = None
    items: Optional[Dict[str, List[Union[str, Dict[str, str]]]]] = None

    def __post_init__(self):
        self._include = None

    def check(self, whitelists: Dict[str, "WhitelistCfg"]) -> None:
        """Check the configuration validity.

        Parameters
        ----------
        whitelists
            Other whitelists for check of include property.
        """

        if not self.name:
            raise WhitelistCfgException("Mandatory field is empty")

        if self.include:
            if self.include not in whitelists:
                raise WhitelistCfgException(
                    f"WhitelistCfg config error: Referenced whitelist '{self.include}' was not found."
                )
            self._check_circular_dependency(whitelists)

            self._include = whitelists[self.include]

    def _check_circular_dependency(self, whitelists: Dict[str, "WhitelistCfg"]):
        """Check circular dependency in whitelist caused by include property."""

        tmp_include = self.include
        while tmp_include is not None:
            if tmp_include == self.name:
                raise WhitelistCfgException("WhitelistCfg config error: Circular dependency in whitelist")
            try:
                tmp_include = whitelists[tmp_include].include
            except KeyError:
                break

    def get_items(self, test_group: str) -> Dict[str, Optional[str]]:
        """Get list of xfail tests.

        Parameters
        ----------
        test_group : str
            Type of tests (e.g. validation).

        Returns
        -------
        Dict[str, Optional[str]]
            Dict of tests and failure reason. Value is None if no reason specified.
        """

        res = {}

        if self._include:
            res.update(self._include.get_items(test_group))

        if self.items and test_group in self.items:
            res.update(self._preprocess_items(self.items[test_group]))

        return res

    @staticmethod
    def _preprocess_items(items: List[Union[str, Dict[str, str]]]) -> Dict[str, Optional[str]]:
        res_items = {}
        for item in items:
            if isinstance(item, dict):
                key, reason = list(item.items())[0]
                res_items[key] = reason
            else:
                res_items[item] = None
        return res_items
