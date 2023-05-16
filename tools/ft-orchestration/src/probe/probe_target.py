"""
Author(s):  Vojtech Pecen <vojtech.pecen@progress.com>
            Michal Panek <michal.panek@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Class for exporter's targets"""

import ipaddress
import logging
import socket

# pylint: disable=R0903


class ProbeTarget:
    """Probe export target where all flow data must be sent.

    Parameters
    ----------
    host : str
        Host where exported flows will be send. Can be hostname or
        IP address.

    port : int
        Port number where the IPFIX collector is expecting flow data.

    protocol : str
        Flow transport protocol. Allowed options: tcp or udp.

    Raises
    ------
    ValueError
        If host is not valid IP address or hostname.
        Invalid transport protocol is used.
        Invalid port number.
    """

    def __init__(self, host, port, protocol):
        try:
            self.host = ipaddress.ip_address(host)
        except ValueError:
            try:
                self.host = socket.gethostbyname(host)
            except socket.gaierror as err:
                logging.getLogger().error("%s is not valid ip address or hostname", self.host)
                raise ValueError(f"{host} is not a valid ip address or hostname. {err}") from err

        if not isinstance(port, int) or not 0 <= port <= 65535:
            logging.getLogger().error("%s is not a valid port number.", port)
            raise ValueError(f"{port} is not not a valid port number.")

        if protocol not in ("udp", "tcp"):
            logging.getLogger().error(
                "%s is not supported transport protocol, \
                allowed options are udp or tcp",
                protocol,
            )
            raise ValueError(f"Only 'tcp' and 'udp' protocols are supported, not '{protocol}'")

        self.port = port
        self.protocol = protocol
