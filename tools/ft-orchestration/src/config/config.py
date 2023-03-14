"""
Author(s): Jakub Rimek <jakub.rimek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Orchestration config file management
"""
import logging
from typing import Dict, Union

from dataclass_wizard.errors import MissingFields, ParseError
from src.config.authentication import AuthenticationCfg, AuthenticationCfgException
from src.config.collector import CollectorCfg, CollectorCfgException
from src.config.generator import GeneratorCfg, GeneratorCfgException
from src.config.probe import ProbeCfg, ProbeCfgException
from src.config.whitelist import WhitelistCfg, WhitelistCfgException


class ConfigException(Exception):
    """Exception raised by the Config class"""


# pylint: disable=too-few-public-methods
class Config:
    """Orchestration configuration files reader.
    Configuration files:
        - generators.yml
        - collectors.yml
        - probes.yml
        - authentication.yml

    Fields:
    -----------
    generators : Dict[str, GeneratorCfg]
        Dictionary of the GeneratorCfg objects, key is the 'alias' field
    collectors : Dict[str, CollectorCfg]
        Dictionary of the CollectorCfg objects, key is the 'alias' field
    probes : Dict[str, ProbeCfg]
        Dictionary of the ProbeCfg objects, key is the 'alias' field
    authentication : Dict[str, AuthenticationCfg]
        Dictionary of the AuthenticationCfg objects, key is the 'name' field
    """

    def __init__(
        self,
        authentications_path: str,
        generators_path: str,
        collectors_path: str,
        probes_path: str,
        whitelists_path: str,
    ) -> None:
        """Reads the configuration files.

        Parameters
        ----------
        authentications_path - path to the authentication.yml
        generators_path - path to the generators.yml
        collectors_path - path to the collectors.yml
        probes_path - path to the probes.yml
        whitelists_path - path to the whitelists.yml

        Raises
        ------
        ConfigException
            If the configuration is not valid or not found.
        """
        self.authentications: Dict[str, AuthenticationCfg] = self._load(
            authentications_path, AuthenticationCfg.from_yaml_file
        )
        self.whitelists: Dict[str, WhitelistCfg] = self._load(whitelists_path, WhitelistCfg.from_yaml_file)
        self.generators: Dict[str, GeneratorCfg] = self._load(generators_path, GeneratorCfg.from_yaml_file)
        self.collectors: Dict[str, CollectorCfg] = self._load(collectors_path, CollectorCfg.from_yaml_file)
        self.probes: Dict[str, ProbeCfg] = self._load(probes_path, ProbeCfg.from_yaml_file)
        self._check()
        logging.getLogger().debug("Orchestration config loaded")

    @staticmethod
    def _load(
        path: str, reader
    ) -> Union[Dict[str, AuthenticationCfg], Dict[str, GeneratorCfg], Dict[str, CollectorCfg], Dict[str, ProbeCfg]]:
        try:
            logging.getLogger().info("Loading configuration from %s", path)
            res = reader(path)
            return {(x.alias if hasattr(x, "alias") else x.name): x for x in res}
        except (OSError, FileNotFoundError) as err:
            logging.getLogger().error("Error reading config file %s, code %i, exception %s", path, err.errno, err)
            raise ConfigException(f"Error reading config file {path}, code {err.errno}, exception {err}") from err
        except (TypeError, AttributeError, ParseError, MissingFields) as err:
            logging.getLogger().error("Parsing error of config file %s, exception %s", path, err)
            raise ConfigException(f"Parsing error of config file {path}, exception {err}") from err

    def _check(self) -> None:
        try:
            for val in self.authentications.values():
                val.check()
            for val in self.whitelists.values():
                val.check(self.whitelists)
            for val in self.generators.values():
                val.check(self.authentications)
            for val in self.collectors.values():
                val.check(self.authentications)
            for val in self.probes.values():
                val.check(self.authentications, self.whitelists)
        except (
            AuthenticationCfgException,
            CollectorCfgException,
            GeneratorCfgException,
            ProbeCfgException,
            WhitelistCfgException,
        ) as err:
            logging.getLogger().error("Validation error: %s", err)
            raise ConfigException(f"Validation error: {err}") from err
