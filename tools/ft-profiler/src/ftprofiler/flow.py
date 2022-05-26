"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Contains implementation of the Flow object. Flow objects are created in readers and temporarily stored in the flow cache
where they can be merged with other flow objects in some cases.
"""


class Flow:
    """Flow object. Holds basic information about a network biflow.

    Contains custom __hash__ and __eq__ method which uses only the following attributes:
    source IP address, destination IP address, source port, destination port, l4 protocol

    Attributes
    ----------
    start_time : int
        Time of the first packet in milliseconds.
    end_time : int
        Time of the last packet in milliseconds.
    key : tuple(str, str, int, int, int)
        Source IP address, destination IP address, source port, destination port, L4 protocol.
    swap : bool
        Information whether source and destination addresses and ports need to be swapped
        when comparing and merging flows.
    packets : int
        Number of packets transmitted in the direction from source to destination.
    bytes : int
        Number of bytes transmitted in the direction from source to destination.
    _rev_packets : int
        Number of packets transmitted in the direction from destination to source.
    _rev_bytes : int
        Number of bytes transmitted in the direction from destination to source.
    _l3 : int
        IP version.
    """

    FLOW_CSV_FORMAT = "START_TIME,END_TIME,L3_PROTO,L4_PROTO,SRC_PORT,DST_PORT,PACKETS,BYTES,PACKETS_REV,BYTES_REV"
    SIZE = 256

    # Define all attributes here, so that we deny __dict__ and __weakref__ creation.
    # Huge memory optimization (10x) and attribute access speed optimization (1.3x).
    __slots__ = (
        "start_time",
        "end_time",
        "key",
        "swap",
        "packets",
        "bytes",
        "_rev_packets",
        "_rev_bytes",
        "_l3",
    )

    def __init__(
        self,
        t_start: int,
        t_end: int,
        proto: int,
        s_addr: str,
        d_addr: str,
        s_port: int,
        d_port: int,
        pkts: int,
        bts: int,
    ) -> None:
        """Initialize flow object.

        Parameters
        ----------
        t_start : int
            Time of the first packet in milliseconds.
        t_end : int
            Time of the last packet in milliseconds.
        proto : int
            L4 protocol number.
        s_addr : str
            Source IP address.
        d_addr : str
            Destination IP address.
        s_port : int
            Source port.
        d_port : int
            Destination port.
        pkts : int
            Number of transmitted packets.
        bts : int
            Number of transmitted bytes.
        """
        self.start_time = t_start
        self.end_time = t_end
        self.packets = pkts
        self.bytes = bts

        if proto not in [6, 17]:
            s_port = 0
            d_port = 0

        if s_addr > d_addr:
            self.swap = True
            self.key = (d_addr, s_addr, d_port, s_port, proto)
        else:
            self.swap = False
            self.key = (s_addr, d_addr, s_port, d_port, proto)

        # Fastest way to distinguish IPv6 from IPv4
        self._l3 = 4
        if ":" in s_addr:
            self._l3 = 6

        self._rev_packets = 0
        self._rev_bytes = 0

    def __str__(self) -> str:
        if self.swap:
            src_port = self.key[3]
            dst_port = self.key[2]
        else:
            src_port = self.key[2]
            dst_port = self.key[3]

        return (
            f"{self.start_time},{self.end_time},{self._l3},"
            f"{self.key[4]},{src_port},{dst_port},{self.packets},"
            f"{self.bytes},{self._rev_packets},{self._rev_bytes}"
        )

    def __hash__(self) -> int:
        """Hash is created from key of the object.

        Returns
        -------
        int
            Hash value of this object.
        """
        return hash(self.key)

    def __eq__(self, flow: "Flow") -> bool:
        """Two flows are considered equal if their keys are equal.

        Parameters
        ----------
        flow : Flow
            Flow to be compared with the current flow.

        Returns
        -------
        bool
            True - Flows are equal, false otherwise.
        """
        return self.key == flow.key

    def update(self, flow: "Flow", inactive_timeout: int, active_timeout: int) -> bool:
        """Attempt to update current flow with data from another flow.

        Flow can be updated by another flow either if the second flow is considered to be the other direction
        of the same flow, or if it is the same flow divided by active timeout.

        Parameters
        ----------
        flow : Flow
            Flow to update the current flow with.
        inactive_timeout : int
            Inactive timeout of the exporting process.
        active_timeout : int
            Active timeout of the exporting process.

        Returns
        -------
        bool
            True - flow updated, false - flow could not be updated.
        """
        # Custom equality check. Filter out hash collisions.
        if self != flow:
            return False

        # Is this the first flow in opposite direction?
        # Other flows from opposite direction must follow rules for active timeout.
        if self._rev_packets == 0 and self._is_reverse(flow, inactive_timeout):
            self._merge(flow)
            return True

        # Check if the duration of the original flow is too short to be the result of splitting by active timeout.
        if self.end_time - self.start_time < active_timeout - inactive_timeout:
            return False

        # Check if the new flow started after an inactive timeout after the end of the original flow.
        # In that case it is a new flow, not an extension of the original flow.
        if self.end_time + inactive_timeout < flow.start_time:
            return False

        # Probably extension of the current flow.
        self._merge(flow)
        return True

    def _is_reverse(self, flow: "Flow", inactive_timeout: int) -> bool:
        """Check if the other flow can be considered the opposite direction of this flow.

        Parameters
        ----------
        flow : Flow
            Flow to be checked.
        inactive_timeout : int
            Inactive timeout of the exporting process.

        Returns
        -------
        bool
            True - flow is most likely just opposite direction of the current flow, false otherwise.
        """
        if self.swap == flow.swap:
            return False

        # The delay between the end of the first flow and start of the new flow is higher than inactive timeout.
        # We do not know which direction started first, so we need to test both options.
        if self.end_time + inactive_timeout < flow.start_time or flow.end_time + inactive_timeout < self.start_time:
            return False

        return True

    def _merge(self, flow: "Flow") -> None:
        """Merge two flows together.

        Parameters
        ----------
        flow : Flow
            Flow to be merged into this flow.
        """
        self.start_time = min(self.start_time, flow.start_time)
        self.end_time = max(self.end_time, flow.end_time)

        if self.swap == flow.swap:
            self.packets += flow.packets
            self.bytes += flow.bytes
        else:
            self._rev_packets += flow.packets
            self._rev_bytes += flow.bytes
