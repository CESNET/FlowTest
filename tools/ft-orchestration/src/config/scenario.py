"""
Author(s): Tomas Jansky <tomas.jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Configuration of individual test scenarios.
"""

import copy
import hashlib
import logging
import os
from dataclasses import dataclass, field
from typing import Optional, Union
from urllib.parse import urlparse
from urllib.request import HTTPError, urlretrieve

from dataclass_wizard import YAMLWizard
from ftanalyzer.models.sm_data_types import SMMetric, SMMetricType
from src.common.required_field import required_field
from src.generator.ft_generator import FtGeneratorConfig
from src.generator.interface import MbpsSpeed, MultiplierSpeed, PpsSpeed


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
class AnalysisCfg(YAMLWizard):
    """Configuration of a test scenario evaluation."""

    model: str = "statistical"
    metrics: list[SMMetric] = field(
        default_factory=lambda: [SMMetric(SMMetricType.PACKETS, 0.0001), SMMetric(SMMetricType.BYTES, 0.0001)]
    )


@dataclass
class SimTest(YAMLWizard):
    """Mandatory configuration for all individual tests in simulation test scenarios."""

    id: str = required_field()
    enabled: bool = True
    marks: list[str] = field(default_factory=list)


@dataclass
class SimConfig(YAMLWizard):
    """Basic configurable options for simulation tests."""

    pps: Optional[int] = None
    mbps: Optional[int] = None
    speed_multiplier: Optional[float] = None
    probe: Optional[ProbeCfg] = None
    generator: Optional[FtGeneratorConfig] = None

    def get_probe_conf(self, probe_type: str, default: ProbeCfg) -> dict:
        """Get probe configuration for a specific probe type (merged with the default configuration).

        Parameters
        ----------
        probe_type: str
            Probe type.
        default: ProbeCfg
            Default probe configuration.

        Returns
        -------
        dict
            ProbeCfg configuration for the specified probe type.
        """

        probe_conf = default.get_args(probe_type)
        if self.probe is None:
            return probe_conf

        return {**probe_conf, **self.probe.get_args(probe_type)}

    def get_generator_conf(self, default: FtGeneratorConfig) -> FtGeneratorConfig:
        """Get generator configuration (merged with the default configuration).

        Parameters
        ----------
        default: FtGeneratorConfig
            Default generator configuration.

        Returns
        -------
        FtGeneratorConfig
            Generator configuration.
        """

        if self.generator is not None:
            default.update(self.generator)

        return default

    def get_replay_speed(self, default: "SimConfig") -> Union[MbpsSpeed, MultiplierSpeed, PpsSpeed]:
        """
        Determine replay speed from the provided configuration.
        Priorities:
            test mbps -> test pps -> test multiplier ->
            -> default mbps -> default pps -> default multiplier -> multiplier 1.0

        Parameters
        ----------
        default: SimConfig
            Default configuration.

        Returns
        -------
        MbpsSpeed, MultiplierSpeed, PpsSpeed
            Replay speed.
        """

        # pylint: disable=too-many-return-statements
        if self.mbps is not None:
            return MbpsSpeed(self.mbps)

        if self.pps is not None:
            return PpsSpeed(self.pps)

        if self.speed_multiplier is not None:
            return MultiplierSpeed(self.speed_multiplier)

        if default.mbps is not None:
            return MbpsSpeed(default.mbps)

        if default.pps is not None:
            return PpsSpeed(default.pps)

        if default.speed_multiplier is not None:
            return MultiplierSpeed(default.speed_multiplier)

        return MultiplierSpeed(1.0)


@dataclass
class SimGeneral(SimTest, SimConfig):
    """Configuration of an individual test in general simulation test scenario."""

    loops: int = 1
    replicator: Optional[int] = None
    analysis: AnalysisCfg = AnalysisCfg()

    def get_replicator_units(self, sampling: float) -> int:
        """Get number of replicator units (either directly configured or based on sampling value).

        Parameters
        ----------
        sampling: float
            Sampling value used to create the profile.

        Returns
        -------
        int
            Number of replicator units.
        """

        return self.replicator if self.replicator is not None else int(1 / sampling)


