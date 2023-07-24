# Flowtest Generator

Flowtest generator (further only *generator*) is a tool for generating network
traffic based on a provided network profile.

Network profile is an **anonymous list of flow records**. The generator
recreates the flows described by the network profile and attempts to match
their characteristics as closely as possible. The generated traffic is saved in
the form of PCAP files.

The generator supports the generation of flow packets with the following protocols:
* L2: VLAN, MPLS
* L3: IPv4, IPv6
* L4: TCP, UDP, ICMP, ICMPv6

Other protocols are not currently supported. If the input profile file contains
unsupported protocols, the unsupported entries can be skipped (see [Usage](#usage)).

## Table of Contents

* [Input Format](#input-format)
* [Output Format](#output-format)
* [Output Report Format](#output-report-format)
* [Usage](#usage)
* [Configuration](#configuration)
* [Example Configuration](#example-configuration)

## Input Format

Input of the generator is a CSV file containing anonymized biflow records with
following fields:

**Required fields**
 * *START_TIME* - relative time of the first observed packet of the flow in
   milliseconds
 * *END_TIME* - relative time of the last observed packet of the flow in
   milliseconds
 * *L3_PROTO* - either 4 for IPv4 or 6 for IPv6
 * *L4_PROTO* - protocol number assigned by
   *[IANA](https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml)*
 * *SRC_PORT* - source port number (0 in case the L4 protocol is not TCP or UDP)
 * *DST_PORT* - destination port number (0 in case the L4 protocol is not TCP or
   UDP)
 * *PACKETS* - number of packets observed in the forward direction of the flow
 * *BYTES* - number of bytes observed in the forward direction of the flow
 * *PACKETS_REV* - number of packets observed in the opposite direction of the
   flow (0 if the other direction was not found)
 * *BYTES_REV* - number of bytes observed in the opposite direction of the flow
   (0 if the other direction was not found)

**Optional fields**
 * *SRC_IP* - IP address (blank in case a generated IP address should be used
   instead)
 * *DST_IP*  - IP address (blank in case a generated IP address should be used
   instead)

Lines starting with the # character are skipped. First row must contain the
header to specify the order of the fields.

## Output Format

Output of the generator is a PCAP file consisting of packets of the generated
traffic.

## Output Report Format

The generator can also output a report of the generated flows in the form of a
CSV file consisting of the following fields:

 * *SRC_IP* - generated source IPv4 or IPv6 address
 * *DST_IP* - generated destination IPv4 or IPv6 address
 * *START_TIME* - relative time of the first observed packet of the flow in
   milliseconds (may contain decimal point)
 * *END_TIME* - relative time of the last observed packet of the flow in
   milliseconds (may contain decimal point)
 * *L3_PROTO* - either 4 for IPv4 or 6 for IPv6
 * *L4_PROTO* - protocol number assigned by
   *[IANA](https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml)*
 * *SRC_PORT* - source port number (0 in case the L4 protocol is not TCP or UDP)
 * *DST_PORT* - destination port number (0 in case the L4 protocol is not TCP or
   UDP)
 * *PACKETS* - number of packets observed in the forward direction of the flow
 * *BYTES* - number of bytes observed in the forward direction of the flow
 * *PACKETS_REV* - number of packets observed in the opposite direction of the
   flow (0 if the other direction was not present)
 * *BYTES_REV* - number of bytes observed in the opposite direction of the flow
   (0 if the other direction was not present)


## Usage

Start the generator by running `ft-generator <args>`.

For example: `./ft-generator -p profiles.csv -o output.pcap`

Arguments:
```
  --profiles, -p FILE   The flow profiles file in CSV format (required)
  --output, -o FILE     The output PCAP file (required)
  --config, -c FILE     The YAML config file
  --report, -r FILE     The report of the generated flows in CSV format
  --verbose, -v         Verbosity level, specify multiple times for more verbose logging
  --help, -h            Show this help message
  --skip-unknown        Skip unknown/unsupported profile records
  --no-diskspace-check  Do not check available disk space before generating
```

## Configuration

Configuration is provided in form of a YAML file.

Configuration consists of the following sections:

### Encapsulation
The encapsulation section is a list of zero or more encapsulation variants,
where each variant consists of an object in the following format:
* `layers` - the encapsulation layers used in this encapsulation variant, a list
  where each layer consists of
  * `type` - one of `mpls`, `vlan`
  * `label` - the MPLS label _(only when `type` is `mpls`)_
  * `id` - the VLAN id _(only when `type` is `vlan`)_
* `probability` - the probability of using this encapsulation variant, as a
  floating point number 0.0 - 1.0 or a percentage 0% - 100%

### IPv4/IPv6

* `fragmentation_probability` - the probability a packet will be fragmented, as
  a floating point number 0.0 - 1.0 or a percentage 0% - 100% _(default = 0%)_
* `min_packet_size_to_fragment` - the minimum size of a packet in bytes for a
  packet to be considered for fragmentation _(default = 512)_
* `ip_range` - possible ranges of IP addresses that can be chosen from when
  generating an address presented in the form of an IP address with a prefix
  length, e.g. `10.0.0.0/8` in case of IPv4 or `ffff::/16` in case of IPv6, can
  be a single value or a list of values _(default = all addresses)_

*Note: Enabling fragmentation may cause the generated flows that use
fragmentation to contain more bytes and packets than specified in the profiles
file*

### MAC
* `mac_range` - possible ranges of MAC addresses that can be chosen from when
  generating an address presented in the form of an MAC address with a prefix
  length, e.g. `ab:cd:00:00:00:00/16`, can be a single value or a list of values.
  MAC addresses with LSB of the first octet set to 1, which denotes a group MAC
  address, are automatically skipped. _(default = all addresses)_

## Example configuration

```yaml
encapsulation:
  - layers:
    - type: vlan
      id: 10
    - type: vlan
      id: 20
    probability: 20%

  - layers:
    - type: mpls
      label: 5
    probability: 10%

ipv4:
  fragmentation_probability: 20%
  min_packet_size_to_fragment: 512
  ip_range:
    - 10.0.0.0/8
    - 192.168.1.0/24

ipv6:
  ip_range: 0123:4567:89ab:cdef::/64

mac:
  mac_range: aa:aa:aa:00:00:00/24
```

Explanation:

The encapsulation section defines two possible encapsulation scenarios. The
first defined encapsulation variant is using two VLAN layers, one with ID 10 and
another with ID 20. This encapsulation variant will be used on the generated
flow with a 20% chance. The second defined encapsulation variant uses a single
MPLS layer and will be applied when generating flows with a 10% chance. Only one
of the variants will be chosen at a given time. There is also the implicit
scenario that no encapsulation will be used on the flow, which will happen the
remaining 70% of the time.

The IPv4 section defines the probability that any given packet of at least the
specified minimum size may be fragmented on the IPv4 layer. In this case any
packet of size of at least 512 bytes has a 20% chance of being fragmented. The
IP range configuration defines the ranges of IP addresses that the generated IP
addresses can be chosen from.

The IPv6 and MAC section defines the range of addresses the generated addresses
can be chosen from, as in the case of the IPv4 section.
