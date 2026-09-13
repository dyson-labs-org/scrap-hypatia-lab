# Study contract

## Question and comparisons

Does bounded preauthorization improve EO product delivery before a deadline,
relative to conventional authorization using the same resources?

Three arms: existing operator resources; expanded partner resources with
conventional authorization; identical expanded resources with SCRAP. Conventional
authorization includes advance planning and cached permission. Any online
approval delay is explicit and swept, not silently charged to the baseline.

## Model boundaries

- Start with a product already captured. No camera, clouds, pointing, or image
  quality model is implied by satellite network coverage.
- Ground plans task routes. This lab does not grant SCRAP autonomous route repair.
- Network propagation, packet serialization, queues, and drops come from ns-3/SNS-3.
- Application processing, contacts, storage, permissions, and deadlines are
  explicit scenario inputs with units and provenance.
- Initial authorization is a behavioral model, not real cryptographic validation.
  Modeled receipts are not proofs of useful sensing, correct computation, or payment.
- A lost reply must not turn a retry into a second execution. Distinguish retry
  recovery from attempting to use a consumed authorization for a different task.
- Execution, delivery, receipt, and ground settlement are separate outcomes.
  This study does not send payments or control flight hardware.

## Specification profile

Reference: https://github.com/dyson-labs-org/scrap/blob/585f4478353e1250b1177f7758624b32282b5c3b/spec/SCRAP.md
The source commit and document blob are pinned in `deps.lock`.
Section 2.2.1 selects CBOR + ES256K; older TLV/Schnorr examples elsewhere
in the document conflict with that normative section. A new implementation
must resolve such conflicts explicitly and name its tested profile.
Historical benchmarks use Ed25519. The reference describes secp256k1. Timings
are not transferable between them. A modeled profile must be named explicitly;
implementation validation requires a selected build and actual message fixtures.

## Evidence

Capture source revisions, input hashes, seed, toolchain, parameters, terminal
outcomes, and event logs for every run. Account for failures and unfinished tasks
in deadline denominators. Report paired repetitions and uncertainty; distinguish
sample quantiles from established tail latency. Synthetic scenarios are not
calibrated customer results. Failure to reproduce a measurement is reported.
