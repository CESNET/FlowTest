# Flowtest Profiler

Flowtest profiler (further only *profiler*) is a tool for creating **network profiles** from a certain time period using netflow data.
Network profile is **anonymous list of flow records** which can be used to replicate network traffic with similar
characteristics as the original traffic which those flow records are based upon.

Network profile is created from flow records originating from a network traffic on a **single network interface**.
Network profile **does not** contain ARP traffic.

## Table of Contents

* [Build and Installation](#build-and-installation)
* [Architecture](#architecture)
* [Output Format](#output-format)
* [Usage](#usage)
* [Flow Readers](#flow-readers)
    * [Nffile Reader](#nffile-reader)
    * [Fdsfile Reader](#fdsfile-reader)
    * [CSVFile Reader](#csvfile-reader)
* [Profile Visualizer](#profile-visualizer)

## Build and Installation

Run `python3 -m build` to build the **ftprofiler** package. The resulting source files (.tar.gz) and wheel (.whl) file
can be found in `dist/` directory.

Preferred installation is done using command `python3 -m pip install <path to the wheel file>`.

## Architecture

![Communication Schema](doc/architecture.png)

Profiler consists of the following components:
* *Core* - initializes and terminates other components, handles exceptions, processes configuration,
           passes flow records from reader to the flow cache and from the flow cache to the writer.
* *Flow Reader* - continuously reads flow records from the system, converts them into internal representation
                  and pass them to the core.
* *Flow Cache* - hash map of flow records useful for temporarily storing incoming flow records to merge
                 unidirectional flows into bidirectional flows and to merge together flows split due to active timeout.
                 Has a defined maximum size to prevent consuming too many of system resources.
* *Profile Writer* - stores biflow records in CSV format into a file.
                     Optionally, the flow records can be compressed using gzip.

## Output Format

Output of the profiler is a CSV file containing anonymized biflow records with following fields:
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

## Usage

Start the profiler by running `ftprofiler <args>`.

Arguments:
```
positional arguments:
  {nffile,fdsfile}      Flow Readers

arguments:
  -h, --help            show this help message and exit
  -V, --version         show program's version number and exit
  -o OUTPUT, --output OUTPUT
                        path to an output file where the profile is to be written
  -g, --gzip            compress output using gzip algorithm
  -a ACTIVE, --active ACTIVE
                        value of the active timeout in seconds (default: 300s)
  -i INACTIVE, --inactive INACTIVE
                        value of the inactive timeout in seconds (default: 30s)
  -m MEMORY, --memory MEMORY
                        limit the maximum number of MiB consumed by the flow cache (default: 2048 MiB)
```

## Flow Readers

Flow readers are capable to acquire flow records from the system by different means.
**Only one** flow reader can be specified when starting the Profiler.

All flow readers must implement interface defined [here](src/ftprofiler/readers/interface.py).

### Nffile Reader

Nffile reader internally starts **nfdump** process which reads files in **nffile** format, which are passed as argument.
The Nffile reader filters out ARP traffic and selects traffic from a single network interface bz SNMP ID
(consists of router IP and interface ID).

Arguments:
```
arguments:
  -h, --help            show this help message and exit
  -c COUNT, --count COUNT
                        limit the number of records read by nfdump (default: 0 - unlimited)
  -i IFCID, --ifcid IFCID
                        interface ID of exporter process (default: 0)
  -M MULTIDIR, --multidir MULTIDIR
                        Specify directories where nffile should be searched for.
                        The actual files are provided in R argument. Nfdump -M argument (man nfdump).
  -r ROUTER, --router ROUTER
                        IP address where exporter process creates flow (default: 127.0.0.1)
  -R READ, --read READ  specify files to be read by nfdump. Nfdump -R argument (man nfdump)
```

### Fdsfile Reader

Fdsfile reader internally starts **fdsdump** process which reads files in **fds** format, which are passed as argument.

Arguments:
```
arguments:
  -h, --help            show this help message and exit
  -c COUNT, --count COUNT
                        limit the number of records read by fdsdump (default: 0 - unlimited)
  -F FILTER, --filter FILTER
                        filter records, specified in bpf-like syntax (see fdsdump -F switch documentation)
  -R READ, --read READ  specify files to be read by fdsdump. Supports glob patterns.
```

### CSVFile Reader

CSVFile reader processes a file in CSV format with the following fields:

```
START_TIME (float unix timestamp in seconds, e.g., 1438603883.553), DURATION (float in seconds, e.g., 10.5),
PROTO (L4), SRC_IP, DST_IP, SRC_PORT, DST_PORT, PACKETS, BYTES
```

Lines starting with char # are skipped. Fields in the header do not need to be in this exact order.
First row must contain the header.

This reader is mainly used for debugging and testing purposes.

Arguments:
```
arguments:
  -h, --help           show this help message and exit
  -f FILE, --file FILE path to a CSV file which should be processed
```

## Profile Visualizer

Profile visualizer is a short helper script to visualize attributes of a network profile in time.
It provides several views:
 * number of packets processed every second
 * number of bits processed every second
 * number of active flows (based on a configurable inactive timeout) per second
 * number of flows which start or end every second

Usage:
```
arguments:
  -h, --help            show this help message and exit
  -V, --version         show program's version number and exit
  -f FORMAT, --format FORMAT
                        type of the output (png/pdf/ps/eps/svg, default: png)
  -i INACTIVE, --inactive INACTIVE
                        value of the inactive timeout in seconds (default: 30)
  -o OUTPUT, --output OUTPUT
                        path to save the visualization
  -p PROFILE, --profile PROFILE
                        path to the profile in CSV format
```

