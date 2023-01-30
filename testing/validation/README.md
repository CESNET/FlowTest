# Validation Tests Annotations

Validation tests focus on verifying the parsing capabilities of a probe.
A validation test usually consists of a one or several packets
which are sent to the probe and its output is compared with an expected result.
Validation tests typically verify whether certain IPFIX fields are present and have correct values.

Every validation test consists of a PCAP file and an annotation file (yaml format).

## Annotation Guidelines

When creating a new validation test, it is recommended that the annotation
should be kept minimalistic, annotating only fields necessary to identify the flow ('key fields')
and fields which must be verified to determine the validation test result.

Key fields directly influence how flow sources are differentiated during the validation,
but do not affect probe behavior in any way.

For instance, annotation of a test designed to verify parsing of the TCP protocol
**should not** include fields from other protocols e.g., MAC addresses, IP time-to-live value
(except fields necessary for the flow identification) and **should** include data
from the TCP header e.g., flags, window size, etc.

The **required_protocols** field in the annotation defines which protocols must the probe
be able to parse (usually also export), so that the test can be executed and properly evaluated.
The following protocols and fields are considered to be present by default
and thus their respective protocol does not need to be stated in the **required_protocols** field.

| Protocol | Fields                     |
|:---------|:---------------------------|
| ETH      | -                          |
| IPv4     | src_ip, dst_ip, protocol   |
| IPv6     | src_ip, dst_ip, protocol   |
| TCP      | src_port, dst_port         |
| UDP      | src_port, dst_port         |
| -        | bytes, packets, ip_version |

For instance, ethernet protocol does not need to be stated in the **required_protocols** field
if only parsing is necessary, but must be stated if the test requires evaluating MAC addresses.

Conforming with this minimalistic approach ensures that any faults related to parsing a specific
field affect only tests focusing on that functionality.

## Annotation Format

If possible, the annotation **should** be done in pairs - *biflows*,
so that both directions of the network communication (*forward* and *reverse*)
are kept together to increase the evaluation potential of the validation test.
Especially, if the probe is capable of exporting biflows.  For this reason, it is sometimes
necessary to categorize which flow fields are supposed to be in which direction.
Therefore, special sections *_forward* and *_reverse* should be utilized for this purpose.

The annotation format allows to define field alternatives (multiple possible values in a single field)
and also group of fields (*subfields*) which can be useful if there are multiple possible values
for the fields in the group. See examples below.

### General Annotation Structure

```yaml
# Comment stating the purpose of this test
test:
  name: Short name of the test
  pcaps: List of PCAP files that should be used (the order of pcaps is respected)
  required_protocols: List of protocols which must be supported by the probe so that this test can be evaluated

key: List of key fields (may be omitted, default - src_ip, dst_ip, protocol, src_port, dst_port)

# List of flows with their flow fields annotation. All fields need to be properly defined in the fields.yml
flows:
  - flow1_field1:      "..."
    flow1_field2:      "..."
  - flow2_field1:      "..."
    flow2_field2:      "..."
```

### Example no. 1 - Simple TCP Test

```yaml
# Check that TCP header is properly parsed
test:
  name: Simple TCP traffic
  pcaps: ["tcp.pcap"]
  required_protocols: ["tcp"]

flows:
  - src_ip:          "192.168.56.101"
    dst_ip:          "192.168.56.102"
    ip_version:      4
    protocol:        6
    bytes:           751
    bytes@rev:       1900
    packets:         6
    packets@rev:     4
    tcp_flags:       0x1a
    tcp_flags@rev:   0x1a
    src_port:        45422
    dst_port:        443
    tcp_options:     0x11e
    tcp_options@rev: 0x11e
    tcp_syn_size:    60
    tcp_syn_ttl:     64
```

### Example no. 2 - ICMP Test with Short Key

```yaml
# Check that ICMPv4 is properly parsed
test:
  name: Simple ICMP traffic
  pcaps: ["icmp.pcap"]
  required_protocols: ["icmp"]

key: [src_ip, dst_ip, protocol]

flows:
  - src_ip:             "192.168.158.139"
    dst_ip:             "174.137.42.77"
    ip_version:         4
    protocol:           1
    bytes:              240
    bytes@rev:          240
    packets:            4
    packets@rev:        4
    icmp_type:          8
    icmp_type@rev:      0
    icmp_code:          0
    icmp_code@rev:      0
    icmp_type_code:     2048
    icmp_type_code@rev: 0
```

### Example no. 3 - VXLAN Parsing

This tests validates that MAC addresses and IP addresses were parsed properly after the VXLAN tunnel.

```yaml
# Check that information from inner Ethernet and IP headers are exported
test:
  name: Traffic in VXLAN tunnel
  pcaps: ["vxlan.pcap"]
  required_protocols: ["eth", "vxlan"]

flows:
  - src_ip:           "10.0.0.1"
    dst_ip:           "10.0.0.2"
    ip_version:        4
    protocol:          1
    bytes:             336
    bytes@rev:         336
    bytes_outer:       536
    bytes_outer@rev:   536
    packets:           4
    packets@rev:       4
    src_mac:           "ba:09:2b:6e:f8:be"
    dst_mac:           "4a:7f:01:3b:a2:71"
    vxlan_id:          123
```

### Example no. 4 - HTTP Parsing with Alternative Field Values

In this example, the `http_content_type` field can contain any of the specified values.

```yaml
# Check that HTTP 500 error response code is parsed properly
test:
  name: HTTP Error 500
  pcaps: ["http_500.pcap"]
  required_protocols: ["http"]

flows:
  - src_ip:      "192.168.0.113"
    dst_ip:      "209.31.22.39"
    ip_version:  4
    protocol:    6
    bytes:       931
    bytes@rev:   4298
    packets:     6
    packets@rev: 6
    src_port:    3541
    dst_port:    80
    http_host:         "www.frys.com"
    http_url:          "/category/Outpost/Notebooks+&+Tablets/View+All+Notebooks?site=sa:Notebook%20Pod:Pod2"
    http_method:       "GET"
    http_method_id:    1
    http_status_code:  500
    http_agent:        "Mozilla/5.0 (Windows; U; Windows NT 6.0; en-US; rv:1.9.1.3) Gecko/20090824 Firefox/3.5.3 (.NET CLR 3.5.30729)"
    http_content_type: ["text/html", "text/html;charset=UTF-8"]
    http_referer:      "http://www.frys.com/template/notebook/"
```

### Example no. 5 - DNS Communication with Special Fields and Subfields

```yaml
# Check that at least one of the IPv4 addresses present in DNS response to type A request is present in the flow
test:
  name: Simple DNS A Request
  pcaps: ["dns_a.pcap"]
  required_protocols: ["dns"]

flows:
  - src_ip:      "192.168.21.89"
    dst_ip:      "192.168.197.92"
    ip_version:  4
    protocol:    17
    bytes:       67
    bytes@rev:   99
    packets:     1
    packets@rev: 1
    src_port:    40980
    dst_port:    53
    dns_id:      14402
    _forward:
      dns_flags:           0x0100
      dns_req_query_type:  1
      dns_req_query_class: 1
      dns_req_query_name:  "newportconceptsla.com"
    _reverse:
      dns_flags:           0x8180
      dns_resp_rcode:      0
      dns_resp_rr:
        - name:            "newportconceptsla.com"
          type:            1
          class:           1
          ttl:             136
          data:            "192.168.73.162"
        - name:            "newportconceptsla.com"
          type:            1
          class:           1
          ttl:             136
          data:            "192.168.215.181"
```
