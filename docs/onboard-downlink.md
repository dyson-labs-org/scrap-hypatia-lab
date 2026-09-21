# Onboard feeder-downlink feasibility profile

This profile addresses [issue #30](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/30).
It is a small, unidirectional onboard-source integration proof, not the full
partner ground-station delivery study.

## Integration path

An ns-3 Application is attached to a real node returned by
SatTopology::GetOrbiterNode. It creates one patterned payload, serializes a UDP/IPv4
datagram, and submits it through the satellite device's active
SatOrbiterFeederMac::EnquePacket entry point. The datagram travels through native
feeder LLC encapsulation and queueing, the native SCPC frame scheduler, feeder PHY,
SatChannel propagation, gateway PHY/MAC/LLC, and the gateway's IPv4/UDP socket.

The sink is on the actual gateway node, not a gateway user behind an additional
terrestrial link. Gateway backhaul and delivery to the operator belong to #32.

The pinned SatOrbiterNetDevice::Send and SendFrom methods assert because the
orbiter does not implement the usual upper-layer IP send interface.
SetReceiveCallback is also empty. Consequently this example supplies a narrow
application-to-feeder adapter; it does not install a working bidirectional IP
stack onboard or claim that UDP sockets work directly on the satellite.

The adapter uses public queue APIs and leaves all pinned dependencies unchanged.
It does not invoke a ground receive callback, fabricate an uplink reception, or
replace the channel with a computed delivery time.

## Explicit assumptions

- Both forward and return links use network regeneration.
- The two pinned LEO and GEO constellation fixtures provide topology, satellite
  motion/positions, gateway locations, antenna patterns, and native radio models.
  Their unrelated ground nodes and ISLs remain part of the upstream scaffold.
  No ground traffic generator is installed for this probe, and the selected
  payload travels directly from its originating orbiter to a connected gateway.
- User beam 43 selects the gateway; GetGwSatId selects its associated satellite.
  The adapter chooses a feeder MAC whose native transmit channel registers a
  receiver on that gateway, with matching native MAC/PHY beam IDs and a connected
  gateway address. Physical antenna-beam IDs are not interchangeable with these
  MAC/channel keys. Missing native connectivity aborts setup.
- The source IP 192.0.2.1 is an explicit unidirectional fixture identity in the
  serialized datagram. No return route, authority, plan provisioning, or
  network configuration protocol is implied.
- SatAddressE2ETag and SatMacTag identify the actual feeder MAC and gateway.
  SNS-3's receive path requires SatUplinkInfoTag even though this source has no
  uplink. Its upstream SINR is infinity, meaning no preceding uplink impairment;
  this is metadata, not an extra simulated hop.
- Fixed QPSK 1/2 DVB-S2 normal frames, ACM disabled, 125 MHz allocated SCPC
  bandwidth, 0.20 roll-off, zero spacing, and native 1 microsecond guard time
  make frame serialization independently checkable.
- The small 64–2048 byte test payload fits one normal frame. This is not a
  throughput benchmark, large-product transport, contact scheduler, or proof
  of packet recovery. Product processing and SCRAP authorization are absent.

## Evidence and independent timing check

The example emits machine-readable EVENT records for onboard enqueue, a native
BBFrameTxTrace carrying the probe, and the gateway application's actual receive.
They identify the source node and satellite device, feeder MAC/beam, receiver
node/device, payload bytes, and event timestamps. The receive callback checks
every payload byte, not merely a packet counter. The probe tag correlates traces
and has no delivery behavior.

For the fixed frame profile, the independent checker calculates:

- Symbol rate: 125,000,000 / 1.2 symbols per second.
- Symbols per normal QPSK frame: 64,800 / 2 + 90 + 22 × 36 = 33,282.
- Frame period: 0.0003195072 seconds.
- Native transmitted burst duration: frame period minus the 1 microsecond guard.
- Propagation: transmitter-to-gateway Cartesian distance at transmission divided
  by 299,792,458 metres per second.
- Expected gateway arrival: native frame transmit time + burst duration +
  propagation, within 2 microseconds for event/time rounding.

This compares independently calculated timing with an actual receive timestamp;
the formula is never used to deliver a packet. A separate queue-delay bound of
two normal-frame periods applies to this small, otherwise unloaded probe.

The independent Python checker ignores the executable's passed flag when
assessing trace evidence. Unit fixtures deliberately introduce instantaneous
delivery, missing native egress, duplicate delivery, wrong device identity, and
a relaxed result-supplied tolerance; each must be rejected.

The tx-disabled case disables the actual feeder MAC transmitter before execution.
The application still enqueues once, but no probe frame or ground delivery may
appear. This is a negative path control, not a contact-loss study.

## Build and verify

Use the repository's supported Linux/WSL2 environment:

~~~sh
python3 environment/bootstrap.py
environment/ns3.sh build scrap-onboard-downlink scrap-relay-lifecycle -j 3
python3 -m unittest discover -s tests -v
python3 tests/run_onboard.py --prefix onboard-check-01
python3 tests/run_integration.py --prefix relay-regression-01
~~~

Use fresh prefixes. The onboard suite runs three payload/start-time combinations
on LEO, one GEO control, a disabled-transmitter case, and a same-seed LEO repeat.
It checks both terminal outcomes and event ordering. Existing relay cases remain
a separate regression suite.

One manual run:

~~~sh
python3 environment/smoke.py --program scrap-onboard-downlink \
  --scenario constellation-leo-2-satellites --case normal \
  --payload-bytes 1024 --send-at 1.0 --run-id onboard-manual-01
~~~

The existing runner retains unique run folders, input/source/binary hashes,
dependency revisions, command, seed, toolchain, logs, and failure status.
This profile extends that runner rather than creating a separate execution path.

## Contact extension point and remaining limits

A future contact implementation (#34) should resolve the active feeder MAC and
gateway connection for each scheduled opportunity. The pinned
SatOrbiterMac::StopPeriodicTransmissions / StartPeriodicTransmissions methods
provide a possible queue/scheduler gating point, but both clear queues.
Any contact adapter must therefore define queued/in-flight packet treatment and
preserve resumable product state above those queues. MAC disable is useful for
this negative control; it is not, by itself, geometry-derived contact behavior.

This example sends near the start of a three-second run. It does not validate
handover, changing gateway associations, contact closure, beam compatibility for
arbitrary station locations, or long-duration LEO service. Those require their
own geometry/link and scheduling tests. Bidirectional onboard IP support would
also be a separate extension, not an existing capability established here.

## Pinned source references

All SNS-3 links use the revision in deps.lock:

- [Orbiter device Send/SendFrom limitations](https://github.com/sns3/sns3-satellite/blob/2c0c6d4d7fd1628eda1b923a7e135749513769fa/model/satellite-orbiter-net-device.cc)
- [Feeder MAC enqueue](https://github.com/sns3/sns3-satellite/blob/2c0c6d4d7fd1628eda1b923a7e135749513769fa/model/satellite-orbiter-feeder-mac.cc)
- [Native orbiter frame scheduling and guard time](https://github.com/sns3/sns3-satellite/blob/2c0c6d4d7fd1628eda1b923a7e135749513769fa/model/satellite-orbiter-mac.cc)
- [Frame configuration](https://github.com/sns3/sns3-satellite/blob/2c0c6d4d7fd1628eda1b923a7e135749513769fa/model/satellite-bbframe-conf.cc)
- [Feeder setup and SCPC scheduler](https://github.com/sns3/sns3-satellite/blob/2c0c6d4d7fd1628eda1b923a7e135749513769fa/helper/satellite-orbiter-helper.cc)

## Validation record

Verified on 2026-09-21 UTC in Arch Linux under WSL2 with GNU C++ 16.2.1,
Python 3.14.7, CMake 3.31.6, Ninja 1.11.1.4, and the unchanged deps.lock revisions.
Both native application targets built successfully.

- 14 unit tests passed, including independent-checker rejection tests and
  acceptance of negligible floating-point guard-time formatting differences.
- All six runs of tests/run_onboard.py with prefix onboard-native-20260921 passed.
  The repeated LEO result and event sequence matched exactly at seed/run 1/1.
- All seven runs of tests/run_integration.py with prefix relay-regression-20260921
  passed: the six existing cases and their same-seed normal repeat.
- Disabling the actual feeder transmitter produced one enqueue, zero probe
  frames, and zero receives.

| Fixture | Payload bytes | Enqueue-to-receive delay | Source node/device | Gateway node/device |
| --- | ---: | ---: | --- | --- |
| LEO, start 1.000 s | 1024 | 7.307076 ms | 0 / 0 | 2 / 1 |
| LEO, start 1.013 s | 64 | 7.308479 ms | 0 / 0 | 2 / 1 |
| LEO, start 1.027 s | 2048 | 7.305205 ms | 0 / 0 | 2 / 1 |
| GEO, start 1.000 s | 1024 | 130.394329 ms | 1 / 0 | 2 / 2 |

Every successful transfer delivered one byte-verified payload in one native
frame. The largest absolute difference from the independent receive-time
calculation was below 0.3 ns, inside the predeclared 2 microsecond bound.
The different GEO receiver interface is why channel-to-receiver matching is
necessary; choosing the first connected interface is insufficient.

[Machine-readable evidence](validation/onboard-feasibility.json) retains the
events, results, dependency revisions, runtime source hashes, executable hashes,
and hashes of each complete manifest and console log. Validation was performed
on a working tree based on the recorded commit; the source hashes identify the
tested implementation. Full logs and manifests, including preliminary failed
checks, remain under ignored runs/ and in the accompanying evidence archive.

These measurements establish the scoped native path and its timing. They are
not throughput, contact-availability, or customer-benefit results.
