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
* L7: HTTP, DNS

Other protocols are not currently supported. If the input profile file contains
unsupported protocols, the unsupported entries can be skipped (see [Usage](#usage)).

## Table of Contents

* [Input Format](#input-format)
* [Output Format](#output-format)
* [Output Report Format](#output-report-format)
* [Usage](#usage)
* [Configuration](#configuration)
* [Example Configuration](#example-configuration)
* [Specifics of Generated Traffic](#specifics-of-generated-traffic)

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
 * *START_TIME* - relative time of the first observed packet in forward
   direction in milliseconds (may contain decimal point)
 * *END_TIME* - relative time of the last observed packet in forward direction
   in milliseconds (may contain decimal point)
 * *START_TIME_REV* - relative time of the first observed packet in reverse
   direction in milliseconds (may contain decimal point)
 * *END_TIME_REV* - relative time of the last observed packet in reverse
   direction of the flow in milliseconds (may contain decimal point)
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
  --seed VALUE          The random generator seed
  --skip-unknown        Skip unknown/unsupported profile records
  --no-diskspace-check  Do not check available disk space before generating
  --no-collision-check  Do not check for flow collisions caused by address reuse
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

### Packet size probabilities
* A map of values, where key is a definition of an interval in the form of
`<lower bound>-<upper bound>`, e.g. `100-200`, and value is a probability of
choosing a value from that interval in the form of a floating point number 0.0
- 1.0 or a percentage 0% - 100%. Both bounds of the interval are inclusive. If
the total sum of probabilities doesn't add up to 1.0 it will be automatically
normalized.

_default:_
```yaml
packet_size_probabilities:
  63-79: 0.2824
  80-159: 0.073
  160-319: 0.0115
  320-639: 0.012
  640-1279: 0.0092
  1280-1500: 0.6119
```

### Payload

Specifies options for payload generation

* `enabled_protocols` - A list of protocols for which valid protocol traffic
  will be generated. Possible options: `http`, `dns`, `tls`  _(default = all supported
  protocols)_

### Additional options

Additionaly, the following options can be specified in the top level configuration section:

* `max_flow_inter_packet_gap` - Specifies the maximum number of seconds two
consecutive packets in a flow can be apart. In case the constraint could not
be fulfilled, **the flow will be trimmed** resulting in a different END_TIME than
the provided one. _(default = no limit)_

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

packet_size_probabilities:
  63-79: 0.2824
  80-159: 0.073
  160-319: 0.0115
  320-639: 0.012
  640-1279: 0.0092
  1280-1500: 0.6119

max_flow_inter_packet_gap: 30

payload:
    enabled_protocols: [http]
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

The packet size probability section defines the possible sizes of generated
packets. The chance of generating a packet with a size between 63 and 79
inclusive is 28.24%, between 80 and 159 inclusive 7.3%, etc..

The max flow inter packet gap option specifies the maximum number of seconds two
consecutive packets in a flow can be apart. In this case, no two packets in a
flow will be more than 30 seconds apart.

The payload section specifies options of payload generation, or in other words,
the application level layers. The `enabled_protocols` option allows us to
enable and disable individual payload generators. If a payload generator is
disabled, random data is generated instead. In this case, we enable the
generation of HTTP payload for flows that are considered HTTP traffic (i.e. TCP
and port 80 or 8080).

## Specifics of Generated Traffic

This section describes the traffic generated for the supported L7 protocols.

For protocols that are not in the list of supported protocols above, or in case
the protocol has been disabled in the payload section of the configuration, a
random payload is generated instead.

### HTTP

HTTP traffic is generated for flows using port 80 or 8080 over TCP.

Generated HTTP traffic consists of HTTP/1.1 messages using the GET and POST
methods. The method and headers are selected based on heursitics to best fit
the characteristic of the flow. The body of the HTTP message is randomly
generated.

The following headers are supported:
 - Accept
 - Accept-Encoding
 - Cache-Control
 - Connection
 - Content-Length
 - Content-Type
 - Cookie
 - Host
 - Server
 - User-Agent

Whether a specific optional header is present in the HTTP message is determined
by random chance and packet size. Header values are typically randomly chosen
from a pool of commonly used values. In case of the `Host` header, a
human-readable domain name is generated consisting of English nouns. In case of
the `Cookie` header, a random string of alphanumeric characters is generated.

### DNS

DNS traffic is generated for flows using port 53 over UDP.

The queried domain is randomly generated from a list of English nouns ensuring
human readability. The record type of the query and response is chosen using a
heuristic approach based on size of the packets. A response may contain
multiple resource records where applicable. Domain name compression may also be
used if deemed viable by the heuristics. EDNS is automatically used when
required according to the standard, i.e. for packet sizes larger than 512
bytes.

The following record types are supported:
 - A
 - AAAA
 - CNAME
 - TXT

_Note:_ As the size of a DNS response depends on the size of a DNS query,
it is not possible to generate valid DNS traffic for all possible combinations
of packet sizes. In such cases, random payload will be generated instead. This
prioritizes generating traffic that is more accurate to the specified profile
over always generating application level payload corresponding to the specified
port, as even in actual traffic the usage of a certain port does not guarantee
usage of the specific protocol.

