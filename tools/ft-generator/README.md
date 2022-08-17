# Flowtest Generator

Flowtest generator (further only *generator*) is a tool for generating network traffic based on a provided network profile.

Network profile is an **anonymous list of flow records**. The generator replicates the flows described by the network profile and attempts to match their characteristics as closely as possible. The generated traffic is saved in the form of pcap files.

## Table of Contents

* [Input Format](#input-format)
* [Output Format](#output-format)
* [Usage](#usage)

## Input Format

Input of the generator is a CSV file containing anonymized biflow records with following fields:
 * *START_TIME* - relative time of the first observed packet of the flow (in milliseconds) offset from time zero
 * *END_TIME* - relative time of the last observed packet of the flow (in milliseconds) offset from time zero
 * *L3_PROTO* - either 4 for IPv4 or 6 for IPv6
 * *L4_PROTO* - protocol number assigned by *[IANA](https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml)*
 * *SRC_PORT* - source port number (0 in case the L4 protocol is not TCP or UDP)
 * *DST_PORT* - destination port number (0 in case the L4 protocol is not TCP or UDP)
 * *PACKETS* - number of packets observed in a single direction of the flow
 * *BYTES* - number of bytes observed in a single direction of the flow
 * *PACKETS_REV* - number of packets observed in the opposite direction of the flow (0 if the other direction was not found)
 * *BYTES_REV* - number of bytes observed in the opposite direction of the flow (0 if the other direction was not found)

Lines starting with the # character are skipped. First row must contain the header to specify the order of the fields.

## Output Format

Output of the generator is a PCAP file consisting of packets of the generated traffic.

## Usage

Start the generator by running `ft-generator <args>`.

For example: `./ft-generator -p profiles.csv -o output.pcap`

Arguments:
```
  --profiles, -p      The flow profiles file in csv format (required)
  --output, -o        The output pcap file (required)
  --verbose, -v       Verbosity level, specify multiple times for more verbose logging
  --help, -h          Show this help message
```
