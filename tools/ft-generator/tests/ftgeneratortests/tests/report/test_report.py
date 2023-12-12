"""
Author(s): Matej Hulák <hulak@cesnet.cz>

Copyright: (C) 2023 CESNET, z.s.p.o.
SPDX-License-Identifier: BSD-3-Clause

File contains tests for ft-generator module, which compare profile file with report file.
"""

import math
from collections import defaultdict
from pathlib import Path
from typing import Optional, Union

from ftgeneratortests.src import (
    REPORT_REL_TOLERANCE,
    TIMING_REL_TOLERANCE,
    Flow,
    Generator,
    GeneratorConfig,
    parse_profiles,
    parse_report,
)


def search_profile_in_list(profile: Flow, flow_list: list) -> Union[Flow, None]:
    """Function searches for given flow in list.
    List must contain flows with same l3 and l4 protocols.

    Parameters
    ----------
    profile : Flow
        Flow which should be searched.
    flow_list : list
        List of flow to search in.
        Must contain flows with same l3 and l4 protocols.

    Returns
    -------
    Union[Flow, None]
        Corresponding flow from list, None otherwise.
    """

    if len(flow_list) == 0:
        return None

    for flow in flow_list:
        if all(
            [
                flow.src_port in (0, profile.src_port),
                flow.dst_port in (0, profile.dst_port),
                math.isclose(profile.start_time, flow.start_time, rel_tol=TIMING_REL_TOLERANCE),
                math.isclose(profile.end_time, flow.end_time, rel_tol=TIMING_REL_TOLERANCE),
                math.isclose(profile.packets, flow.packets, rel_tol=REPORT_REL_TOLERANCE),
                math.isclose(profile.bytes, flow.bytes, rel_tol=REPORT_REL_TOLERANCE),
                math.isclose(profile.packets_rev, flow.packets_rev, rel_tol=REPORT_REL_TOLERANCE),
                math.isclose(profile.bytes_rev, flow.bytes_rev, rel_tol=REPORT_REL_TOLERANCE),
            ]
        ):
            return flow

    return None


def test_report(ft_generator: Generator, profiles: Path, custom_config: Optional[Path]):
    """Test verifies if generated flows correspond to profiles.
    Test compares provided profile file with report file generated by ft-generator.
    Note that test will ignore source and destination ports, if set to 0 by ft-generator.

    Parameters
    ----------
    ft_generator : Generator
        Instance of Generator with configured paths.
    profiles : Path
        Path to profiles files.
    custom_config : Optional[Path]
        Path to custom configuration file for ft-generator.
    """

    if custom_config:
        config = GeneratorConfig.read_from_file(custom_config)
    else:
        config = GeneratorConfig()

    _, report_file = ft_generator.generate(config)

    profiles = parse_profiles(profiles)
    report = parse_report(report_file)

    assert len(profiles) == len(report)

    # Sort flows from report by l3 and l4 protocols
    report_dict = defaultdict(list)
    for value in report.values():
        report_dict[hash((value.l3_proto, value.l4_proto))].append(value)

    for profile in profiles.values():
        prof_key = hash((profile.l3_proto, profile.l4_proto))
        found = search_profile_in_list(profile, report_dict[prof_key])
        assert found is not None
        # Remove flow from report, so its not matched multiple times
        report_dict[prof_key].remove(found)

    # Check for unused flows in report
    for flow_list in report_dict.values():
        assert len(flow_list) == 0