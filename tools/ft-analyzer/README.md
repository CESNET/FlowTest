# Flowtest Analyzer

Flowtest Analyzer (ft-analyzer) is a library for evaluating the quality of flow records.
The library provides several evaluation models which can be used to determine differences
in between tested data and expected result.

## Validation Model

The purpose of this model is to verify parsing capabilities of a NetFlow/IPFIX probe by identifying
differences in individual flow fields (even custom ones). It is recommended to run the validation model
with small samples where each sample is focused on a specific traffic (TLS 1.3 communication,
DNS AAAA reply, etc.).
Validation model accepts lists of flows resp. biflows and reference biflows from an annotation
file provided by the user. The structure of the annotation file
is described [here](../../testing/validation/README.md).

Validation model requires also flow field definition file which describes individual flow fields
and their attributes. Structure of such file as well as an example
is described [here](../../conf/fields.yml).

The model attempts to find the best mapping between flow records from a NetFlow/IPFIX probe and provided annotation.
A report of the validation process is created with all found differences.
The possible differences can be categorized as follows:
 * missing flow (flow record was part of the annotation but not present in the data from the probe)
 * unexpected flow (flow record was part of the data from the probe but not present in the annotation)
 * missing flow field (a flow field is part of the annotation of a specific flow but present
   in the flow record from the probe)
 * unexpected flow field (a flow field is part of the flow record from the probe but not part of the annotation)
   * only for flow fields of type array
 * unchecked flow fields (a flow field is part of the flow record from the probe but not part of the annotation)
   * unchecked flow fields are not considered errors by default to allow users to create annotation files only
     for flow fields which are object of interest

The report also contains statistics from the validation process.

Example of using the validation model:

```python
import yaml
from ftanalyzer.fields import FieldDatabase
from ftanalyzer.models import ValidationModel
from ftanalyzer.normalizer import Normalizer

# define fields which are supported by the probe
# only the supported fields will be taken into account during evaluation process
SUPPORTED_FIELDS = [
    "src_ip",
    "dst_ip",
    "src_port",
    "dst_port",
    "protocol",
    "ip_version",
    "bytes",
    "packets",
    ...
]

# setup flow fields database
fields = FieldDatabase("path_to_flow_fields_database.yml")

# setup normalizer to convert flows records from the probe and from the annotation
# to a format expected by the validation model
norm = Normalizer(fields)

# read annotation file
with open("path_to_annotation_file.yml", "r", encoding="ascii") as ref_stream:
    test_reference = yaml.safe_load(ref_stream)

# override the default flow fields key if necessary
key = test_reference.get("key", None)
norm.set_key_fmt(key)
# normalize annotation flow fields
norm_references = norm.normalize(test_reference["flows"], True)

# obtain flow fields from the probe (here just simple definition)
flows = [{"src_ip": "127.0.0.1", "dst_ip": "192.168.3.5", ...}, ...]

# normalize flow fields from the probe
norm_flows = norm.normalize(flows)

# setup validation model
forward_key_fmt, _ = fields.get_key_formats(key)
model = ValidationModel(forward_key_fmt, norm_references)

# run validation
report = model.validate(norm_flows, SUPPORTED_FIELDS, {})

# print results
report.print_results()
report.print_flows_stats()
report.print_fields_stats()
```

## Statistical Model

The purpose of the statistical model is to analyze large set of flow records with reference flow records
(provided from tools generating and transmitting network packets - see ft-generator and ft-replay).
This model is extremely useful in finding performance limits of a NetFlow/IPFIX probe in different
network configurations and environments. The input CSV files are expected to contain flow records
with the following fields:

 * START_TIME: time of the first observed packet in the flow (UTC timestamp in milliseconds)
 * END_TIME: time of the last observed packet in the flow (UTC timestamp in milliseconds)
 * PROTOCOL: L4 protocol number defined by IANA (e.g. 6 for TCP, 17 for UDP)
 * SRC_IP: source IP address (IPv4 or IPv6)
 * DST_IP: destination IP address (IPv4 or IPv6)
 * SRC_PORT: source port number (can be 0 if the flow does not contain TCP or UDP protocol)
 * DST_PORT: destination port number (can be 0 if the flow does not contain TCP or UDP protocol)
 * PACKETS: number of transferred packets
 * BYTES: number of transferred bytes (IP headers + payload)

The statistical model is able to merge flows which were split by active timeout configured on the network probe.
The following metrics are taken into account to describe the difference between both flow sets:
 * PACKETS: relative difference in the number of packets across all flows
 * BYTES: relative difference in the number of bytes across all flows
 * FLOWS: relative difference in the number of flows

Every analysis can be done either with the whole set of flows or with a specific subset (called "segment")
which can be specified either by IP subnets or time intervals.

Example of using the statistical model:

