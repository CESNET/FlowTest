"""
Author(s): Tomas Jansky <tomas.jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Configuration of individual test scenarios.
"""

import hashlib
import logging
import os
from dataclasses import dataclass, field
from typing import Optional
from urllib.parse import urlparse
from urllib.request import HTTPError, urlretrieve

from dataclass_wizard import YAMLWizard
from ftanalyzer.models.sm_data_types import SMMetric
from src.common.required_field import required_field
from src.generator.ft_generator import FtGeneratorConfig


class ScenarioCfgException(Exception):
    """Exception raised by the ScenarioCfg class"""


@dataclass
class Requirements(YAMLWizard):
    """
    Fields which must match with the traffic generator capabilities and probe capabilities for a test to run.
    """

    protocols: list[str] = field(default_factory=list)
    speed: Optional[int] = 0


@dataclass
class ProbeCfg(YAMLWizard):
    """Probe runtime configuration of a scenario."""

    protocols: list[str] = field(default_factory=list)
    connectors: list[dict] = field(default_factory=list)
    active_timeout: Optional[int] = None
    inactive_timeout: Optional[int] = None

    def get_args(self, probe_type: str) -> dict:
        """Get arguments which can be passed directly to the probe builder.
        Arguments are chosen for the provided probe type.

        Parameters
        ----------
        probe_type: str
            Type of the probe to be built.

        Returns
        -------
        dict
            Probe configuration parameters.
        """

        ret = {
            "protocols": self.protocols,
        }
        if self.active_timeout:
            ret["active_timeout"] = self.active_timeout
        if self.inactive_timeout:
            ret["inactive_timeout"] = self.inactive_timeout
        for connector in self.connectors:
            if connector.get("type", "") == probe_type:
                del connector["type"]
                ret.update(connector)

        return {k: v for k, v in ret.items() if v is not None}


@dataclass
class ScenarioCfg(YAMLWizard):
    """Test scenario configuration."""

    name: str = required_field()
    description: str = required_field()
    marks: list[str] = required_field()
    exclude: list[str] = field(default_factory=list)
    requirements: Requirements = Requirements()

    def check(self) -> None:
        """Check validity of the configuration."""

        if len(self.name) == 0 or len(self.description) == 0:
            raise ScenarioCfgException("Empty name or description.")


@dataclass
class SimulationScenario(ScenarioCfg):
    """Simulation testing scenario configuration."""

    profile: str = required_field()
    mtu: int = required_field()
    pps: int = required_field()
    sampling: float = required_field()
    analysis: list[SMMetric] = required_field()

    probe: Optional[ProbeCfg] = ProbeCfg()
    generator: Optional[FtGeneratorConfig] = FtGeneratorConfig()

    def get_profile(self, scenario_filename: str, simulation_tests_dir: str) -> str:
        """
        Download simulation profile if URL is provided in scenario configuration.

        Parameters
        ----------
        scenario_filename: str
            Path to scenario filename.
        simulation_tests_dir: str
            Path to directory used for simulation tests.

        Returns
        -------
        str
            Absolute path to profile - csv file.
        """
        parsed_url = urlparse(self.profile)
        if parsed_url.scheme and parsed_url.netloc:
            profiles_dir = "profiles"
            # add hash of URL to downloaded profile
            hash_url = hashlib.md5(self.profile.encode("utf-8")).hexdigest()
            profile_name = os.path.splitext(os.path.basename(scenario_filename))[0]
            profile_name = f"{profile_name}_{hash_url}.csv"
            profile_path = os.path.join(simulation_tests_dir, profiles_dir, profile_name)

            # duplicity - check if the profile has been already downloaded in some previous tests
            csv_list = [f for f in os.listdir(os.path.dirname(profile_path)) if hash_url in f]
            if csv_list:
                return os.path.join(simulation_tests_dir, profiles_dir, csv_list[0])

            # profile does not exist - download it
            try:
                logging.getLogger().info("Downloading scenario profile from URL %s", self.profile)
                urlretrieve(self.profile, profile_path)
            except HTTPError as err:
                raise ScenarioCfgException("Error downloading profile from provided URL.") from err

            return profile_path

        # profile is local path
        return os.path.join(simulation_tests_dir, self.profile)


@dataclass
class ValidationScenario(ScenarioCfg):
    """Validation testing scenario configuration."""

    pcap: str = required_field()
    flows: list[dict] = required_field()

    key: Optional[list[str]] = None
    at_least_one: Optional[list[str]] = None
    probe: Optional[ProbeCfg] = ProbeCfg()
