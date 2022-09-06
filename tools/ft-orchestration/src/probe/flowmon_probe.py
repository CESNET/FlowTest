"""
Author(s):  Vojtech Pecen <vojtech.pecen@progress.com>
            Michal Panek <michal.panek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Library for managing Flowmon probe"""

import json
import logging
import shutil
import tempfile
import time

from pathlib import Path

import invoke

from src.probe.interface import ProbeException, ProbeInterface

FLOWMONEXP_BIN = "/usr/bin/flowmonexp5"
QUEUE_SIZE = 22
DPDK_INFO_FILE = "/data/components/dpdk-tools/stats/ifc_map.csv"

PLUGIN_PARAMS = {
    "as-helper": "/etc/flowmon/flowmon-as.txt",
    "l2": "protocols=MPLS#VLAN#MAC,mac-crc=0,vlan-crc=0,mpls-crc=0",
    "tls": "fields=MAIN#CLIENT#CERT#JA3",
    "vxlan": "port=4789,export_vni=1,decapsulation=1",
}

PROTOCOLS_TO_PLUGINS = {
    "mac": "l2",
    "vlan": "l2",
    "mpls": "l2",
    "tcp": "extended_tcp",
    "as": "as-helper",
    "ipv4": "extended_tcp",
    "ipv6": "extended_tcp",
    "dns": "dns",
    "http": "http",
    "tls": "tls",
    "gre": "gre",
    "vxlan": "vxlan",
}

SPECIAL_FIELDS = {"dns_resp_rr": "OneInArray"}

FIELDS = {
    "common": [
        "bytes",
        "packets",
        "protocol",
        "tcp_flags",
        "src_port",
        "dst_port",
        "src_ip",
        "dst_ip",
        "ip_version",
        "icmp_type_code",
    ],
    "l2": [
        "src_mac",
        "dst_mac",
        "vlan_id",
        "vlan_id_inner",
        "mpls_label_1",
        "mpls_label_2",
    ],
    "vxlan": [
        "vxlan_id",
    ],
    "as-helper": [
        "src_asn",
        "dst_asn",
    ],
    "extended_tcp": [
        "tcp_syn_size",
        "tos",
        "ttl",
    ],
    "dns": [
        "dns_id",
        "dns_req_flags",
        "dns_req_query_type",
        "dns_req_query_class",
        "dns_req_query_name",
        "dns_resp_flags",
        "dns_resp_rcode",
        "dns_resp_rr",
        "dns_resp_rr.name",
        "dns_resp_rr.type",
        "dns_resp_rr.class",
        "dns_resp_rr.ttl",
        "dns_resp_rr.data",
    ],
    "http": [
        "http_host",
        "http_url",
        "http_method_id",
        "http_status_code",
        "http_agent",
        "http_content_type",
        "http_referer",
    ],
    "tls": [
        "tls_server_version",
        "tls_cipher_suite",
        "tls_alpn",
        "tls_sni",
        "tls_client_version",
        "tls_issuer_cn",
        "tls_subject_cn",
        "tls_validity_not_before",
        "tls_validity_not_after",
        "tls_public_key_alg",
        "tls_ja3",
    ],
}


