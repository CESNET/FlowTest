"""
Author(s): Tomas Jansky <Tomas.Jansky@progress.com>

Copyright: (C) 2022 Flowmon Networks a.s.
SPDX-License-Identifier: BSD-3-Clause

Flow cache for temporarily storing and manipulating with flows.
"""

import logging
from typing import List, Optional

from ftprofiler.flow import Flow


class FlowCacheException(Exception):
    """Basic exception raised by flow cache."""


class FlowCache:
    """Cache for storing flows in order to join flows into biflows and join flows split by active timeout.

    Attributes
    ----------
    _it : int
        Inactive timeout in milliseconds.
    _at : int
        Active timeout in milliseconds.
    _limit : int
        Maximum number of flows the flow cache can store.
    _now : int
        Maximum end time across all processed flows treated as current time.
    _cache : dict[int, Flow]
        Hash map of flows.
    """

    def __init__(self, inactive_timeout: int, active_timeout: int, limit: int) -> None:
        """Initialize flow cache.

        Parameters
        ----------
        inactive_timeout : int
            Inactive timeout in milliseconds.
        active_timeout : int
            Active timeout in milliseconds.
        limit : int
            Maximum number of flows the flow cache can store.
        """
        self._it = inactive_timeout
        self._at = active_timeout
        self._limit = limit
        self._now = 0
        self._cache = {}

    def add_flow(self, flow: Flow) -> List[Flow]:
        """Add flow to a flow cache. May return older flow in case of hash collision.

        This method attempts to add a new flow into the flow cache.
        If the slot in the cache is already occupied, it checks whether this flow extends flow occupying the slot.
        If not, the older flow is returned and this flow is inserted into the cache.
        If the number of flows in the cache hits the limit after inserting the new flow, the cache is searched for flows
        with end time older than start time of the new flow - 1.5 * value of active timeout. All such flows are returned
        as a list of flows.

        Parameters
        ----------
        flow : Flow
            Flow to be inserted into the flow cache.

        Returns
        -------
        List[Flow]
            List of Flow objects.

        Raises
        ------
        FlowCacheException
            Cache overflow.
        """

        self._now = max(self._now, flow.end_time)

        flow_hash = hash(flow)

        # Hash collision:
        # - the same flow from different direction
        # - the same flow from any direction which extends previous flow (active timeout)
        # - the same flow but much later for it to be extension of the previous flow (happens a lot with ICMP packets)
        # - different flow whatsoever (just hash collision)
        if flow_hash in self._cache:
            orig = self._cache[flow_hash]

            # Check if the new flow can update (extend any of its direction) the flow already in the cache.
            if not orig.update(flow, self._it, self._at):
                # Unable to extended - either same flow but much earlier or hash collision -> remove immediately
                self._cache[flow_hash] = flow
                return [orig]
            return []

        # New flow, it is necessary to check size limit.
        self._cache[flow_hash] = flow
        if len(self._cache) >= self._limit:
            # Remove old flows from the cache based on the start time of this flow.
            # Flows are not strictly ordered based on start time (start time trend of flows is generally increasing),
            # therefore the active timeout is multiplied by a constant here, so we don't remove flows for which
            # extending flow could still arrive.
            flows = self.remove_flows(self._now - 1.5 * self._at)
            if not flows:
                raise FlowCacheException("Flow cache is full and cannot hold all active flows! Increase memory limit.")
            return flows

        return []

    def remove_flows(self, threshold: Optional[int] = None) -> List[Flow]:
        """Remove flows from the flow cache.

        Parameters
        ----------
        threshold : int, None
            Timestamp of the oldest possible flow (based on the flow end time).
            Keep only flows in the flow cache with end time newer than threshold.

        Returns
        -------
        List[Flow]
            List of Flow objects.
        """
        if threshold is None:
            flows = self._cache.values()
            self._cache = {}
            return list(flows)

        flows = {k: v for k, v in self._cache.items() if v.end_time < threshold}
        for k in list(flows.keys()):
            del self._cache[k]

        logging.getLogger().debug("Removed %d flows from the cache", len(flows))
        return list(flows.values())
