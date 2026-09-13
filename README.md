# SCRAP Satellite Simulation Lab — SNS-3

This repository studies how SCRAP authorization and coordination affect satellite
service workflows, starting with Earth-observation product delivery. The aim is
to build reproducible evidence about delivery deadlines, protocol overhead, and
recovery when communications or requests fail.

**The simulation engine has changed from Hypatia to SNS-3, built on ns-3.** The
previous implementation was removed and the lab was rebuilt from scratch. The
repository keeps the name `scrap-hypatia-lab`, but Hypatia is no longer a dependency.
Git history and the tag `archive/pre-sns3-rebuild-20260913` preserve the old version.

## Purpose of the study

The intended customer study follows a captured observation product through
authorization, transfer, processing, delivery, and receipt. It will measure:

- Products delivered before their deadlines and reasons for missed deadlines.
- Delivery latency and authorization, transfer, and receipt overhead.
- Use of partner receiving and processing resources.
- Retry behavior, duplicate execution, and unresolved requests.

Comparisons must separate the benefit of additional resources from the benefit
of SCRAP: existing customer resources, expanded resources with conventional
authorization, and those same expanded resources with SCRAP. Conventional
authorization includes reasonable advance planning and cached permissions.

## What runs today

SNS-3 supplies the native satellite network models, including packet transport,
link capacity, queues, communication delay, and satellite mobility. Exact ns-3,
SNS-3, supporting-module, and satellite-data revisions are pinned in [deps.lock](deps.lock).

The current implementation provides:

- Working upstream GEO and two-satellite LEO scenarios with received traffic.
- A new C++ behavioral application for authorization, chunked product transfer,
  processing, delivery, and receipt recovery over native SNS-3 relay links.
- Six integration cases: normal completion, wrong subject, expired permission,
  lost receipt, conflicting token reuse, and an insufficient delivery deadline.
- Repeatable runs with source/input and executable hashes, seeds, commands,
  console logs, statistics, and terminal outcomes. Failed runs remain recorded.
- Bootstrap safeguards and upstream mobility/constellation validation.

See [baseline validation](docs/baseline-validation.md) and the
[behavioral application profile](docs/relay-profile.md) for the evidence and details.

## Current limits

The application endpoints are currently **ground nodes communicating through
satellite relays**. Onboard EO endpoints and customer constellation workloads
still need implementation and validation. This is research software.

Authorization uses a synthetic behavioral encoding, not real cryptography.
Its 1 ms authorization and 5 ms processing costs are explicit assumptions.
The [historical BBB/Jetson measurements](calibration/historical.json) are
user-reported reference data; the approximately 18 ms local round trip is not
assigned as a signature-verification delay. Restart durability, physical contact
interruption, customer comparisons, and payment settlement are not established.
The deadline case does not simulate a physical contact loss.

## Build and run on Linux / WSL2

Requires Git, a C++23 compiler, Python 3 with venv/pip, pkg-config, and libxml2
development headers. Bootstrap installs pinned CMake and Ninja versions into
an isolated environment and refuses mismatched or modified dependency sources.

```sh
python3 environment/bootstrap.py
environment/ns3.sh build sat-constellation-example test-runner scrap-relay-lifecycle -j 3
python3 -m unittest discover -s tests -v
python3 environment/smoke.py --run-id geo-01
python3 environment/smoke.py --scenario constellation-leo-2-satellites --run-id leo-01
python3 tests/run_integration.py --prefix lifecycle-01
```

Use fresh run IDs and prefixes: existing results are never overwritten. Each
run writes under `runs/`. Downloaded dependencies and build products live under
`.deps/`; both directories are ignored by Git. Sparse satellite-data checkout
excludes optional external fading traces of approximately 3.4 GB; scenarios that
need those traces are outside this bootstrap profile. Antenna patterns still
require approximately 442 MB. There is no simulator fallback.

The next study steps are onboard application integration, controlled EO contact
and workload scenarios, hardware calibration, and paired customer comparisons.
The [study contract](docs/study-contract.md) defines the assumptions and evidence
required before making customer-facing performance claims.
