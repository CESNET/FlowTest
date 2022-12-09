"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Builder for creating a generator instance based on a static configuration.
"""

import logging
from os import path
from typing import Any, Dict, List, Optional

from lbr_testsuite.topology.generator import Generator
from src.common.builder_base import BuilderBase, BuilderError
from src.config.common import InterfaceCfg
from src.config.config import Config
from src.generator.interface import PcapPlayer

GENERATOR_IMPORT_PATH = path.dirname(path.realpath(__file__))


class GeneratorBuilder(BuilderBase, Generator):
    """Builder for creating a generator instance based on a static configuration.
    Generator class is dynamically imported from module in 'generator' directory."""

    def __init__(
        self,
        config: Config,
        alias: str,
        probe_mac_addresses: List[str],
        add_vlan: Optional[int] = None,
        edit_dst_mac: bool = True,
        cmd_connector_args: Dict[str, Any] = frozenset(),
    ) -> None:
        """Create generator instance from static configuration by alias identifier.

        Parameters
        ----------
        config : Config
            Static configuration object.
        alias : str
            Unique identifier in static configuration.
        probe_mac_addresses : List[str]
            List of mac addresses of probe input interfaces.
        add_vlan : int, optional
            If specified, vlan header with given tag will be added to sent packets.
        edit_dst_mac : bool, optional
            If true, destination mac address will be edited in packets.
        cmd_connector_args : Dict[str, Any], optional
            Additional settings passed to connector.

        Raises
        ------
        BuilderError
            Raised when type of generator from static configuration not found in python modules. Or alias does not
            exist in configuration. Or number of generator interfaces does not match number of probe interfaces.
        """

        super().__init__(config)

        if alias not in self._config.generators:
            raise BuilderError(f"Generator '{alias}' not found in generators configuration.")
        generator_cfg = self._config.generators[alias]

        self._prepare_env(generator_cfg)

        self._add_vlan = add_vlan
        self._edit_dst_mac = edit_dst_mac
        self._probe_mac_addresses = probe_mac_addresses
        self._interfaces = generator_cfg.interfaces
        self._connector_args = cmd_connector_args

        if len(self._probe_mac_addresses) != len(self._interfaces):
            logging.getLogger().error("Number of generator interfaces should match number of probe interfaces.")
            raise BuilderError("Number of generator interfaces should match number of probe interfaces.")

        self._class = self._find_class(GENERATOR_IMPORT_PATH, generator_cfg.type, PcapPlayer)

    # pylint: disable=arguments-differ
    def get(self) -> PcapPlayer:
        """Create instance of generator by alias.

        Returns
        -------
        PcapPlayer
            New generator instance.
        """

        instance = self._class(self._host, self._add_vlan, **self._connector_args)
        for i, ifc in enumerate(self._interfaces):
            mac = self._probe_mac_addresses[i] if self._edit_dst_mac else None
            instance.add_interface(ifc.name, mac)
        return instance

    def get_enabled_interfaces(self) -> List[InterfaceCfg]:
        """Get interfaces on which traffic is sent.

        Returns
        -------
        List[InterfaceCfg]
            Generator enabled interfaces.
        """

        return self._interfaces

    def get_vlan(self) -> Optional[int]:
        """Get vlan tag added to sent packets.

        Returns
        -------
        Optional[int]
            Vlan tag if specified, otherwise None.
        """

        return self._add_vlan
