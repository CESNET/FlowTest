# Flowtest Replay

Flowtest Replay is a high-speed tool designed for replaying input PCAP file with the ability to
replicate and modify packets at the output interface.

The tool offers flexibility in modifying packets during the replay process by enabling users to
alter selected packet content or applying amplification techniques. The traffic can be replayed
in multiple loops, where each loop can be replicated and modified differently. Additionally, if
properly arranged, it is possible to ensure that IP flows in each loop do not overlap.

For performance reasons, all packets from the PCAP file are first prefetched into memory,
preprocessed and divided into several queues according to the IP flow. Once everything is ready,
for each queue, a thread is started that replicates and modifies the traffic according to the
optional configuration and then sends it through the output network interface.

### Limitation

It's important to note that only IP traffic is supported for replay. Packets such as ARP and
others that are not part of IP traffic are not supported and cannot be properly distributed for
modification.

For the tool to work properly, the system being used to replay the traffic must have enough free
operating memory, as the tool will attempt to prefetch all PCAP content.

# Table of Contents

* [Usage](#usage)
* [Replicator](#replicator)
* [Output Plugins](#output-plugins)

# Usage

Start the flowtest replay by running `ft-replay <args>`.

For example: `./ft-replay -i input.pcap -o pcapFile:file=output.pcap`

By default, if no replay speed or modification options are specified, the tool replays the input
PCAP file once at the original speed based on the timestamps of the packets. The packet content
is not replicated or modified.

Arguments:
```
  -i, --input=FILE                Input pcap file (required)
  -o, --output=SPEC               The output plugin specification (required)
  -c, --config=FILE               The replicator configuration file (optional)
  -d, --disable-hw-offload        Disable hardware offloading
  -x, --multiplier=NUMBER         Modify replay speed to a given multiple
  -p, --pps=NUMBER                Replay packets at a given packets/sec
  -M, --mbps=NUMBER               Replay packets at a given Mbps
  -t, --topspeed                  Replay packets as fast as possible
  -v, --vlan-id=NUMBER            Insert VLAN header with the given ID
  -l, --loop=NUMBER               Number of loops over PCAP file. [0 = infinite]
  -n, --no-freeram-check          Disable verification of free RAM resources
  -h, --help                      Show this help message
      --src-mac=MAC               Rewrite all source MAC addresses
      --dst-mac=MAC               Rewrite all destination MAC addresses
```

Detailed description:
```
-x, --multiplier=NUMBER: This parameter is used to modify the replay speed. The NUMBER value
specifies the multiple of the original replay speed. For example, if you specify -x 2.0, the
replay will be performed at twice the original speed.

-p, --pps=NUMBER: This parameter allows you to replay packets at a specific packets per second
(pps) rate. The NUMBER value determines the desired replay speed.

-M, --mbps=NUMBER: This parameter is used to replay packets at a specific speed in megabits per
second (Mbps). The NUMBER value determines the desired replay speed.

-t, --topspeed: This parameter replays packets as fast as possible, ignoring any speed limits.

It's important to note that these parameters are mutually exclusive, meaning that when running
the ft-replay tool, you can only use one of these parameters -x, -p, -M, or -t.

Please keep in mind that the -x, -p, and -M parameters are used to control the replay speed,
while the -t parameter allows for replaying packets as fast as possible without any speed limits.

The -n, --no-freeram-check parameter is an optional argument that disables the free RAM check of
the system during the tool initialization. It should be used with caution and only if you are
certain that there is sufficient free RAM for the replay operation.

-d, --disable-hw-offload: By using this option, you can prevent the tool from utilizing hardware
offloading features if they are available. When hardware offloading is disabled (or unavailable),
the tool will resort to software offloading methods for processing network traffic. Tool supports
the following offloads: IPv4 checksum, TCP checksum, UDP checksum and Rate limiting. Please note
that when the original checksums are incorrect, the updated checksums will also be incorrect.

--src-mac=MAC, --dst-mac=MAC: This option is used to overwrite the source and destination MAC
addresses of packets. The address change is made when the file is being loaded into memory. Note,
however, that the MAC addresses themselves can be independently modified later by the replication
units (see "Replicator" below).
```

# Replicator

The Replicator allows you to replicate and modify packets during the replay process. You can
configure the Replicator using a YAML file to specify how each packet should be replicated and
modified. The Replicator file **must have** *.yaml* suffix.

### **Replicator configuration:**

The Replicator configuration consists of replication units and a loop configuration.

A replication unit defines how an individual packet should be modified. The number of replication
units determines the multiplication rate of the input PCAP file. Each replication unit can modify
specific fields of the packet.

The available modification options for Replication Unit are:

| Replication Unit | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| srcip \| dstip   | Specifies modifications for the source and destination IP addresses. Note: Values are limited to 32 bits. In the case of IPv6 addresses, only the first 4 bytes are edited. <br> - **None** - makes no changes <br> - **addConstant(value)** - add constant number to the IP address <br> - **addCounter(start, step)** - add counter value to the IP address. The counter initial value is \<*start*\> and it is incremented by \<*step*\> on every call. When multiple output queues are used, the addCounter(start, step) function introduces non-determinism in packet modification. Each output queue has its own counter, resulting in varying modifications applied to packets. This enhances the generation of "noise" in network traffic, making each packet's modification unpredictable and more closely resembling real-world scenarios. |
| srcmac \| dstmac | Specifies modifications for the source and destination MAC addresses. <br> - **None** - makes no changes <br> - **value** - set MAC address to this value                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            |
| loopOnly         | Specifies the replication loops in which the replication unit should be enabled. <br> - **All** - The replication unit will be applied in all replication loops. (default) <br> - **[range,range,...]** The replication unit will only be applied in the specified replication loops. (**note:** range = [num, "num" or "num-num"]).                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |

The loop configuration specifies how the IP addresses should be modified during each replication loop.
It allows you to apply additional modifications to the IP addresses on each loop iteration.

The available modification options for the loop configuration are:

| Loop           | Description                                                                                                                                                                                                                                                                                                                                           |
| -------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| srcip \| dstip | Specifies modifications for the source and destination IP addresses within each loop. Note: Values are limited to 32 bits. In the case of IPv6 addresses, only the first 4 bytes are edited. <br> - **None** - makes no changes <br> - **addOffset(value)** - add offset to the IP address. Offset is increased by \<value\> on each replication loop |

#### **Configuration format:**

The Replicator configuration file follows the YAML format:

``` yaml
units:
  - srcip: None | addConstant(Value) | addCounter(start,step)
    dstip: None | addConstant(Value) | addCounter(start,step)
    srcmac: None | Value
    dstmac: None | Value
    loopOnly: All | [range,range,...] (note: range = [num, "num" or "num-num"])

loop:
  srcip: None | addOffset(value)
  dstip: None | addOffset(value)
```

### **Example configuration**

Consider the following example of a replication configuration:

``` yaml
units:
  # Use the original packets without modification
  - srcip:  addConstant(0)         # Same as None
    dstip:  addConstant(0)         # Same as None
    srcmac: None
    dstmac: None
    loopOnly: All

  # Modify packets by incrementing the highest byte of the IP addresses.
  - srcip: addConstant(16777216)  # 256^3
    dstip: addConstant(16777216)  # 256^3
    srcmac: None
    dstmac: AA:BB:CC:DD:EE:FF
    loopOnly: All

  # Generate noise in selected loops (i.e. 0,6,7,8,9,10,13)
  - srcip: addConstant(33554432)   # 2x 256^3
    dstip: addCounter(0, 100)
    srcmac: FF:FF:FF:FF:FF:FF
    dstmac: FF:FF:FF:FF:FF:FF
    loopOnly: [0,6-10,"13"]        # All supported formats

loop:
  # Modify packets each loop by adding +4 to the highest byte of the IP addresses.
  srcip: addOffset(67108864)     # 4 x 256^3
  dstip: addOffset(67108864)     # 4 x 256^3
```

The replication functionality of the tool is best demonstrated on the example of a suitably prepared
PCAP, where all IP addresses of packets are in a fixed range e.g. 1.0.0.0/8. Such a PCAP can be
created by, for example, ft-generator tool. The above configuration consists of 3 replication units
and a behavior configuration for multi-loop playback.

- The loop configuration defines that in each loop, the offset of the source and destination IP
  addresses is increased by +4 in the highest byte of the IP address.
- The first replication unit takes the original packets and does not modify it in any way. Thus,
  the first loop will generate packets with the original IP ranges 1.0.0.0/8 for output. In the
  second loop, however, due to loop offset, it will produce packets in the range 5.0.0.0/8. In
  the third loop, it is 9.0.0.0/8 and so on.
- The second replication unit first modifies the original packets by incrementing the highest byte
  of the IP address. Thus, in the first loop its output will be packets in the range 2.0.0.0/8,
  in the second loop 6.0.0.0/8 and so on. It is important to note here that by adding a second
  replication unit, the amount of traffic generated is doubled compared to the original PCAP, while
  at the same time there is no overlap of IP flows in each loop.
- The third replication unit generates noise in selected loops by modifying both the source and
  destination IP addresses. Thus, the first loop will generate packets with source IP address in
  the range 3.0.0.0/8, in the next selected loop (i.e. loop number 6) 27.0.0.0/8 and so on. Simultaneously,
  the destination IP address is altered by incrementing it with a counter value. This results in a
  varying destination IP address for each packet within the same loop, generating a form of "noise"
  that enhances the diversity and authenticity of the replayed network traffic.

# Output plugins

Flowtest Replay provides various output plugins that can store or forward packets. You need to
select one of the supported plugins.

The configuration format for the output plugins is as follows:

```
pluginName:pluginParam=value,otherPluginParam=value
```

Example:
```
pcapFile:file=replay.pcap,queueCount=8
```

### **pcapFile**
Store packets into PCAP files. A separate file is created for each queue. It serves mainly to
demonstrate how the packets will be edited and transmitted.

| Parameter | Description   |
|---	|---	|
| file | Specifies an absolute path of output PCAP file. <br>(\<file\>.\<queueId\> for multiple queues). [value: filename]  |
| queueCount | Specifies the queue numbers to use. [value: number, default: 1]  |
| burstSize  | Sent packets in bursts of \<size\>. [value: number, default: 1024] |
| packetSize | Maximal size (Bytes) of single packet. [value: number, default: 2048]  |

### **raw**
Send packets through raw socket. Allows transmission through only one output queue. Only suitable
for lower speeds (e.g. 1 Gbps) due to system overhead.

| Parameter | Description   |
|---	|---	|
| ifc | Network interface name to use. [value: name]  |
| burstSize  | Sent packets in bursts of \<size\>. [value: number, default: 1024] |
| packetSize | Maximal size (Bytes) of single packet. [value: number, default: interface MTU]  |

### **xdp**
Send packets over XDP (eXpress Data Path). Advanced system interface allowing zero-copy packet
transfers and multiple output queues. Requires a compatible network card and driver. Suitable for
transmission rates in the order of tens of Gbps.

| Parameter | Description   |
|---	|---	|
| ifc | Network interface name to use. [value: name]  |
| queueCount | Specifies the queue numbers to use. [value: number, default: allAvailable]  |
| burstSize  | Sent packets in bursts of \<size\>. [value: number, default: 64] |
| packetSize | Maximal size (Bytes) of single packet. [value: number (power of 2), default: 2048]  |
| umemSize | Size of UMEM array, used to exchange packets between kernel and user space. [value: number (power of 2), default: 4096]  |
| xskQueueSize | Size of TX and Completion queue, used to transfer UMEM descriptors between kernel and user space. [value: number (power of 2), default: 2048]  |
| zeroCopy | Use Zero Copy mode. [value: bool, default: true]  |
| nativeMode | Use Native driver mode. [value: bool, default: true]  |
| mlx_legacy | Enable support for legacy Mellanox/NVIDIA drivers with shifted zero-copy queues. [value: bool, default: false] |

### **nfb**
Send packets over nfb (*Netcope FPGA Board*). Requires a special network card with compatible
firmware for high-speed playback and hardware offloads. Suitable for speeds of 100 Gbps and above.

| Parameter | Description   |
|---	|---	|
| device | Path to the NFB device to be used. Typically `/dev/nfbX` or `/dev/nfb/by-pci-slot/...`. [value: path]  |
| queueCount | Specifies the queue numbers to use. [value: number, default: allPossible]  |
| burstSize  | Sent packets in bursts of \<size\>. [value: number, default: 64] |
| superPacket | Enable Super Packet feature (merging of small packets into larger ones). <br>[value: no, yes, auto, default: auto (enabled if available)] |
| packetSize | Maximal size (Bytes) of single packet. [value: number, default: MTU, max: 14000] |
