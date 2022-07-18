"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Ipfixcol2 collector reads NetFlow/IPFIX data supplied by exporter and stores it
as FDS file.
"""

import glob
import logging
import os
import pathlib
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from src.collector.fdsdump import Fdsdump
from src.collector.interface import CollectorException, CollectorInterface


class Ipfixcol2(CollectorInterface):
    """Ipfixcol2 collector reads NetFlow/IPFIX data supplied by exporter
    and stores it as FDS file.

    Attributes
    ----------
    _cmd : list of strings
        Arguments passed to the ipfixcol2 process on startup.
    _process : subprocess.Popen
        Ipfixcol2 process.
    _fdsdump : Fdsdump
        Fdsdump instance.
    _input_plugin : str
        Input plugin
    _port : int
        Listening port.
    _work_dir: str
        Working directory.
    """

    NAME = "ipfixcol2"
    CONFIG_FILE = "config.xml"

    # traverse YYYY/MM/DD directory structure
    FDS_FILE = "*/*/*/*.fds"

    def __init__(self, input_plugin="udp", port=4739, work_dir=None):  # pylint: disable=super-init-not-called
        """Initialize the collector.

        Parameters
        ----------
        input_plugin : str, optional
            Input plugin. Only 'udp' and 'tcp' plugins are supported.
        port : int, optional
            Listen for incoming NetFlow/IPFIX data on this port.
        work_dir : str, optional
            Working directory. If not set, temp directory is created.
            Warning: directory is deleted when using 'cleanup' method.

        Raises
        ------
        CollectorException
            Bad parameter values.
            Ipfixcol2 could not be located.
            Base directory could not be created.
        """

        if input_plugin not in ("udp", "tcp"):
            raise CollectorException(f"Only 'tcp' and 'udp' input plugins are supported, not '{input_plugin}'")

        if not isinstance(port, int) or not 0 <= port <= 65535:
            raise CollectorException("Bad port")

        if work_dir is not None:
            try:
                os.makedirs(work_dir, exist_ok=True)
            except Exception as exc:
                raise CollectorException(f"Cannot create working directory '{work_dir}': {exc}") from exc
            self._work_dir = work_dir
        else:
            self._work_dir = tempfile.mkdtemp()

        logging.getLogger().info("Initiating collector: %s", self.NAME)

        ipfixcol2_bin = shutil.which("ipfixcol2")
        if ipfixcol2_bin is None:
            raise CollectorException("Unable to locate or execute ipfixcol2 binary")

        self._cmd = [ipfixcol2_bin, "-c", str(pathlib.Path(self._work_dir, self.CONFIG_FILE))]
        self._process = None
        self._fdsdump = None
        self._input_plugin = input_plugin
        self._port = port

    def _create_xml_config(self):
        """Create XML configuration file for Ipfixcol2 based on startup arguments.

        Raises
        ------
        CollectorException
            Cannot create base directory.
        """

        # For XML structure see:
        # https://github.com/CESNET/ipfixcol2/blob/master/doc/sphinx/configuration.rst

        # pylint: disable=too-many-locals
        root = ET.Element("ipfixcol2")
        input_plugins = ET.SubElement(root, "inputPlugins")
        output_plugins = ET.SubElement(root, "outputPlugins")

        inpt = ET.SubElement(input_plugins, "input")
        inpt_name = ET.SubElement(inpt, "name")
        inpt_plugin = ET.SubElement(inpt, "plugin")
        inpt_params = ET.SubElement(inpt, "params")
        inpt_params_port = ET.SubElement(inpt_params, "localPort")
        ET.SubElement(inpt_params, "localIPAddress")

        outpt = ET.SubElement(output_plugins, "output")
        outpt_name = ET.SubElement(outpt, "name")
        outpt_plugin = ET.SubElement(outpt, "plugin")
        outpt_params = ET.SubElement(outpt, "params")
        outpt_params_storage = ET.SubElement(outpt_params, "storagePath")
        outpt_params_compression = ET.SubElement(outpt_params, "compression")
        outpt_params_dump_interval = ET.SubElement(outpt_params, "dumpInterval")
        outpt_params_dump_interval_time_window = ET.SubElement(outpt_params_dump_interval, "timeWindow")
        outpt_params_dump_interval_align = ET.SubElement(outpt_params_dump_interval, "align")

        inpt_name.text = f"{self._input_plugin} input plugin"
        inpt_plugin.text = f"{self._input_plugin}"
        inpt_params_port.text = f"{self._port}"
        outpt_name.text = "FDS output plugin"
        outpt_plugin.text = "fds"
        outpt_params_storage.text = self._work_dir
        outpt_params_compression.text = "lz4"

        # store all data in one file, do not split it over multiple files
        outpt_params_dump_interval_time_window.text = "999999999"
        outpt_params_dump_interval_align.text = "no"

        tree = ET.ElementTree(root)
        tree.write(str(pathlib.Path(self._work_dir, self.CONFIG_FILE)))

    def start(self):
        """Start ipfixcol2 process.

        Raises
        ------
        CollectorException
            Ipfixcol2 process exited unexpectedly with an error.
        """

        if self._process is not None:
            self.stop()

        self._create_xml_config()

        # pylint: disable=R1732
        self._process = subprocess.Popen(self._cmd, shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        try:
            return_code = self._process.wait(2)
        except subprocess.TimeoutExpired:
            # Probably OK.
            pass
        else:
            if return_code != 0:
                error = self._process.stderr.readline()
                logging.getLogger().error("ipfixcol2 return code: %d, error: %s", return_code, error.decode())
                raise CollectorException("ipfixcol2 startup error")

    def stop(self):
        """Stop ipfixcol2 process."""

        self._process.terminate()
        try:
            self._process.wait(3)
        except TimeoutError:
            self._process.kill()

        while True:
            error = self._process.stderr.readline()
            if len(error) == 0:
                break
            logging.getLogger().error("ipfixcol2 runtime error: %s", error.decode())

        self._process = None

    def cleanup(self):
        """Delete working directory."""

        shutil.rmtree(self._work_dir, ignore_errors=True)

    def get_reader(self):
        """Return flow reader fdsdump.

        Flow reader implements ``__iter__`` and ``__next__`` methods for
        reading flows. Example usages::

            fdsdump = Ipfixcol2().get_reader()
            fdsdump = iter(fdsdump)
            flow = next(fdsdump)
            print(flow)

            # second example
            for flow in fdsdump:
                print(flow)

        Returns
        -------
        Fdsdump
            Flow reader fdsdump.
        """

        # use glob module for paths with wildcards
        fds_file = glob.glob(str(pathlib.Path(self._work_dir, self.FDS_FILE)))
        if len(fds_file) != 1:
            raise CollectorException(f"Located zero or more than one FDS file: {fds_file}")

        return Fdsdump(fds_file[0])