@dataclass
class SimOverload(SimTest, SimConfig):
    """Configuration of an individual test in overload simulation test scenario."""

    multiplier: float = 1.0
    analysis: AnalysisCfg = AnalysisCfg()


@dataclass
class SimThreshold(SimTest, SimConfig):
    """Configuration of an individual test in threshold simulation test scenario."""

    mbps_accuracy: int = required_field()
    speed_min: int = 0
    speed_max: Optional[int] = None
    mbps_required: int = 0
    analysis: AnalysisCfg = AnalysisCfg()


@dataclass
class ScenarioCfg(YAMLWizard):
    """Test scenario configuration."""

    name: str = required_field()
    description: str = required_field()
    marks: list[str] = required_field()
    requirements: Requirements = Requirements()

    def check(self) -> None:
        """Check validity of the configuration."""

        if len(self.name) == 0 or len(self.description) == 0:
            raise ScenarioCfgException("Empty name or description.")


@dataclass
class SimulationScenario(ScenarioCfg):
    """Simulation testing scenario configuration."""

    profile: str = required_field()
    sampling: float = required_field()
    mtu: int = required_field()
    default: SimConfig = required_field()

    # attributes which are not read from configuration but runtime
    filename: Optional[str] = None
    test: Optional[SimTest] = None

    # configuration of individual tests for different simulation scenarios
    sim_general: Optional[list[SimGeneral]] = None
    sim_overload: Optional[list[SimOverload]] = None
    sim_threshold: Optional[list[SimThreshold]] = None

    def check(self) -> None:
        """Check validity of the configuration."""

        super().check()

        if self.default.probe is None:
            raise ScenarioCfgException("Missing default probe configuration.")

        if self.default.generator is None:
            raise ScenarioCfgException("Missing default generator configuration.")

    def get_tests(self, filename: str, name: Optional[str]) -> list[tuple]:
        """
        Get all tests configuration for a specific simulation test scenario.
        If the simulation scenario supports configuration of specific tests, the test configuration is
        added into the main configuration under attribute "test".

        Parameters
        ----------
        filename: str
            Path to the scenario configuration filename. Added to the configuration as "filename" attribute.
            Can be used as test id in case the particular simulation scenario
            doesn't support individual tests configuration.
        name: str, None
            Name of the simulation test scenario.
            If the name is not provided, the "test" attribute of the scenario configuration is set to None
            and filename is used in place of the test id.

        Returns
        -------
        list
            Individual tests for the specified test scenario.
            Tests consists of a tuple (configuration, marks, id).
        """

        if name is None:
            cfg = copy.deepcopy(self)
            cfg.filename = filename
            return [(cfg, cfg.marks, filename.removesuffix(".yml"))]

        if getattr(self, name, None) is None:
            return []

        tests = []
        for test in getattr(self, name):
            if not test.enabled:
                continue

            cfg = copy.deepcopy(self)
            cfg.test = test
            cfg.filename = filename
            tests.append((cfg, cfg.marks + test.marks + [name], test.id))

        return tests

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
            profiles_dir = os.path.join(simulation_tests_dir, "profiles")
            if not os.path.exists(profiles_dir):
                os.mkdir(profiles_dir)
            # add hash of URL to downloaded profile
            hash_url = hashlib.md5(self.profile.encode("utf-8")).hexdigest()
            profile_name = os.path.splitext(os.path.basename(scenario_filename))[0]
            profile_name = f"{profile_name}_{hash_url}.csv"
            profile_path = os.path.join(profiles_dir, profile_name)

            # duplicity - check if the profile has been already downloaded in some previous tests
            csv_list = [f for f in os.listdir(os.path.dirname(profile_path)) if hash_url in f]
            if csv_list:
                return os.path.join(profiles_dir, csv_list[0])

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

    def get_tests(self, filename: str, *_) -> list[tuple]:
        """
        Get validation test scenario.

        Parameters
        ----------
        filename: str
            Used as test id.

        Returns
        -------
        list
            Validation test scenario configuration, marks, id.
        """

        return [(self, self.marks, filename.removesuffix(".yml"))]