```python
from datetime import datetime, timezone
from ftanalyzer.models import (
    SMMetric,
    SMMetricType,
    SMRule,
    SMSubnetSegment,
    SMTimeSegment,
)
from ftanalyzer.models import StatisticalModel as SMod

active_timeout = 300
inactive_timeout = 30
model = SMod("path_to_ipfix_probe_flows.csv", "path_to_reference_flows.csv", (active_timeout, inactive_timeout))
metrics = [
    SMMetric(SMMetricType.FLOWS, 0.1),    # 10% tolerance for the difference in number of flows
]

metrics_segment = [
    SMMetric(SMMetricType.PACKETS, 0),  # 0% tolerance for the difference in number of packets across flows
    SMMetric(SMMetricType.BYTES, 0),    # 0% tolerance for the difference in number of bytes across flows
    SMMetric(SMMetricType.FLOWS, 0),    # 0% tolerance for the difference in number of flows
]

subnet_segment = SMSubnetSegment(source="192.168.187.0/24", dest="212.24.128.0/24", bidir=True)
tend = datetime(2023, 3, 8, 21, 49, 22, 483000, timezone.utc)
time_segment = SMTimeSegment(end=tend)

# Start the evaluation of provided rules.
# If segment is specified, the rule is evaluated only with a subset of the traffic.
report = model.validate([SMRule(metrics_segment, time_segment), SMRule(metrics_segment, subnet_segment), SMRule(metrics)])
report.print_results()

# Example output:
# ANY -> 2023-03-08 21:49:27.962000+00:00	PACKETS	0.0000/0.0000	(29/29)
# ANY -> 2023-03-08 21:49:27.962000+00:00	BYTES	0.0000/0.0000	(2007/2007)
# ANY -> 2023-03-08 21:49:27.962000+00:00	FLOWS	0.0000/0.0000	(7/7)
# 192.168.187.0/24 <-> 212.24.128.0/24	PACKETS	0.0000/0.0000	(90/90)
# 192.168.187.0/24 <-> 212.24.128.0/24	BYTES	0.0000/0.0000	(14201/14201)
# 192.168.187.0/24 <-> 212.24.128.0/24	FLOWS	0.0000/0.0000	(2/2)
# ALL DATA	FLOWS	0.1727/0.1000	(182/220)
```

## Precise Model

The precise model aims to discover specific differences between flow records from an IPFIX probe
and reference flow records. It is an extension of the statistical model mainly for situations in which
network errors (such as packet drops) are not expected. Due to the computational complexity, this model
is not recommended if the number of flow records in the validated set exceeds approximately 100,000
(after subnet or time filter is applied).

The model is able to discover the following differences:
 * missing flows
 * unexpected flows
 * incorrect timestamps
 * incorrect values of packets and bytes

Example of using the precise model:

```python
from datetime import datetime, timezone
from ftanalyzer.models import (
    SMSubnetSegment,
    SMTimeSegment,
)
from ftanalyzer.models import PreciseModel as PMod

active_timeout = 300
inactive_timeout = 30
model = PMod("path_to_ipfix_probe_flows.csv", "path_to_reference_flows.csv", (active_timeout, inactive_timeout))

subnet_segment = SMSubnetSegment(source="192.168.187.0/24", dest="212.24.128.0/24", bidir=True)
tend = datetime(2023, 3, 8, 21, 49, 22, 483000, timezone.utc)
time_segment = SMTimeSegment(end=tend)

report = model.validate_precise([None, subnet_segment, time_segment])
report.print_results()

# Example output:
# ANY -> 2023-03-08 21:49:27.962000+00:00
#   - missing flows
#         1678312182319,1678312188318,17,10.100.40.140,37.186.104.44,42088,123,4,304
#         1678312192320,1678312198319,17,10.100.40.140,37.186.104.44,42088,123,6,708
# 192.168.187.0/24 <-> 212.24.128.0/24
#   - incorrect values of packets / bytes
#         1678312182319,1678312188318,17,192.168.187.1,192.168.187.2,42088,123,5,304 != 1678312182319,1678312188318,17,192.168.187.1,192.168.187.2,42088,123,4,304
# ALL DATA
#   - missing flows
#         1678312182319,1678312188318,17,10.100.40.140,37.186.104.44,42088,123,4,304
#         1678312192320,1678312198319,17,10.100.40.140,37.186.104.44,42088,123,6,708
#   - unexpected flows
#         1678313191320,1678313199819,17,10.100.40.140,37.186.104.44,42088,123,6,708
#         1678312272317,1678312288316,17,10.100.40.140,37.188.104.44,42088,123,4,304
#   - incorrect values of packets / bytes
#         1678312182319,1678312188318,17,192.168.187.1,192.168.187.2,42088,123,5,304 != 1678312182319,1678312188318,17,192.168.187.1,192.168.187.2,42088,123,4,304
```
