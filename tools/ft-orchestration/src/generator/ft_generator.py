"""
Author(s): Jan Sobol <sobol@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Connector for ft-generator tool.
"""

import hashlib
import logging
import pickle
import tempfile
import uuid
from dataclasses import dataclass, field
from os import path
from pathlib import Path
from typing import Any, Optional, TextIO, Union

import numpy as np
import pandas as pd
import yaml
from dataclass_wizard import JSONWizard, YAMLWizard
from lbr_testsuite.executable import ExecutableProcessError, Executor, Rsync, Tool
from src.common.tool_is_installed import assert_tool_is_installed


class FtGeneratorException(Exception):
    """Error in FtGenerator connector."""


@dataclass
class FtGeneratorConfig(YAMLWizard, JSONWizard, key_transform="SNAKE"):
    """Class holding PCAP generation settings.
    YAMLWizard used to save object to YAML. JSONWizard provides useful method from_dict.

    Attributes
    ----------
    encapsulation: list[Encapsulation]
        List of encapsulation description and probability of occurrence.
        Several possible encapsulations can be defined.
    ipv4: IP
        Description of IPv4 layer generation.
    ipv6: IP
        Description of IPv6 layer generation.
    mac: Mac, optional
        Definition of range(s) from which MAC addresses are generated.
    packet_size_probabilities: dict[str, float], optional
        A map of values, where key is a definition of an interval in the form of
        <lower bound>-<upper bound>, e.g. 100-200, and value is a probability of
        choosing a value from that interval in the form of a floating point number 0.0.
    max_flow_inter_packet_gap: int, optional
        Specifies the maximum number of seconds two consecutive packets in a flow can be apart.
        In case the constraint could not be fulfilled, the flow will be trimmed resulting in
        a different END_TIME than the provided one. None = no limit.
    """

    @dataclass
    class Encapsulation:
        """Description of encapsulation.

        Attributes
        ----------
        layers: list[Layer]
            List of packet encapsulation layers.
        probability: str
            Probability of occurrence of this encapsulation variant in generated network traffic.
            In form percentage - "5%", "10%", ...
        """

        @dataclass
        class Layer:
            """Single encapsulation layer.

            Attributes
            ----------
            type: str
                One of 'mpls', 'vlan'.
            label: int, optional
                The MPLS label (only when type is 'mpls').
            id: int, optional
                The VLAN id (only when type is 'vlan').
            """

            type: str
            label: Optional[int] = None
            # pylint: disable=invalid-name
            id: Optional[int] = None

        layers: list[Layer]
        probability: str

    @dataclass
    class IP:
        """Representation of IP layer generation description.

        Attributes
        ----------
        fragmentation_probability: str, optional
            The probability a packet will be fragmented.
            In form percentage - "5%", "10%", ...
        min_packet_size_to_fragment: int, optional
            The minimum size of a packet in bytes for a packet to be considered for fragmentation.
        ip_range: Union[list[str], str], optional
            Possible ranges of IP addresses that can be choosen from when generating an address presented
            in the form of an IP address with a prefix length, e.g. "10.0.0.0/8" in case of IPv4 or "ffff::/16"
            in case of IPv6, can be a single value or a list of values.
        """

        fragmentation_probability: Optional[str] = None
        min_packet_size_to_fragment: Optional[int] = None
        ip_range: Optional[Union[list[str], str]] = None

    @dataclass
    class Mac:
        """Definition of range(s) from which MAC addresses are generated.

        Attributes
        ----------
        mac_range: Union[list[str], str]
            Possible ranges of MAC addresses that can be choosen from when generating an address presented
            in the form of an MAC address with a prefix length, e.g. "ab:cd:00:00:00:00/16".
            Can be a single value or a list of values.
        """

        mac_range: Union[list[str], str]

    encapsulation: list[Encapsulation] = field(default_factory=list)
    ipv4: IP = IP()
    ipv6: IP = IP()
    mac: Optional[Mac] = None
    packet_size_probabilities: Optional[dict[str, float]] = None
    max_flow_inter_packet_gap: Optional[int] = None


def _custom_dump(data: Any, stream: TextIO = None, **kwds) -> Optional[str]:
    """
    Serialize a Python object into a YAML stream.
    If stream is None, return the produced string instead.

    None attributes from dicts are removed.
    """

    def remove_none_attrib(node):
        if isinstance(node, dict):
            res = {}
            for key, value in node.items():
                if value is None:
                    continue
                res[key] = remove_none_attrib(value)
            return res
        if isinstance(node, list):
            return list(map(remove_none_attrib, node))
        return node

    return yaml.safe_dump(remove_none_attrib(data), stream, **kwds)


