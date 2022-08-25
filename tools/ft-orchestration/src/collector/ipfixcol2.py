"""
Author(s): Dominik Tran <tran@cesnet.cz>

Copyright: (C) 2022 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

Ipfixcol2 collector reads NetFlow/IPFIX data supplied by exporter and stores it
as FDS file.
"""

import logging
import pathlib
import shutil
import tempfile
import time
import xml.etree.ElementTree as ET

from src.collector.fdsdump import Fdsdump
from src.collector.interface import CollectorException, CollectorInterface


class Ipfixcol2(CollectorInterface):
    """Ipfixcol2 collector reads NetFlow/IPFIX data supplied by exporter
    and stores it as FDS file.

    Attributes
    ----------
    _host : Host
        Host class with established connection.
    _cmd : string
        Ipfixcol2 command for startup.
    _process : invoke.runners.Promise
        Representation of Ipfixcol2 process.
    _fdsdump : Fdsdump
        Fdsdump instance.
    _input_plugin : str
        Input plugin
    _port : int
        Listening port.
    _work_dir: str
        Local working directory.
    """

    NAME = "ipfixcol2"
    CONFIG_FILE = "config.xml"

    # traverse YYYY/MM/DD directory structure
    FDS_FILE = "*/*/*/*.fds"

    def __init__(self, host, input_plugin="udp", port=4739):  # pylint: disable=super-init-not-called
        """Initialize the collector.

        Parameters
        ----------
        host : Host
            Host class with established connection.
        input_plugin : str, optional
            Input plugin. Only 'udp' and 'tcp' plugins are supported.
        port : int, optional
            Listen for incoming NetFlow/IPFIX data on this port.

        Raises
        ------
        CollectorException
            Bad parameter values.
            Ipfixcol2 could not be located.
        """

        if host.run("command -v ipfixcol2", check_rc=False).exited != 0:
            logging.getLogger().error("ipfixcol2 is missing")
            raise CollectorException("Unable to locate or execute ipfixcol2 binary")

        if input_plugin not in ("udp", "tcp"):
            raise CollectorException(f"Only 'tcp' and 'udp' input plugins are supported, not '{input_plugin}'")

        if not isinstance(port, int) or not 0 <= port <= 65535:
            raise CollectorException("Bad port")

        logging.getLogger().info("Initiating collector: %s", self.NAME)

        self._host = host
        self._work_dir = tempfile.mkdtemp()
        self._cmd = f"ipfixcol2 -c {pathlib.Path(self._work_dir, self.CONFIG_FILE)}"
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

        # Host class replaces local paths with remote paths in command strings,
        # but paths stored in file must be set for target remote machine
        if self._host.is_local():
            outpt_params_storage.text = self._work_dir
        else:
            outpt_params_storage.text = self._host.get_storage().get_remote_directory()

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
        self._process = self._host.run(self._cmd, asynchronous=True, check_rc=False)
        time.sleep(1)

        runner = self._process.runner
        if runner.process_is_finished:
            res = self._process.join()
            if res.failed:
                # If pty is true, stderr is redirected to stdout
                err = res.stdout if runner.opts["pty"] else res.stderr
                logging.getLogger().error("ipfixcol2 return code: %d, error: %s", res.return_code, err)
                raise CollectorException("ipfixcol2 startup error")

    def stop(self):
        """Stop ipfixcol2 process."""

        if self._process is None:
            return

        self._host.stop(self._process)
        res = self._host.wait_until_finished(self._process)
        runner = self._process.runner

        if res.failed:
            # If pty is true, stderr is redirected to stdout
            # Since stdout could be filled with normal output, print only last 1 line
            err = runner.stdout[-1] if runner.opts["pty"] else runner.stderr
            logging.getLogger().error("ipfixcol2 runtime error: %s, error: %s", res.return_code, err)
            raise CollectorException("ipfixcol2 runtime error")

        self._process = None

    def cleanup(self):
        """Delete working directory."""

        if not self._host.is_local():
            self._host.get_storage().remove_all()

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

        # pylint: disable=protected-access
        if self._fdsdump is not None and self._fdsdump._process is not None:
            self._fdsdump._stop()
        self._fdsdump = None

        if self._host.is_local():
            work_dir = self._work_dir
        else:
            work_dir = self._host.get_storage().get_remote_directory()

        self._fdsdump = Fdsdump(self._host, str(pathlib.Path(work_dir, self.FDS_FILE)))

        return self._fdsdump
