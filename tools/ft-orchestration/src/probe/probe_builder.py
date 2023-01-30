"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Builder for creating a probe instance based on a static configuration.
"""

from os import path
from typing import Any, Dict, List

from lbr_testsuite.topology import Device
from src.common.builder_base import BuilderBase, BuilderError
from src.config.common import InterfaceCfg
from src.config.config import Config
from src.config.probe import ProbeCfg
from src.probe.interface import ProbeInterface
from src.probe.probe_target import ProbeTarget

PROBE_IMPORT_PATH = path.dirname(path.realpath(__file__))


class ProbeBuilder(BuilderBase, Device):
    """Builder for creating a probe instance based on a static configuration.
    Probe class is dynamically imported from module in 'probe' directory."""

    def __init__(
        self,
        config: Config,
        alias: str,
        target: ProbeTarget,
        enabled_interfaces: List[str],
        cmd_connector_args: Dict[str, Any],
    ) -> None:
        """Create probe instance from static configuration by alias identifier.

        Parameters
        ----------
        config : Config
            Static configuration object.
        alias : str
            Unique identifier in static configuration.
        target : ProbeTarget
            Export target passed to probe constructor.
        enabled_interfaces : List[str]
            Network interfaces where the exporting process should be initiated.
        cmd_connector_args : Dict[str, Any]
            Additional settings passed to connector.

        Raises
        ------
        BuilderError
            Raised when type of probe from static configuration not found in python modules.
            Or alias does not exist in configuration.
        """

        super().__init__(config)  # pylint: disable=too-many-function-args

        if alias not in self._config.probes:
            raise BuilderError(f"Probe '{alias}' not found in probes configuration.")
        probe_cfg = self._config.probes[alias]

        self._prepare_env(probe_cfg)

        self._target = target
        self._interfaces = self._load_interfaces(probe_cfg, enabled_interfaces)
        self._connector_args = probe_cfg.connector if probe_cfg.connector else {}

        # cmd additional arguments has higher priority, update arguments from config
        self._connector_args.update(cmd_connector_args)

        self._class = self._find_class(PROBE_IMPORT_PATH, probe_cfg.type, ProbeInterface)

    # pylint: disable=arguments-differ
    def get(self, required_protocols: List[str]) -> ProbeInterface:
        """Create probe instance from static configuration by alias identifier.

        Parameters
        ----------
        required_protocols : List[str]
            List of the networking protocols which the probe should parse and export.

        Returns
        -------
        ProbeInterface
            New probe instance.
        """

        return self._class(self._host, self._target, required_protocols, self._interfaces, **self._connector_args)

    def get_mac_addresses(self) -> List[str]:
        """Get list of hardware addresses of enabled interfaces.

        Returns
        -------
        List[str]
            Hardware addresses of enabled interfaces.
        """

        return [ifc.mac for ifc in self._interfaces]

    @staticmethod
    def _load_interfaces(probe_cfg: ProbeCfg, enabled_interfaces: List[str]) -> List[InterfaceCfg]:
        """Check and convert string interfaces from cmd argument to InterfaceCfg form.

        Parameters
        ----------
        probe_cfg : ProbeCfg
            Probe static configuration.
        enabled_interfaces : List[str]
            Interfaces from test cmd argument.

        Returns
        -------
        List[InterfaceCfg]
            Converted interfaces.

        Raises
        ------
        BuilderError
            When interface is not supported by probe.
        """

        if len(enabled_interfaces) == 0:
            return probe_cfg.interfaces

        interfaces = []
        for ifc in enabled_interfaces:
            found = False
            for ifc_cfg in probe_cfg.interfaces:
                if ifc_cfg.name == ifc:
                    interfaces.append(ifc_cfg)
                    found = True
                    break

            if not found:
                raise BuilderError(f"Interface '{ifc}' is not supported by probe '{probe_cfg.alias}'.")

        return interfaces
