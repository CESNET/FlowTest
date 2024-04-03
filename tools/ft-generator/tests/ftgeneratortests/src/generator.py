"""
Author(s):  Matej Hulák <hulak@cesnet.cz>
            Michal Sedlak <sedlakm@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains Generator class.
"""

import shlex
import subprocess
from pathlib import Path, PurePath
from typing import Optional, Union

from ftgeneratortests.src import FlowCache, GeneratorConfig


class Generator:
    """Wrapper of ft-generator.
    Class is used hold parameters for generator, execute generator and hold paths to crated files.

    Attributes
    ----------
    _generator_file : Path
        Path to generator executable.
    _default_profiles : Path
        Path to files with flow profiles.
    _tmp_dir : Path, optional
        Path to temporary directory, used to stores temporary files.
    _profiles_file : Path, optional
        Path to temporary profiles file.
    _config_file : Path, optional
        Path to configuration file of generator.
    _report_file : Path, optional
        Path to report file, created by generator.
    _pcap_file : Path, optional
        Path to pcap file, created by generator.
    """

    def __init__(
        self,
        generator_file: Optional[Path],
        profiles_file: Path,
        tmp_dir: Path,
    ):
        """Generator class init.
        Validates ft-generator executable and configures paths to files and temporary directory.

        Parameters
        ----------
        generator_file : Optional[Path]
            Path to ft-generator executable, None if ft-generator is installed in system.
        profiles_file : Path
            Path to profiles file.
        tmp_dir : Path
            Path to temporary directory.

        Raises
        ------
        RuntimeError
            Raised when ft-generator file cannot be executed.
        FileNotFoundError
            Raised when generator executable or profiles file cannot be found.
        """

        # Configure generator file
        if generator_file is None:
            self._generator_file = self.validate_executable("ft-generator")
            if not self._generator_file:
                raise RuntimeError(
                    "Generator::__init__(): Ft-generator not installed on system. Use --executable argument."
                )
        else:
            self._generator_file = Path(generator_file).absolute()
            if not generator_file.exists():
                raise FileNotFoundError("Ft-generator executable not found, path:", self._generator_file)
            if not self.validate_executable(self._generator_file.as_posix()):
                raise RuntimeError(
                    "Generator::__init__(): Ft-generator file cannot be executed. Maybe insufficient privilege."
                )

        # Configure profiles file
        self._default_profiles = Path(profiles_file).absolute()
        if not self._default_profiles.exists():
            raise FileNotFoundError(self._default_profiles)

        # Configure tmp folder
        self._tmp_dir = Path(tmp_dir).absolute()
        if not self._tmp_dir.exists():
            self._tmp_dir.mkdir()

        self._profiles_file = self._tmp_dir / "profiles.csv"
        self._config_file = self._tmp_dir / "config.yaml"
        self._report_file = self._tmp_dir / "report.csv"
        self._pcap_file = self._tmp_dir / "pcap.pcap"
        self._stdout_file = self._tmp_dir / "stdout.txt"
        self._stderr_file = self._tmp_dir / "stderr.txt"
        self._cmdline_file = self._tmp_dir / "cmdline.txt"

    def validate_executable(self, executable: str) -> Union[Path, None]:
        """Validates executable, and return absolute path to executable.

        Parameters
        ----------
        exec : str
            Name or path to executable.

        Returns
        -------
        Union[Path, None]
            If executable valid, returns absolute path to it, otherwise None.
        """

        cmd = "command -v " + shlex.quote(executable)
        ret = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, check=False)

        if ret.returncode == 0 and len(ret.stdout) != 0:
            return Path(ret.stdout.decode("utf-8").strip())

        return None

    def prepare_profiles(self, provided_profiles: Union[FlowCache, str, None]):
        """Function prepares profiles file for ft-generator.
        Profiles can be provided as Path to file, FlowCache or None, if default profiles are requested.
        In all cases, function created new temporary profiles file, which will be used by generator.

        If Path provided, profiles will be copied.
        If FlowCache provided, profiles are converted to csv and written to file.
        If str provided, content is written to file.
        If None provided, default ones will be used.

        Parameters
        ----------
        provided_profiles : Union[FlowCache, str, None]
            Profiles to be used, if None, default ones are used.

        Raises
        ------
        FileNotFoundError
            Raised when provided profiles file cannot be found.
        TypeError
            Raised when incorrect parameter type.
        """

        self._profiles_file.unlink(True)

        if provided_profiles is None:
            self._profiles_file.write_text(self._default_profiles.read_text(encoding="utf-8"))
        elif isinstance(provided_profiles, str):
            self._profiles_file.write_text(provided_profiles)
        elif isinstance(provided_profiles, FlowCache):
            provided_profiles.to_csv_profile(self._profiles_file)
        else:
            raise TypeError("Generator::prepare_profiles(): Type of provided profiles not supported.")

    def prepare_config(self, provided_config: Union[GeneratorConfig, Path, None] = None):
        """Function prepares configuration for ft-generator.
        Configuration can be path to configuration file, GeneratorConfig or None if default configuration is requested.
        In all cases, function created new temporary configuration file, which will be used by generator.

        If Path provided, configuration is copied.
        If GeneratorConfig provided, content is written to file via write_to_file().
        If None provided, function creates GeneratorConfig with its default attributes.

        Parameters
        ----------
        provided_config : Union[GeneratorConfig, Path, None], optional
            Configuration to be used to execute generator, by default None

        Raises
        ------
        FileNotFoundError
            Raised when provided config file cannot be found.
        TypeError
            Raised when incorrect parameter type.
        """

        self._config_file.unlink(True)

        if provided_config is None:
            config = GeneratorConfig()
            config.write_to_file(self._config_file)
        elif isinstance(provided_config, PurePath):
            if not provided_config.exists():
                raise FileNotFoundError("Generator::prepare_config(): Provided config file not found.")
            self._config_file.write_text(provided_config.read_text())
        elif isinstance(provided_config, GeneratorConfig):
            provided_config.write_to_file(self._config_file)
        else:
            raise TypeError("Generator::prepare_config(): Type of provided config not supported.")

    def get_cmd_args(self) -> str:
        """Function prepares command arguments to execute generator.
        Arguments are generated based on current parameters if Generator instance.

        Returns
        -------
        str
            Command line arguments to execute generator.
        """

        return [
            self._generator_file.as_posix(),
            "--profiles",
            self._profiles_file.as_posix(),
            "--config",
            self._config_file.as_posix(),
            "--report",
            self._report_file.as_posix(),
            "--output",
            self._pcap_file.as_posix(),
            "-v",
        ]

    def generate(
        self,
        generator_config: Union[GeneratorConfig, Path, None] = None,
        profiles: Union[FlowCache, str, None] = None,
    ) -> list[Path, Path]:
        """Function will execute generator with provided configuration.
        Configuration can be providede as Path to file, GeneratorConfig or None if default configuration is requested.
        Profiles can be provided as Path to file, FlowCache or None, if default profiles are requested.
        Pcap and report files are created.

        Parameters
        ----------
        generator_config : Union[GeneratorConfig, Path, None], optional
            Configuration to be used to execute generator, by default None
        profiles: Union[FlowCache, str, None], optional
            Profiles to be used, if None, default ones are used.

        Returns
        -------
        list[Path, Path]
            Path to created pcap and report file.
        """

        self.clear()

        self.prepare_profiles(profiles)
        self.prepare_config(generator_config)

        cmdline = self.get_cmd_args()
        self._cmdline_file.write_text(shlex.join(cmdline))

        with open(self._stdout_file, "wb") as stdout, open(self._stderr_file, "wb") as stderr:
            subprocess.run(cmdline, stdout=stdout, stderr=stderr, check=True)

        return [self._pcap_file, self._report_file]

    def clear(self):
        """Removes temporary files."""
        self._profiles_file.unlink(True)
        self._config_file.unlink(True)
        self._report_file.unlink(True)
        self._pcap_file.unlink(True)
        self._stdout_file.unlink(True)
        self._stderr_file.unlink(True)
        self._cmdline_file.unlink(True)
