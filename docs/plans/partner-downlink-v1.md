# Partner downlink v1 — implementation plan

Status: Published planning index for a **draft specification**. Modeling decisions remain open in [D00 (#29)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/29). Publishing this plan does not authorize treating those assumptions as settled.

- [Draft specification](../specifications/partner-downlink-v1.md)
- [Study contract](../study-contract.md)
- [Partner downlink v1 milestone](https://github.com/dyson-labs-org/scrap-hypatia-lab/milestone/1)

GitHub Issues hold detailed scope, acceptance criteria, discussion, and live status. This document holds the work sequence and requirement coverage; it deliberately does not duplicate issue bodies or completion checkboxes.

## Baseline and gap

Compared against repository commit [de9de55117d68e5aafc21ff297d7983c177e93fb](https://github.com/dyson-labs-org/scrap-hypatia-lab/tree/de9de55117d68e5aafc21ff297d7983c177e93fb) on 2026-09-21. This assessment used static source inspection; simulator tests were not rerun for the planning exercise.

Retain the pinned dependency/bootstrap environment, checkout safeguards, unique run directories, input/source/binary hashes, atomic manifests, failure logs, and six existing relay lifecycle cases. Extend these capabilities rather than rebuild them.

The current application attaches endpoints to ground user nodes, sends one fixed 8 KB product, and treats executor processing completion as delivery. The target is an onboard product moving through a scheduled satellite-to-station contact and explicit backhaul to the operator, with fair conventional/SCRAP comparisons.

A specific semantic difference is tracked in T04: the current application rejects completion at `Now() >= deadline`; the specification counts complete delivery exactly at the deadline as on-time. Receipt recovery must also become independent of that delivery cutoff.

## Tickets and dependencies

| ID | Issue | Depends on |
| --- | --- | --- |
| D00 | [Settle the remaining study assumptions (#29)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/29) | None |
| T01 | [Prove an onboard SNS-3 source can downlink to a station (#30)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/30) | None |
| T02 | [Load a versioned synthetic study scenario (#31)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/31) | [D00 (#29)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/29) |
| T03 | [Deliver one onboard product through a station to its operator (#32)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/32) | [D00 (#29)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/29), [T01 (#30)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/30) |
| T04 | [Track multiple products, storage, and exact delivery outcomes (#33)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/33) | [T02 (#31)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/31), [T03 (#32)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/32) |
| T05 | [Derive and enforce moving-satellite contact windows (#34)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/34) | [T01 (#30)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/30), [T02 (#31)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/31), [T03 (#32)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/32) |
| T06 | [Schedule products against satellite and station capacity (#35)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/35) | [T04 (#33)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/33), [T05 (#34)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/34) |
| T07 | [Deliver plans and service credentials over explicit control paths (#36)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/36) | [T06 (#35)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/35) |
| T08 | [Add conventional partner admission and cached permissions (#37)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/37) | [T07 (#36)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/36) |
| T09 | [Add SCRAP downlink admission with equivalent service policy (#38)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/38) | [T07 (#36)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/36), [T08 (#37)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/37) |
| T10 | [Recover interrupted transfers and lost receipts without duplicate service (#39)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/39) | [T04 (#33)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/33), [T05 (#34)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/34), [T06 (#35)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/35), [T08 (#37)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/37), [T09 (#38)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/38) |
| T11 | [Run all three arms on the same environment and workload (#40)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/40) | [T06 (#35)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/35), [T08 (#37)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/37), [T09 (#38)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/38), [T10 (#39)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/39) |
| T12 | [Validate the complete study model against independent fixtures (#41)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/41) | [T11 (#40)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/40) |
| T13 | [Execute paired experiments with complete evidence (#42)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/42) | [T02 (#31)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/31), [T11 (#40)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/40) |
| T14 | [Generate the operator-facing comparison report (#43)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/43) | [T12 (#41)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/41), [T13 (#42)](https://github.com/dyson-labs-org/scrap-hypatia-lab/issues/42) |

Dependencies are completion prerequisites. T01 can proceed while D00 is resolved. T13 may be developed alongside T12, but comparative study results are not accepted before T12's validation gate passes. The first feasibility result may require splitting or revising later tickets; these are not fixed effort estimates.

## Delivery checkpoints

1. **Integration proven:** T01 demonstrates a real onboard-to-ground path; D00 records agreed model boundaries.
2. **One useful delivery:** T03 delivers a product to the operator over explicit backhaul.
3. **A scheduled workload:** T02 and T04–T07 provide bounded products, moving contacts, reservations, and delivered plans.
4. **A fair failure-aware comparison:** T08–T12 demonstrate both authorization approaches and recovery on identical inputs.
5. **Operator-readable evidence:** T13–T14 produce repeatable synthetic results, uncertainty, and limitations.

## Specification acceptance coverage

| Specification acceptance criterion | Primary tickets |
| --- | --- |
| 1. Reproducible command sequence | T12–T14 |
| 2. Onboard source through downlink and backhaul | T01, T03, T05 |
| 3. Deterministic lifecycle and failure behavior | T04–T10, T12 |
| 4. Independently checked transfer timing | T01, T05, T12 |
| 5. Capacity and authorization invariants | T06, T08–T10, T12 |
| 6. Fair three-arm comparison and caching | T07–T09, T11 |
| 7. Repeatability | T02, T11–T13 |
| 8. Complete provenance and failure records | T02, T04, T13–T14 |
| 9. Complete denominators and correctly attributed effects | T04, T11–T14 |
| 10. Visible assumptions and calibration needs | D00, T02, T09, T14 |

## Keeping the records current

- Update the specification in the same PR when an agreed requirement changes, with the reason recorded in the relevant issue.
- Maintain dependency links in both the issue and this index when sequencing changes.
- Close implementation issues with the implementing PR and appropriate validation evidence. Close D00 with explicit decisions and their specification update.
- Preserve the distinction between draft assumptions, implemented behavior, and validated results.
- Consult the milestone for current status; do not infer completion from a ticket's position in this plan.

Product processing, payment, real cryptographic conformance, autonomous replanning, and customer calibration remain outside this milestone.