class FtGeneratorCache:
    """Class representing PCAP cache on (remote) machine.

    PCAPs generated with ft-generator are kept on a machine and could be reused
    if generation with the same options is requested.

    Warning: Unknown L4 protocols are skipped during PCAP generation (option --skip-unknown).
    This situation is logged.
    """

    @dataclass(frozen=True)
    class _Key:
        profile_name: str
        profile_hash: str
        config_hash: str

    @dataclass(frozen=True)
    class _Value:
        pcap_filename: str
        pcap_hash: str
        csv_filename: str
        csv_hash: str

    INDEX_FILENAME = "ft-generator-cache.pickle"

    def __init__(self, executor: Executor, cache_dir: str, biflow_export: bool) -> None:
        """Cache init.

        Parameters
        ----------
        executor: Executor
            Executor for command execution.
        cache_dir: str, optional
            Path to save PCAPs on (remote) host.
        biflow_export: bool
            Flag indicating whether the tested probe exports biflows.
            Flag is part of the cache record key (config hash).
        """

        self._executor = executor
        self._rsync = Rsync(executor, data_dir=cache_dir)
        self._cache_dir = cache_dir
        self._tmp_dir = tempfile.mkdtemp()
        self._tmp_index = path.join(self._tmp_dir, self.INDEX_FILENAME)
        self._biflow_export = biflow_export

    def get(self, profile_path: str, config: Optional[FtGeneratorConfig]) -> Optional[tuple[str, str]]:
        """Get PCAP path and CSV report path in cache generated with given profile and config.
        None is returned if PCAP is not found in cache.

        Parameters
        ----------
        profile_path : str
            Path to source network traffic profile.
        config : FtGeneratorConfig, optional
            Ft-generator configuration object.

        Returns
        -------
        tuple[str, str] or None
            Path to PCAP and CSV report in cache if record is found, otherwise None.
        """

        index = self._load()
        key = self._get_key(profile_path, config)
        record = index.get(key, None)

        if not record:
            return None

        pcap_path = path.join(self._cache_dir, record.pcap_filename)
        csv_path = path.join(self._cache_dir, record.csv_filename)

        try:
            Tool(["test", "-f", pcap_path], executor=self._executor).run()
            Tool(["test", "-f", csv_path], executor=self._executor).run()

            if (
                self._compute_hash_on_host(pcap_path) == record.pcap_hash
                and self._compute_hash_on_host(csv_path) == record.csv_hash
            ):
                # PCAP and CSV files were found in cache and their hashes match
                return (pcap_path, csv_path)
        except ExecutableProcessError:
            pass

        # if hashes do not match, delete old files (files will be regenerated)
        Tool(["rm", "-f", pcap_path], executor=self._executor).run()
        Tool(["rm", "-f", csv_path], executor=self._executor).run()

        logging.getLogger().warning(
            "PCAP cache inconsistency, files not found or hashes do not match for profile '%s'. "
            "Files will be regenerated.",
            Path(profile_path).stem,
        )

        return None

    def update(
        self,
        profile_path: str,
        config: Optional[FtGeneratorConfig],
        pcap_path: str,
        csv_path: str,
    ) -> None:
        """Update cache record with given options.

        Parameters
        ----------
        profile_path : str
            Path to source network traffic profile.
        config : FtGeneratorConfig, optional
            Ft-generator configuration object.
        pcap_path : str
            Path to result PCAP.
        csv_path : str
            Path to generator report.
        """

        index = self._load()
        key = self._get_key(profile_path, config)

        index[key] = self._Value(
            pcap_filename=path.basename(pcap_path),
            csv_filename=path.basename(csv_path),
            pcap_hash=self._compute_hash_on_host(pcap_path),
            csv_hash=self._compute_hash_on_host(csv_path),
        )

        self._save(index)

    def generate_unique_paths(self, profile_name: str) -> tuple[str, str]:
        """Get unique name for generated PCAP and CSV.

        Parameters
        ----------
        profile_name : str
            Name of profile for filename prefix.

        Returns
        -------
        (str, str)
            Path to PCAP and CSV in cache.
        """

        basename = profile_name + "_" + uuid.uuid4().hex

        return (
            path.join(self._cache_dir, basename + ".pcap"),
            path.join(self._cache_dir, basename + ".csv"),
        )

    def _load(self) -> dict:
        """Load PCAP cache index from file. Index file is pulled from host.

        Returns
        -------
        dict
            PCAP cache index.
        """

        index_exist_cmd = Tool(
            ["test", "-f", path.join(self._cache_dir, self.INDEX_FILENAME)],
            executor=self._executor,
            failure_verbosity="silent",
        )
        index_exist_cmd.run()
        if index_exist_cmd.returncode() == 0:
            self._rsync.pull_path(self.INDEX_FILENAME, self._tmp_dir)
            with open(self._tmp_index, "rb") as file:
                return pickle.load(file)

        # index does not exist
        return {}

    def _save(self, index: dict) -> None:
        """Save current index to index file. File is pushed to host.

        Parameters
        ----------
        index : dict
            Index to save.
        """

        with open(self._tmp_index, "wb") as file:
            pickle.dump(index, file)

        self._rsync.push_path(self._tmp_index)

    def _get_key(self, profile_path: str, config: Optional[FtGeneratorConfig]) -> _Key:
        """Get index key based on profile and generator options.
        Key consist from basename of profile file, md5sum of profile and md5sum of generator config.

        Parameters
        ----------
        profile_path : str
            Path to source network traffic profile.
        config : FtGeneratorConfig, optional
            Ft-generator configuration object.

        Returns
        -------
        _Key
            Computed key.
        """

        stdout, _ = Tool(["md5sum", profile_path]).run()
        profile_hash = stdout.split()[0]

        if config:
            config_hash = hashlib.md5(config.to_json().encode("utf-8")).hexdigest()
        else:
            config_hash = ""
        config_hash += str(self._biflow_export)

        return self._Key(
            profile_name=path.basename(profile_path),
            profile_hash=profile_hash,
            config_hash=config_hash,
        )

    def _compute_hash_on_host(self, file_path: str) -> str:
        """Compute hash of (remote) file. Used to check generated PCAP and CSV files in cache dir.

        Parameters
        ----------
        file_path : str
            Path to file on (remote) host.

        Returns
        -------
        str
            Hash of given file.

        Raises
        ------
        FtGeneratorException
            If an error occurred while calculating hash using md5sum.
        """

        try:
            stdout, _ = Tool(["md5sum", file_path], executor=self._executor).run()
            return stdout.split()[0]
        except ExecutableProcessError as err:
            raise FtGeneratorException(f"Cannot compute hash for file in PCAP cache: '{file_path}'.") from err


