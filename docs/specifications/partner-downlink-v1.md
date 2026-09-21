# SNS-3 partner ground-station downlink study

Status: Draft v0.1 for review. User-confirmed scope and proposed modeling choices are distinguished below. This is a specification, not a report of implemented or validated behavior.

Date: 2026-09-21

Repository: https://github.com/dyson-labs-org/scrap-hypatia-lab

Navigation: [study contract](../study-contract.md) · [implementation plan](../plans/partner-downlink-v1.md) · [milestone](https://github.com/dyson-labs-org/scrap-hypatia-lab/milestone/1)

Open modeling decisions are tracked in the milestone's D00 issue. Publishing this draft does not resolve those decisions.

## 1. Purpose and intended decision

Help a prospective satellite or ground-station operator decide whether a pilot of SCRAP-enabled partner downlink is worth investigating, and identify the operational conditions in which it would or would not help.

The primary question is:

**For already-captured Earth-observation products, when does access to additional partner ground stations improve delivery before a deadline, and does SCRAP provide any incremental benefit over conventional authorization with access to the same stations?**

The study must distinguish additional contact opportunities, authorization and coordination effects, and protocol overhead. A null result, or conventional authorization outperforming SCRAP, is a valid result. A positive SCRAP result is not an acceptance requirement.

## 2. Scope decisions

### Confirmed with the project owner

- Focus on downlink and delivery through partner ground stations.
- The motivating hypothesis is difficulty coordinating with partners that lack an existing direct relationship with the satellite operator. This is not an established measurement of industry practice.
- Ground systems plan routes; start with operational behavior that is straightforward to implement.
- Exclude onboard and third-party product processing.
- No customer workload or network dataset is available. Use explicitly synthetic scenarios and sensitivity analysis.
- Emphasize usefulness to prospective satellite and ground-station operators.

### Proposed defaults for this draft

- Begin with one LEO satellite, one existing station, and two candidate partner stations. Expand only after this configuration is validated.
- A product is already stored onboard when released into the workload. Its deadline is an absolute simulation timestamp.
- The delivery endpoint is an operator-controlled ground destination. Ground-station reception and delivery to that destination are distinct milestones.
- A partner is technically compatible, participates in a common service/trust arrangement, and has an explicit resource policy, but need not have a direct bilateral relationship with this operator.
- Compare modeled authorization behavior first. Real cryptographic interoperability is a later validation step.
- No financial settlement, pricing model, or autonomous onboard rerouting in this milestone.

These defaults are proposed modeling choices, not additional facts supplied by the project owner.

## 3. What an unfamiliar partner means

A candidate partner must have a usable radio configuration, known contact opportunities, an available receiver, a ground delivery path, and a way to recognize the operator's authority to request service. Its policy specifies who may request service, during which intervals, and within what resource limits.

The study assumes a common trust/service arrangement makes such requests possible. It does not assume that a token alone establishes commercial consent, compatible radio equipment, regulatory permission, or trust between arbitrary strangers. Establishing that arrangement is outside the timed packet simulation and must be stated in every report.

Within that boundary, compare the time and communication needed to admit a particular downlink request. Model partner admission delay and control-service availability as scenario inputs. Do not interpret a chosen admission delay as a measured contract-negotiation duration or claim to quantify legal/commercial onboarding savings.

Both conventional authorization and SCRAP use the same acceptance policy, trust eligibility, task limits, receiver availability, and information about planned contacts. Neither gets preferential station access merely because of its label.

## 4. Required end-to-end behavior

1. Release a product into satellite storage with a stable identifier, size, destination, priority, and deadline. Acquisition and image quality are outside scope.
2. The ground planner evaluates known satellite-to-station contacts, advertised station availability, transfer capacity, ground delivery capacity, and authorization state.
3. The planner produces a scheduled transfer to a station and communicates the necessary plan and credentials before execution. Plan/control delivery uses an explicit communication path and delay; plans cannot appear onboard instantaneously during disconnection.
4. The station admits or rejects the operation according to the selected authorization mechanism and its resource policy. Permission must not grant capacity already reserved by another operation.
5. The satellite transfers product data only during an available and authorized contact. Packet transport, serialization, propagation, queues, and drops use ns-3/SNS-3 network behavior.
6. The station stores the received product and forwards a complete product to the operator endpoint. V0.1 uses store-and-forward at the station; no product computation or streaming-processing model is implied.
7. The destination records the first complete, unique delivery. A receipt is returned to the responsible ground controller. Receipt arrival is tracked separately from data delivery.
8. Log partial transfers, rejected requests, retries, contact interruptions, deadline misses, and unresolved receipts. Preserve enough events to explain each terminal outcome.

Proposed recovery policy: retain partial chunks at the chosen station and resume on a later ground-scheduled contact to that station. A transfer to another station starts a separate copy; v0.1 does not combine fragments across stations. Deduplicate product delivery at the final destination. An identical request retry must not create another reservation or count as a second service execution.

At the deadline, stop scheduling new transmission work for an undelivered product and mark it missed. A subsequently arriving in-flight completion is recorded as late, never on-time. State retention for receipt recovery follows an explicit configured retention interval.

## 5. Comparison design

| Arm | Station access | Authorization |
| --- | --- | --- |
| A: Existing resources | Existing station only | Conventional authorization, with existing permissions |
| B: Expanded conventional | Existing and candidate partner stations | Conventional partner admission with realistic modeled advance approval and caching |
| C: Expanded SCRAP | Exactly the same stations as B | Explicitly named SCRAP behavioral authorization profile |

Report B minus A as the effect of expanded resources under conventional coordination. Report C minus B as the incremental effect of the modeled SCRAP mechanism. Do not attribute C minus A entirely to SCRAP.

The conventional arm must support preapproved/cached permissions and a request/approval path when new permission is needed. SCRAP must include credential issuance/provisioning traffic and lead time, verification cost, expiry, and retry behavior. If both mechanisms can be prepared before a contact, both receive that opportunity.

Run at least two authorization conditions:

- **Prepared access:** both approaches have valid authority available before contact. This is the essential cached-permission control.
- **New task-specific admission:** credentials or approval still need preparation. Both incur their configured preparation requirements, with connected and temporarily disconnected control-service variants.

Use a ground-computed schedule with the same scheduling algorithm for each arm. Arm-specific eligibility can change the resulting schedule; report those changes. Also run B/C on one common feasible schedule in diagnostic cases to isolate authorization overhead from scheduling effects.

Ground-station contention, background reservations, product arrivals, physical faults, and backhaul conditions must be identical within each paired comparison. Keep independent random streams for workload, network faults, and other exogenous inputs so extra protocol messages do not change the sampled environment.

## 6. Ground planning and resource rules

Use a deterministic initial planner: consider products by earliest deadline, break ties by stable product identifier, and assign the earliest predicted complete-delivery opportunity that satisfies known constraints. Reserve satellite transmitter, station receiver, and relevant transfer capacity explicitly. Document how predicted transfer times are calculated and compare them with observed times.

Each satellite and station initially has one active downlink resource. A station may carry synthetic background reservations, representing other users without requiring a full multi-operator constellation.

The planner has knowledge of scheduled geometry and advertised reservations, not future random outages. For the first study, compute the plan before execution; do not perform adaptive replanning after unexpected failures. Scheduled resumptions may execute if they were included and authorized in advance. This keeps route choice ground-controlled and bounds the first milestone.

“Downlink optimization” means evaluating delivery improvements from resource access and coordination under this stated policy. The first milestone does not claim a globally optimal scheduling algorithm.

## 7. SNS-3 implementation boundary

The current repository documents ground endpoints communicating through satellite relays. The new workload requires an application source onboard a modeled satellite and an actual satellite-to-ground receiver path. Relabeling the existing ground source as a satellite is insufficient.

Before comparative studies, validate the pinned SNS-3 configuration's ability to support this path, its contact transitions, and the selected antenna/link assumptions. If extension is needed, document it and verify it before proceeding. Do not silently replace the network with a transfer-duration formula.

Keep two types of scenarios distinct:

- Small deterministic test fixtures may use explicitly imposed contact gates to verify scheduling, interruption, and recovery. Label these as synthetic gates.
- Study scenarios use a documented synthetic orbit and station geography, with contact availability derived consistently from geometry and the chosen visibility/link rules. A separate fault can make a geometrically visible station unavailable.

Report orbit definitions, station coordinates, elevation/visibility rules, configured radio parameters, and effective usable contact capacities. Validate that contact closure prevents further radio transmission and define treatment of packets already in flight. Application timers alone are not evidence of a physical contact interruption.

Use an explicit ns-3 ground backhaul model for station-to-destination traffic. Its capacity, delay, and outages are assumptions shared by all arms.

## 8. Synthetic scenario inputs and experiment plan

Every parameter must have units and provenance: user-confirmed scope, chosen synthetic assumption, derived quantity, or future measured input. Do not call synthetic parameters representative of an operator without supporting evidence.

Start with a 24-hour synthetic workload window and extend execution until all evaluated product deadlines and the configured receipt-observation interval have elapsed. Products still incomplete at observation end remain in the denominator.

Required configurable inputs include:

- Orbit, station locations, visibility rules, and link configuration.
- Product release times, sizes, deadlines, onboard and station storage capacities.
- Station reservations, availability faults, and ground backhaul behavior.
- Authority scope, validity, provisioning lead time, control connectivity, message sizes, verification delay, and retry policy.
- Plan delivery timing, fixed scheduling policy, and random-stream seeds.

Proposed initial sensitivity levels, explicitly experimental rather than empirical:

| Variable | Initial levels |
| --- | --- |
| Offered demand | 25%, 75%, and 125% of derived existing-station usable capacity over the workload window |
| Product size | 10%, 50%, and 125% of the usable capacity of a nominated reference contact |
| Deadline slack from release | 15 minutes, 1 hour, and 6 hours |
| Partner background occupancy | 0%, 50%, and 90% of otherwise available receiver time, using saved reservation schedules |
| Additional admission-service wait | 0 seconds, 30 seconds, and 5 minutes, with network delay accounted separately |
| Control-service availability | Connected; a saved outage overlapping the selected preparation/contact interval |

Convert capacity-relative sizes and demand into concrete bytes and timestamps before execution and save them. All arms use those same values. Verify the chosen geometry actually contains the intended useful contacts; flag combinations with no physically feasible solution.

Vary one major factor at a time around a recorded reference case, then run selected interactions: deadline versus admission wait; occupancy versus load; preparation lead time versus control outage. A full Cartesian product is not required. Select cases before inspecting which approach wins.

For stochastic study cases, start with 30 paired seeds per selected parameter setting. Report uncertainty across independent run pairs; products within a run are not independent replicates. Increase repetitions if uncertainty prevents answering the intended comparison. Deterministic fixtures do not need artificial repetitions.

## 9. Required scenarios

| Scenario | What it establishes |
| --- | --- |
| Existing station can meet the deadline | Extra partners need not improve delivery |
| Only a partner has a useful contact | Separates additional access from mechanism benefit |
| Both approaches are preauthorized | Fair cached-permission control and overhead comparison |
| Admission completes near contact closure | Sensitivity to preparation and approval timing |
| Control outage with previously prepared authority | Compare cached conventional permission and SCRAP under equivalent conditions |
| Control outage without prepared authority | No mechanism receives unexplained authorization during disconnection |
| Partner receiver contention or outage | Permission cannot manufacture station capacity |
| Contact ends during transfer | Partial transfer and ground-scheduled resumption operate correctly |
| Expired, wrong-subject, or wrong-audience request | Invalid authority cannot admit the service |
| Lost receipt and identical retry | Delivery is not executed or counted twice |
| Conflicting reuse of an authorization | A retry cannot authorize a different operation |
| Slow ground backhaul | Station reception does not imply timely operator delivery |
| No feasible contact or inadequate aggregate capacity | Honest failure accounting and bounded conclusions |

Define whether authorization must remain valid throughout transmission or only through admission in the behavioral profile. Proposed continuity with the existing profile: authority must be valid at admission completion; admitted work remains bounded by its reserved contact, byte allowance, and product deadline. A later contact requires a valid reservation and authority covering that attempt in both mechanisms.

## 10. Measurements and interpretation

Primary metric: unique products completely delivered to the operator endpoint at or before their deadlines, divided by all evaluated released products. Equality at the deadline counts as on-time. Failed, denied, and unfinished products stay in the denominator.

Also report:

- Unique bytes delivered on-time as a fraction of offered bytes.
- Delivery latency distributions alongside completion counts; label latency conditioned on successful delivery to avoid hiding failed products.
- Ground-station reception time, operator delivery time, and receipt arrival time separately.
- Usable and used contact time, station occupancy, unique payload goodput, and retransmitted bytes.
- Authorization/control bytes and messages, preparation time, verification time, and wait for admission.
- Partner stations used, rejected reservations, partial products, duplicates suppressed, and unresolved receipts.
- Paired differences between arms with run-level uncertainty intervals and all per-run outcomes.

Use event-backed failure classifications such as no feasible contact, resource conflict, missing/invalid authority, interrupted transfer, storage exhaustion, and backhaul deadline miss. Record secondary contributing events. Do not present the final observed blocker as proof of the counterfactual cause; use controlled comparison runs for causal interpretation.

The operator-facing report should explain under which assumptions a benefit appears, the magnitude and uncertainty, when conventional coordination matches SCRAP, and which missing customer measurements could reverse the conclusion. No synthetic result establishes commercial adoption, cryptographic security, or customer ROI.

## 11. Acceptance criteria

The milestone is complete when:

1. One documented command sequence builds the pinned environment, runs deterministic validation, executes the selected paired studies, and generates the report.
2. An onboard product travels through the validated satellite-to-ground path and explicit ground backhaul to its destination.
3. Deterministic fixtures verify on-time delivery, expiry, contact closure, resource conflicts, duplicate suppression, partial resumption, and late/incomplete accounting against independently specified expected outcomes.
4. A small, uncongested transfer matches independently calculated serialization/propagation bounds within a documented tolerance that accounts for modeled overhead.
5. No station serves conflicting reservations beyond its configured receiver capacity, and no unauthorized operation transfers admitted payload.
6. A, B, and C can run from the same saved workload/environment input; B/C have identical partner eligibility and a working cached-permission control.
7. Repeat runs with identical seeds and inputs reproduce terminal counts and event ordering, subject to any documented simulator limitations.
8. Each run records dependency and source revisions, executable/input hashes, parameters, seeds, commands, traces, and terminal status. Failed runs remain recorded; infrastructure failures are reported separately from simulated delivery failures and cannot be silently discarded.
9. Reports include all evaluated products and distinguish access gains from authorization gains. No minimum SCRAP improvement is required.
10. Every synthetic assumption and unresolved implementation limitation is visible in the report, together with the customer data needed for calibration.

## 12. Deliverables and exclusions

Deliverables: validated onboard downlink application; ground station and destination applications; deterministic ground planner; conventional and SCRAP behavioral profiles; versioned scenario inputs; paired experiment runner; outcome/event schema; regression fixtures; and a reproducible report with machine-readable results and plots.

Exclude product processing, sensing/acquisition, autonomous rerouting, inter-satellite forwarding, orbit optimization, market negotiation, billing/payment, real flight control, restart durability, full multi-tenant security validation, and claims of production cryptographic conformance. Later milestones may add these through explicit scope changes.

The existing protocol-reference conflicts and historical cryptographic timings remain unresolved by this study. Name the behavioral message profile and its assumed sizes/costs, sweep material overhead assumptions, and do not label it a conformant wire implementation.

## 13. Review and implementation sequence

Review this specification first, particularly the common trust arrangement and fixed-plan behavior. Then derive implementation tickets in this order: onboard/contact feasibility; minimum downlink and backhaul; scheduling/resource accounting; authorization comparison; interruption/retry validation; study runner and operator report. The [implementation plan](../plans/partner-downlink-v1.md) links the corresponding GitHub issues and their dependencies.

Future operator input should prioritize: actual contact schedules and station compatibility, service-admission policies, product arrival/size/deadline distributions, receiver contention, backhaul characteristics, and measured credential preparation/verification costs.

## Source basis

- [Repository README](https://github.com/dyson-labs-org/scrap-hypatia-lab/blob/main/README.md), read 2026-09-21; blob `1eb702647d30b835b3250c25e2905ddbe35345d1`.
- [Existing study contract](https://github.com/dyson-labs-org/scrap-hypatia-lab/blob/main/docs/study-contract.md), read 2026-09-21; blob `2f19c0f6bf4581e470c338680e31ed0c01d13f32`.
- [Dependency lock](https://github.com/dyson-labs-org/scrap-hypatia-lab/blob/main/deps.lock), read 2026-09-21; blob `a80e16cfbf2ade024d6b0d188f99838a2b5c4a3e`.
- Project-owner scope answers in this conversation. All additional behavior and experiment values above are draft proposals.