class FlowmonProbe(ProbeInterface):
    """Flowmon Probe exporter. It's able to start, stop and terminate flow exporting process
    on a single monitoring interface."""

    def __init__(
        self,
        host,
        target,
        protocols,
        interface,
        input_plugin="rawnetcap",
        active_timeout=300,
        inactive_timeout=30,
    ):
        """Create JSON configuration file for a single exporting process
        based on the provided configuration and actual host server attributes.
        The configuration file is saved on the host server.

        host : src.host.host object
            Initialized host object with deployed probe.

        target : src.probe.ProbeTarget
            Export target where all flow data generated by the probe should be sent.

        protocols : list
            List of networking protocols which the probe should parse and export.

        interface : str
            Network interface where the exporting process should be initiated.

        input_plugin : str, optional
            Input plugin - could be either dpdk or rawnetcap.

        active_timeout : int
            Maximum duration of an ongoing flow before the probe exports it (in seconds).

        inactive_timeout : int
            Maximum duration for which a flow is kept in the probe if no new data updates it (in seconds).
        """

        self._host = host

        try:
            self._host.run("systemctl -q is-active dpdk-controller.service", check_rc=True)
            if input_plugin == "rawnetcap":
                logging.getLogger().error("DPDK controller should not be running when rawnetcap input is used.")
                raise ProbeException("DPDK controller should not be running when rawnetcap input is used.")
        except invoke.exceptions.UnexpectedExit as err:
            if input_plugin == "dpdk":
                logging.getLogger().error("DPDK controller should be running when DPDK input is used.")
                raise ProbeException("DPDK controller should be running when DPDK input is used.") from err

        self._plugins = list({PROTOCOLS_TO_PLUGINS[p] for p in protocols if p in PROTOCOLS_TO_PLUGINS})

        self._interface = interface
        self._pidfile = f"/tmp/tmp_probe_{interface}.pidfile"
        self._settings = {}
        attributes = self._get_appliance_attributes(input_plugin)
        self._set_config(
            QUEUE_SIZE,
            active_timeout,
            inactive_timeout,
            attributes,
        )
        self._set_input(
            input_plugin,
            attributes,
        )
        self._set_plugins()
        self._set_filters(attributes)
        self._set_output(target)
        self._probe_json = f"tmp_probe_{interface}.json"
        self._pid = None

        local_temp_dir = tempfile.mkdtemp()
        local_conf_file = Path(local_temp_dir) / self._probe_json
        try:
            with open(local_conf_file, "w", encoding="utf=8") as file:
                json.dump(self._settings, file, indent=4, separators=(",", ": "))
        except IOError as err:
            logging.getLogger().error("Unable to save probe configuration locally %s", err)
            raise ProbeException(f"Unable to save probe configuration locally : {err}") from err
        except TypeError as err:
            logging.getLogger().error("Unable to create json configuration file %s", err)
            raise ProbeException(f"Unable to create json configuration file : {err}") from err

        self._host.get_storage().push(local_conf_file)
        shutil.rmtree(local_temp_dir)
        self._temporary_dir = self._host.get_storage().get_remote_directory()
        self._probe_json_conf = Path(self._temporary_dir) / self._probe_json

    def _get_appliance_attributes(self, input_plugin):
        """Returns dict of appliance attributes"""

        attributes = {}
        if input_plugin == "rawnetcap":
            cmd = f"cat /sys/class/net/{self._interface}/device/numa_node"
            try:
                attributes["numa_node"] = int(self._host.run(cmd).stdout)
            except invoke.exceptions.UnexpectedExit as err:
                logging.getLogger().error("Interface %s is not present on target host, err : %d", self._interface, err)
                raise ProbeException(f"{self._interface} is not present on target host.") from err
            if attributes["numa_node"] == -1:
                attributes["numa_node"] = 0
            attributes["ifindex"] = int(self._host.run(f"cat /sys/class/net/{self._interface}/ifindex").stdout)
        elif input_plugin == "dpdk":
            attributes = self._read_dpdk_info_file()
        return attributes

    def _read_dpdk_info_file(self):
        """Read configuration details for the dpdk"""

        ifc_map = self._host.run(f"cat {DPDK_INFO_FILE} | grep {self._interface}", check_rc=False).stdout.split()
        if len(ifc_map) == 12:
            dpdk_info = {
                "device": ifc_map[0],
                "driver_name": ifc_map[4],
                "numa_node": int(ifc_map[6]),
                "pmd": ifc_map[8],
                "cores": ifc_map[9],
                "ifindex": int(ifc_map[11]),
            }
            return dpdk_info
        logging.getLogger().error("Unable to find interface %s in %d", self._temporary_dir, DPDK_INFO_FILE)
        raise ProbeException(f"Unable to find interface {self._interface} in {DPDK_INFO_FILE}.")

    def _set_config(self, queue_size, active_timeout, inactive_timeout, attributes):
        """Setup basic parameters for probe configuration"""

        self._settings["OPTIONS"] = "DBUS_MODE=OFF,MPLS_MODE=0,SPLIT_BIFLOWS=1"
        self._settings["PLUGIN-DIR"] = "/usr/lib64/flowmonexp5"
        self._settings["DAEMON"] = True
        self._settings["QUEUE-SIZE"] = int(queue_size)
        self._settings["ACTIVE-TIMEOUT"] = int(active_timeout)
        self._settings["INACTIVE-TIMEOUT"] = int(inactive_timeout)
        self._settings["NUMA-NODE"] = attributes["numa_node"]
        self._settings["USER"] = "flowmon"
        self._settings["VERBOSE"] = "DEBUG"
        self._settings["PID-FILE"] = self._pidfile

    def _set_input(self, input_plugin, attributes):
        """Setup input part of flowmon probe configuration."""

        if input_plugin == "rawnetcap":
            input_parameters = f"device={self._interface},sampling=0,cache-size=32768,mtu=1518"
            self._settings["INPUT"] = {"NAME": input_plugin, "PARAMS": input_parameters}
        elif input_plugin == "dpdk":
            input_parameters = (
                f'name={attributes["driver_name"]},device={attributes["device"]},'
                + f'cores={attributes["cores"]},pmd={attributes["pmd"]},'
                + f"stats=/data/components/dpdk-tools/stats/{self._interface},sampling=0,mtu=1500"
            )
            self._settings["INPUT"] = {"NAME": input_plugin, "PARAMS": input_parameters}

    def _set_plugins(self):
        """Responsible for compiling part of the probe
        configuration consisting of used export plugins"""

        self._settings["PROCESS"] = [{"NAME": item, "PARAMS": PLUGIN_PARAMS.get(item, "")} for item in self._plugins]

    def _set_filters(self, attributes):
        """Compiles probe configuration regarding used filters."""

        ifindex = attributes["ifindex"]
        self._settings["FILTERS"] = [{"NAME": "interface-id", "PARAMS": f"input-id={ifindex},output-id=0"}]

    def _set_output(self, target):
        """Setup the output part of the flowmon probe configuration."""

        self._settings["OUTPUTS"] = [
            {
                "NAME": "ipfix-ng",
                "PARAMS": f"host={target.host},port={target.port},protocol={target.protocol},"
                f"source-id=1,template-refresh-packets=4096,template-refresh-time=600",
                "FILTERS": [],
            }
        ]

    def supported_fields(self):
        """Returns list of IPFIX fields the probe may export in its current configuration.

        Returns
        -------
        List
            Fields which may present in the flows.
        """

        fields_2d = [FIELDS[p] for p in self._plugins + ["common"]]
        return [p for sub in fields_2d for p in sub]

    def get_special_fields(self):
        """Get list of IPFIX fields the probe may export in its current configuration
        and need special evaluation.

        Returns
        -------
        List
            of special fields with way to evaluate them
        """

        fields = self.supported_fields()
        return {name: value for name, value in SPECIAL_FIELDS.items() if name in fields}

    def start(self):
        """Start flowmonexp5 process"""

        logging.getLogger().info("Starting exporter on %s", self._interface)
        cmd = f"{FLOWMONEXP_BIN} {self._probe_json_conf}"
        check_running_cmd = f"ps aux | grep -Ei '[f]lowmonexp5.*{self._interface}'"
        running_processes = self._host.run(check_running_cmd, check_rc=False).stdout
        if len(running_processes) > 0:
            running_pid = int(running_processes.split()[1])
            self._stop_process(running_pid)
            time.sleep(2)
        try:
            self._host.run(f"sudo {cmd}", check_rc=True)
            time.sleep(1)
        except invoke.exceptions.UnexpectedExit as err:
            logging.getLogger().error("Unable to start probe on %s.", self._interface)
            raise ProbeException(f"Unable to start probe on {self._interface}.") from err
        try:
            self._pid = self._host.run(f"cat {self._pidfile}", check_rc=False).stdout
        except ValueError as err:
            logging.getLogger().error("Unable to get pid of probe on %s", self._interface)
            raise ProbeException(f"Unable to get pid of probe on {self._interface}.") from err

    def stop(self):
        """Stop the flowmonexp5 process"""

        logging.getLogger().info("Stopping exporter on %s", self._interface)
        self._stop_process(self._pid)

    def _stop_process(self, pid):
        """Stop exporter process"""

        self._host.run(f"kill -2 {pid}", check_rc=False)
        for _ in range(5):
            if self._host.run(f"ps -p {pid}", check_rc=False):
                return
            time.sleep(1)
        logging.getLogger().warning("Unable to stop exporter process with SIGINT, using SIGKILL.")
        self._host.run(f"kill -9 {pid}", check_rc=False)

    def cleanup(self):
        """Clean any artifacts which were created by the connector or the active probe itself."""

        logging.getLogger().info("Removing remote temporary directory %s", self._temporary_dir)
        try:
            self._host.run(f"rm -f {self._probe_json_conf}", check_rc=False)
        except invoke.exceptions.UnexpectedExit as err:
            logging.getLogger().error("Unable to remove %s", self._probe_json_conf)
            raise ProbeException(f"Unable to remove {self._probe_json_conf}.") from err
