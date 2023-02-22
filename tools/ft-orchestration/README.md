# Flowtest Orchestration Tool

## Production Pytests
Tests that verify the functionality or performance of network probes are run using pytest. There are always 3 components
needed for testing - a **generator**, a **probe** and a **collector** (analyzer). The generator sends test traffic to
the network probe, which acts as a system under test. The collector stores all the information exported
by the probe for further analysis (by models in ft-analyzer package). The orchestration tool uses connectors to control
these components, and can also prepare selected components using Ansible playbooks if needed.

The test scenarios are divided into validation, simulation and performance. Further subdivision is based on the type of
generator used - PCAP file player or replicator. Validation and performance tests use a PCAP file player. Simulation
tests use the replicator's potential to simulate real network traffic.

### Common pytest arguments
The following arguments do not depend on the type of the generator or the type of the test. All arguments are
**mandatory**.

Each object argument (generator, probe, collector) has a structure of the form: `alias:arg1=val1;arg2=val2;...`. Alias
refers to the name of an object in a static configuration that contains more information for the object construction.
Arguments following `:` specify or override how the object is launched. Arguments can be universal for all
implementations of the object (e.g., the input network interface of a probe) or specific to a particular connector
(e.g., the input plugin for the Flowmon probe). **The argument string must be enclosed in quotes if multiple**
**parameters separated by ; are used.**
```
--config-path=CONFIG_PATH   Path to directory with static configuration files - definition of objects.
                            (probes.yaml, authentications.yaml, ...)

--probe=PROBE_ARGS          Probe from the configuration file probes.yml. Use 'alias' to identify specific probe.
                            Optionally, you can select interfaces to use by appending :ifc=if1,if2... to the alias.
                            If not specified, all available interfaces will be used.
                            Any of the connector-specific parameters can be appended after the alias.
                            E.g.: flowmonexp-10G:ifc=eth2 or ipfixprobe-10G:ifc=eth2,eth3;cache_size=64

--collector=COLLECTOR       Collector from the configuration file collectors.yml. Use 'alias' to identify specific
                            collector. Optionally, you can override default export port (4739) and export protocol (tcp)
                            by appending :port={any_port};protocol={tcp/udp} to the alias.
                            Any of connector-specific parameters can be appended.
                            E.g.: validation-col:port=9999;protocol=udp;specific-param=1
```

### Generator of the PcapPlayer type
The generator with PCAP file replay capability is used in validation and performance tests. Adding the following
argument will automatically activate tests that support execution with a PcapPlayer generator. The generator argument
can be added repeatedly or combined with another generator type. Tests will be executed for each generator
separately.
```
--pcap-player=PCAP_PLAYER   Use 'alias' to identify specific generator from the configuration file generators.yml.
                            Optionally, you can add generator parameters after the alias.
                            Standard options are 'vlan' for adding vlan tag to sent packets and 'orig-mac'
                            for disabling rewriting destination mac address.
                            Any of connector-specific parameters can be appended after the alias.
                            E.g.: validation-gen:vlan=90;orig-mac;mtu=3500
```

### Generator of the Replicator type
TBD

### Validation tests
Description and annotation of the validation tests can be found in `flowtest_root/testing/validation`. According to the
YAML files in this folder, the tests are dynamically generated and tagged with the `validation` marker. This can be used
to run validation tests only.
```
pytest ... -m validation
```
Each test is further assigned a set of markers according to the tags in the annotation, which specify the network
protocols being tested (e.g. dns, eth, http, etc.). Therefore, it is possible to filter the tests according to the
protocols they target.
```
pytest ... -m "validation and dns"
pytest ... -m "validation and (dns or http)"
```
Substring filtering (`-k`) can be used to run a specific test. The name or part of the name of the YAML file with the
test description is used by pytest to limit the set of selected tests.
```
pytest ... -m validation -k dns_aaaa.yml
pytest ... -m validation -k fragmented
```

### Examples of usage
Run validation tests. Generator machine and probe are connected via a switch in vlan network 90.
```
pytest --pcap-player=validation-gen:vlan=90 --collector=validation-col --probe=validation-probe:ifc=eth2 \
       --config-path=/home/user/flowtest-configs -m validation
```
When generator machine and probe are connected directly and the mac address does not need to be rewritten.
The MTU of the generator may increase due to some test packets.
```
pytest --pcap-player="validation-gen:orig-mac;mtu=4000" --collector=validation-col --probe=validation-probe:ifc=eth1 \
       --config-path=/home/user/flowtest-configs -m validation
```
For better output readability you can add some pytest arguments.
```
pytest --no-header --disable-warnings --tb=line \
       --pcap-player=validation-gen:vlan=90 --collector=validation-col --probe=validation-probe:ifc=eth2 \
       --config-path=/home/user/flowtest-configs -m validation
```
Or the output can be recorded in an HTML report (`pytest-html` package must be installed).
```
pytest --capture=tee-sys --html=validation_report.html --self-contained-html \
       --pcap-player=validation-gen:vlan=90 --collector=validation-col --probe=validation-probe:ifc=eth2 \
       --config-path=/home/user/flowtest-configs -m validation
```

### FAQ
- Unable to start probe error or collector doesn't receive any data when collector is on another machine than probe.
  - Ipfix export port must be opened on collector machine (default 4739). You can try ansible role in
    `flowtest_root/ansible/roles/firewall` if you use nftables.
