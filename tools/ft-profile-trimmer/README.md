# FlowTest Profile Trimmer

FlowTest Profile Trimmer (further only *trimmer*) is a tool for trimming **network profiles**
from **ft-profiler** tool.
Such profiles can contain long-lasting or burst flows that are not suitable for
network replaying on loop.

## Motivation

Problem with profiles created in ft-profiler is that the flow records are stored as
they come from the Probe.
If typical 5-minute slot is used for profiling, it contains long-lasting flows,
which started much sooner than the 5-minute slot.
On the other hand, long-lasting flows started in the 5-minute slot,
which end after the slot are not visible.
To obtain correct 5-minute profile, it needs to process at least two 5-minute slots.
For 10-minute simulation, at least three 5-minute slots are needed, etc.
However when longer interval is used, the obtained profile contains long leading edge
and falling edge, which negatively affects traffic replaying in loop.

Goal is to create a tool, which trims obtained profile data, so the leading/falling edge
does not negatively affect replaying and the 5-minute slot is better represented.

## Algorithm

This tools reads CSV file with anonymous profiles, trims the profiles and saves the trimmed
profiles to output CSV file with the same structure.
When trimming flows, the tools works with so-called main interval (or interval of interest)
and tolerance intervals. There are two tolerance intervals - left and right, to the left of
the main interval and to the right of to the main interval.
The main interval represents the time period for which we want to have the correct number of
packets/bytes in the future transmission of traffic as it would occur on the simulated network.
Tolerance intervals ensure a gradual ramp-up, or gradual reduction of traffic so that the
Probe (or device under test) does not receive a "shock" with a sudden jump in the number of
flows that must be created in the flow cache or exported, due to their termination.

The main interval can be entered by the user:
- By specifying the start and end timestamps (see -s and -e parameters)
- By specifying only the length of the interval (see parameter -m). In this case, the tool
analyzes the entire profile, determines the minimum and maximum timestamp, and determines
the center, which will also be the center of the main interval. The start and
end timestamps are then derived.

The start and end interval parameters -s and -e cannot be combined with the interval length
parameter -m.

## Flows trimming

Flows are trimmed as follows:

- If the start and end timestamps of the flow are in the main interval, the flow is saved
to the output in a completely unchanged form and its processing ends.

- If the flow starts before the start of left tolerance interval and ends before the main
interval, it will be discarded and its processing ends.

- If the flow starts after the main interval and ends after the end of right tolerance
interval, it will be discarded and its processing ends.

- If the flow starts and ends in the left tolerance interval or starts and ends in the
right tolerance interval, it is randomly selected whether the flow is saved or
discarded (50:50 chance). Its processing then ends.

- If the flow starts before the start of the main interval, the flow will be truncated by
uniformly selecting a random start timestamp in the range:

    * Lower limit: max(start of left tolerance interval, start of flow)

    * Upper limit: end of the left tolerance interval

- If the flow ends after the end of the main interval, the flow is truncated by uniformly
selecting a random end timestamp in the range:

    * Lower limit: start of the right tolerance interval

    * Upper limit: min(end of right tolerance interval, end of flow)

- If, after editing the start and end timestamps, the situation occurs that the start and
end timestamps are the same, i.e. the duration is zero, the flow is discarded and its
processing ends. This can exceptionally happen under the assumption that the flow e.g.
ends exactly at the beginning of the main interval and the adjusted start timestamp is
shifted to exactly the end of the left tolerance interval. The same applies to the
right tolerance interval.

- The flow that has reached here is scaled in terms of number of packets/bytes.

## Flows scaling

The principle of scaling the number of packets and bytes takes place separately in
each direction.
The original duration is calculated for the flow before the timestamps are modified.
After modifying the start and/or end timestamp, the duration of the changed flow
will be recalculated.
The ratio of these values is then used to scale the number of packets and bytes.
Values are rounded using the arithmetic method.
During scaling, a situation may arise where the number of packets in the forward or
reverse direction will be zero, but the corresponding number of bytes will not.
In this case, the number of packets (of the given direction) is raised to one, and
the minimum number of bytes is 40.
If the flow will have only one packet in total after modification, the end timestamp
must be modified to match the initial one.

Note: Since the timestamps in the input and output CSV are in milliseconds,
generation of adjusted ranges with millisecond precision is also considered.

## Command line interface

Command line parameters:

- -i FILE, --input FILE
    * (required) path to an input csv file where the network profile is stored
- -o FILE, --output FILE
    * (required) path to an output csv file which has been trimmed
- -t INT, --tolerance INT
    * (required) tolerance interval in seconds for flows before and after main interval. Could be 0.
- -m INT, --main INT
    * main interval in seconds for flow trimming. Must be positive non zero number.
- -s INT, --start INT
    * start time for flow trimming in seconds. Could be positive or negative number.
- -e INT, --end INT
    * end time for flow trimming in seconds. Could be positive or negative number.
- -h, --help
    * show this help message and exit

### CLI Example

Execute with main interval width of 300 seconds and tolerance intervals of 10 seconds on
each side. The start and end of the interval is determined automatically.
```
python3 ./fttrimmer.py -i original.csv -o new.csv -m 300 -t 10
```

Execute with the start time at the 10th second and end time at the 310th second.
Tolerance interval of 5 seconds on each side.
```
python3 ./fttrimmer.py -i original.csv -o new.csv -s 10 -e 310 -t 5
```

## Using ProfileTrimmer class

The tool can be also used as a module. First import the class:
```
from fttrimmer import ProfileTrimmer
```

and then trim the profile using main interval width of 300 seconds and  tolerance
interval of 5 seconds on each side.

```
f = ProfileTrimmer('original.csv', 'new.csv', 5, 300)
```

Usage with start time at the 10th second and end time at the 310th second. This time
using tolerance intervals of 10 seconds.
```
f = ProfileTrimmer('original.csv', 'new.csv', 10, (10, 310))
```

## Statistics

After the trimming, statistics about trimmed profile will be displayed. The statistics includes:

- Number of flows in the resulting profile vs. input profile (+ percentage difference)
- Number of non-altered/altered/discarded flows (+ percentage difference)
- Number of packets, bytes and reverse packets and bytes (+ percentage difference)
