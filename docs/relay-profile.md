# relay-lifecycle-v1 (behavioral integration profile)

This program replaces the simple traffic generator with a new application over
native SNS-3 packet queues and satellite relay links. Both application endpoints
are ground nodes: a terminal-side commander and a gateway-side executor. It is
not an onboard EO source model, a constellation comparison, or a flight protocol.

The protocol reference is pinned in deps.lock. This test uses its own explicitly
synthetic encoding, not CBOR/ES256K: eight network-order uint32 fields, padded to
256 request bytes, 64 reply bytes, and 544 bytes per data chunk. There is no
cryptography. A fixed peer address represents an assumed trusted association.

One token authorizes one task with a fixed audience and subject. Sixteen unique
512-byte chunks constitute the 8192-byte synthetic product. Authorization costs
1 ms and processing costs 5 ms, both assumed. They do not represent the BBB's
17.62 ms round trip. Processing completion delivers the product at the executor;
receipt arrival at the commander is tracked separately. Neither proves useful
computation, signed evidence, nor payment settlement.

Authority must be unexpired when verification completes. Admitted work may
continue until the task deadline. Identical retries recover a completed receipt
without executing again, including after token expiry. Conflicting task reuse is
rejected. State is in memory for the entire simulation: restart durability is
not established. Retries resend the request every two seconds; a permit triggers
all chunks again, with duplicates deduplicated at the receiver.

Cases: normal; wrong subject; expiry before authorization; first receipt omitted
as an explicit application fault; conflicting task reuse; and a 1.05-second
deadline that is insufficient for the initial GEO exchange. The last case is a
deadline test, not a modeled physical contact loss. Packet delay and loss still
come from SNS-3. No arm receives a synthetic authorization advantage.

```sh
environment/ns3.sh build scrap-relay-lifecycle -j 3
python3 tests/run_integration.py --prefix check-01
python3 environment/smoke.py --program scrap-relay-lifecycle --case normal --run-id lifecycle-normal-01
```

Each case emits event traces and a machine-readable RESULT line, and exits
nonzero if its expected terminal outcome fails. The model is limited to one
token/task pair; multi-tenant authorization, persistent restart recovery,
onboard endpoint support, real contacts, calibration, and paired customer
comparisons remain separate validation work.
