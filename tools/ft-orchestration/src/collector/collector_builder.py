"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Builder for creating a collector instance based on a static configuration.
"""

from os import path
from typing import Any, Dict

from lbr_testsuite.topology.analyzer import Analyzer
from src.collector.interface import CollectorInterface
from src.common.builder_base import BuilderBase, BuilderError
from src.config.config import Config
from src.probe.probe_target import ProbeTarget

COLLECTOR_IMPORT_PATH = path.dirname(path.realpath(__file__))


class CollectorBuilder(BuilderBase, Analyzer):
    """Builder for creating a collector instance based on a static configuration.
    Collector class is dynamically imported from module in 'collector' directory."""

    def __init__(
        self,
        config: Config,
        disable_ansible: bool,
        extra_import_paths: list[str],
        alias: str,
        input_plugin: str,
        port: int,
        cmd_connector_args: Dict[str, Any],
    ) -> None:
        """Create collector instance from static configuration by alias identifier.

        Parameters
        ----------
        config : Config
            Static configuration object.
        disable_ansible: bool
            If True, ansible preparation (with ansible_playbook_role) is disabled.
        extra_import_paths: list[str]
            Extra paths from which connectors are imported.
        alias : str
            Unique identifier in static configuration.
        input_plugin : str
            Input plugin (protocol) to use for receiving.
        port : int
            Port on which collector should listen.
        cmd_connector_args : Dict[str, Any]
            Additional settings passed to connector.

        Raises
        ------
        BuilderError
            Raised when type of probe from static configuration not found in python modules.
            Or alias does not exist in configuration.
        """

        super().__init__(config, disable_ansible)  # pylint: disable=too-many-function-args

        if alias not in self._config.collectors:
            raise BuilderError(f"Collector '{alias}' not found in collectors configuration.")
        collector_cfg = self._config.collectors[alias]

        self._prepare_env(collector_cfg)

        self._input_plugin = input_plugin
        self._port = port
        self._probe_target = ProbeTarget(collector_cfg.name, port, input_plugin)
        self._connector_args = cmd_connector_args

        import_paths = extra_import_paths + [COLLECTOR_IMPORT_PATH]
        self._class = self._find_class(import_paths, collector_cfg.type, CollectorInterface)

    # pylint: disable=arguments-differ
    def get(self) -> CollectorInterface:
        """Create collector instance.

        Returns
        -------
        CollectorInterface
            New collector instance.
        """

        return self._class(self._executor, self._input_plugin, self._port, **self._connector_args)

    def get_probe_target(self) -> ProbeTarget:
        """Get exporting target used by probe connector.

        Returns
        -------
        ProbeTarget
            Export target.
        """

        return self._probe_target
