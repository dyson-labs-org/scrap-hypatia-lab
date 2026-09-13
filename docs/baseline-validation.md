# Native SNS-3 baseline validation

Verified locally on 2026-09-13 UTC in Arch Linux under WSL2, using GNU C++
16.2.1, Python 3.14.7, CMake 3.31.6, Ninja 1.11.1.4, and deps.lock revisions.

- Bootstrap safeguard tests: six passed on Linux and Windows.
- Native constellation example and test runner: built successfully.
- GEO two-satellite ISL scenario: completed, with nonzero received application
  throughput in both directions (3647.7 / 3708.92 kbps forward / return).
- LEO two-satellite scenario: completed, with nonzero received application
  throughput in both directions (4123.68 / 6162.78 kbps forward / return).
- `sat-mobility-test`: all three cases passed.
- `sat-constellation-test`: all four cases passed, including loading a
  351-satellite topology. This is not a constellation-scale performance benchmark.

The example uses its upstream traffic settings and seed/run 1/1. Its throughput
numbers are smoke evidence, not a comparison between equivalent GEO and LEO
workloads and not customer results. Input scenarios define different topologies.
Run manifests, packet traces, and statistics are retained under ignored runs/.

Initial isolated runs failed because SNS-3 locates data before command-line
parsing. The runner now provides the pinned data at that required relative path.
No upstream source was patched. The failed runs remain recorded separately.
