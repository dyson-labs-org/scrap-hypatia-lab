# SCRAP SNS-3 Lab

A fresh foundation for an Earth-observation delivery study using SNS-3 packet
simulation. A behavioral relay application is provided; onboard EO and customer experiments remain to be implemented. The repository retains its historical `scrap-hypatia-lab` name.
The previous implementation is preserved by Git history and the recovery tag
`archive/pre-sns3-rebuild-20260913`; none of its code is used by this lab.

## Scope

Evaluate deadline delivery, protocol overhead, and failure recovery under
explicit contact, traffic, and authorization assumptions. This is research
software, not a flight implementation or a customer performance guarantee.
Historical BBB/Jetson measurements are user-reported, not independently verified.

## Build on Linux / WSL2

Requires Python 3 with venv/pip, Git, a C++23 compiler, pkg-config, and libxml2
development headers. CMake and Ninja are installed into an isolated environment.
On Debian 13: `apt-get install git g++ python3-venv pkg-config libxml2-dev`.

```sh
python3 environment/bootstrap.py
environment/ns3.sh build sat-constellation-example test-runner -j 3
python3 environment/smoke.py --run-id upstream-geo-01
```

Bootstrap fetches the exact commits in `deps.lock`, including satellite data. Sparse checkout excludes
the optional 3.4 GB external fading traces; scenarios requiring those traces are
outside this bootstrap profile. Antenna patterns still require about 442 MB.
It verifies existing dependency checkouts and fails on mismatches or modified
tracked source. There is no network simulator fallback. Upstream simulation
output and downloaded dependencies remain under ignored `.deps/`.

See `docs/study-contract.md` for boundaries and `calibration/historical.json`
for the original aggregate measurements. Large experiment outputs belong in
ignored `runs/`; each published result must include its run manifest.

## Verification gates

1. Run `python3 -m unittest discover -s tests -v` for bootstrap safeguards.
2. Build and execute the upstream example above successfully.
3. Build `scrap-relay-lifecycle` and run the six behavioral integration cases.
4. Run paired EO workloads and failure scenarios before reporting comparisons.

The bootstrap is not a SCRAP simulation. No customer result is established by
successfully compiling or running the upstream connectivity example.

The smoke runner creates a new `runs/<run-id>/` directory with console output,
statistics, and a manifest of revisions, input hashes, toolchain, seed, command,
and exit status. Existing run directories are never overwritten. A zero exit
code alone does not establish useful delivery: inspect the traffic statistics.

See `docs/relay-profile.md` for the new application lifecycle and its six integration cases.