# pylint: disable=too-few-public-methods
class FtGenerator:
    """Connector for ft-generator tool.

    Attributes
    ----------
    CSV_COLUMN_TYPES : dict
        Description of ft-generator output report format.
    CSV_COLUMN_RENAME : dict
        Mapping used to rename columns to prepare CSV output for use with StatisticalModel.
    CSV_OUT_COLUMN_NAMES : dict
        Filtered columns to use with StatisticalModel.
    """

    CSV_COLUMN_TYPES = {
        "SRC_IP": str,
        "DST_IP": str,
        "START_TIME": np.uint64,
        "END_TIME": np.uint64,
        "START_TIME_REV": np.uint64,
        "END_TIME_REV": np.uint64,
        "L3_PROTO": np.uint8,
        "L4_PROTO": np.uint8,
        "SRC_PORT": np.uint16,
        "DST_PORT": np.uint16,
        "PACKETS": np.uint64,
        "BYTES": np.uint64,
        "PACKETS_REV": np.uint64,
        "BYTES_REV": np.uint64,
    }

    CSV_COLUMN_RENAME = {
        "L4_PROTO": "PROTOCOL",
    }

    CSV_OUT_COLUMN_NAMES = [
        "START_TIME",
        "END_TIME",
        "PROTOCOL",
        "SRC_IP",
        "DST_IP",
        "SRC_PORT",
        "DST_PORT",
        "PACKETS",
        "BYTES",
    ]

    def __init__(
        self, executor: Executor, cache_dir: Optional[str] = None, biflow_export: bool = False, verbose: bool = False
    ) -> None:
        """Init ft-generator connector.

        Parameters
        ----------
        executor: Executor
            Executor for command execution.
        cache_dir : str, optional
            Path to save PCAPs on (remote) host. Temp directory is created when value is None.
        biflow_export : bool, optional
            Flag indicating whether the tested probe exports biflows.
            If the probe exports biflows, the START_TIME resp. END_TIME in the generator
            report contains min(START_TIME,START_TIME_REV) resp. max(END_TIME,END_TIME_REV).
        verbose : bool, optional
            If True, logs will collect all debug messages.

        Raises
        ------
        FtGeneratorException
            When ft-generator is not installed on host.
        """

        self._executor = executor
        self._bin = "ft-generator"
        self._local_workdir = tempfile.mkdtemp()
        self._biflow_export = biflow_export
        self._verbose = verbose
        self._tmp_rsync = Rsync(executor)
        self._cache_rsync = Rsync(executor, data_dir=cache_dir)

        if cache_dir:
            self._cache = FtGeneratorCache(executor, cache_dir, biflow_export)
        else:
            self._cache = FtGeneratorCache(executor, self._cache_rsync.get_data_directory(), biflow_export)

        assert_tool_is_installed(self._bin, executor)

    def generate(
        self,
        profile_path: str,
        config: Optional[FtGeneratorConfig] = None,
        log_file: Optional[str] = None,
    ) -> tuple[str, str]:
        """Generate PCAP from profile with given configuration.

        Parameters
        ----------
        profile_path : str
            Path to source network profile.
        config : FtGeneratorConfig, optional
            Ft-generator configuration object.
        log_file : str, optional
            Local path to save ft-generator log.

        Returns
        -------
        (str, str)
            Path to result PCAP file and CSV report file.
            CSV report is modified to use in StatisticalModel (biflows are splitted and columns are filtered).

        Raises
        ------
        FtGeneratorException
            When ft-generator run failed.
        """

        from_cache = self._cache.get(profile_path, config)
        if from_cache:
            return from_cache

        config_arg = ""
        if config:
            local_config = path.join(self._local_workdir, "config.yaml")
            config.to_yaml_file(local_config, encoder=_custom_dump)
            config_path = self._tmp_rsync.push_path(local_config)
            config_arg = f"-c {config_path}"

        pcap_path, csv_path = self._cache.generate_unique_paths(Path(profile_path).stem)

        remote_profile_path = self._tmp_rsync.push_path(profile_path)
        verbosity = ""
        if self._verbose:
            verbosity = "-v"

        # always skip unknown L4 protocols (option --skip-unknown)
        process = Tool(
            f"{self._bin} -p {remote_profile_path} -o {pcap_path} -r {csv_path} {config_arg} --skip-unknown "
            f"{verbosity}",
            executor=self._executor,
        )

        if log_file:
            process.set_outputs(log_file)

        try:
            process.run()
        except ExecutableProcessError as err:
            raise FtGeneratorException(f"Process ft-generator failed, {err}.") from err

        self._process_output(csv_path)
        self._cache.update(profile_path, config, pcap_path, csv_path)

        return (pcap_path, csv_path)

    def _process_output(self, csv_path: str):
        """Process CSV report generated by ft-generator. Output format is readable by StatisticalModel.
        Biflows are split, some columns are omitted and column mapping is used.
        Original CSV file is overwritten by modified one.

        Parameters
        ----------
        csv_path : str
            Path to CSV report on host.

        Raises
        ------
        FtGeneratorException
            When pandas is unable to read source CSV.
        """

        local_csv_path = self._cache_rsync.pull_path(csv_path, self._local_workdir)

        try:
            biflows = pd.read_csv(local_csv_path, engine="pyarrow", dtype=self.CSV_COLUMN_TYPES)
        except Exception as err:
            raise FtGeneratorException("Unable to read ft-generator output CSV.") from err

        reverse_biflows = biflows.copy()
        reverse_biflows["SRC_IP"] = biflows["DST_IP"]
        reverse_biflows["DST_IP"] = biflows["SRC_IP"]
        reverse_biflows["SRC_PORT"] = biflows["DST_PORT"]
        reverse_biflows["DST_PORT"] = biflows["SRC_PORT"]
        reverse_biflows["PACKETS"] = biflows["PACKETS_REV"]
        reverse_biflows["BYTES"] = biflows["BYTES_REV"]

        # timestamp columns need to be swapped for aggregation purposes
        # biflows export aggregation is located below filtering
        reverse_biflows["START_TIME"] = biflows["START_TIME_REV"]
        reverse_biflows["START_TIME_REV"] = biflows["START_TIME"]
        reverse_biflows["END_TIME"] = biflows["END_TIME_REV"]
        reverse_biflows["END_TIME_REV"] = biflows["END_TIME"]
        # reverse packets information to filter biflows
        reverse_biflows["PACKETS_REV"] = biflows["PACKETS"]

        biflows = pd.concat([biflows, reverse_biflows], axis=0, ignore_index=True)
        biflows = biflows.loc[biflows["PACKETS"] > 0]

        # If a probe exports a biflow, both flow directions have the same start and end timestamps.
        # However, in the report by ft-generator, the timestamps are provided separately for both
        # directions. For the purpose of comparing the biflow records, the start and end of both
        # directions need to be adjusted to their minimums and maximums, respectively.
        # Aggregated are only flows which has non-zero reverse direction (to avoid zero timestamps).
        if self._biflow_export:
            condition = biflows["PACKETS_REV"] > 0
            biflows["START_TIME"].loc[condition] = biflows.loc[condition, ["START_TIME", "START_TIME_REV"]].min(axis=1)
            biflows["END_TIME"].loc[condition] = biflows.loc[condition, ["END_TIME", "END_TIME_REV"]].max(axis=1)

        biflows.rename(columns=self.CSV_COLUMN_RENAME, inplace=True)
        biflows.loc[:, self.CSV_OUT_COLUMN_NAMES].to_csv(local_csv_path, index=False)

        self._cache_rsync.push_path(local_csv_path)
