"""
Author(s): Tomas Jansky <tomas.jansky@progress.com>

Copyright: (C) 2023 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Configuration of individual test scenarios.
"""

from dataclasses import dataclass, field
from typing import Optional

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


@dataclass
class ValidationScenario(ScenarioCfg):
    """Validation testing scenario configuration."""

    pcap: str = required_field()
    flows: list[dict] = required_field()

    key: Optional[list[str]] = None
    at_least_one: Optional[list[str]] = None
    probe: Optional[ProbeCfg] = ProbeCfg()
