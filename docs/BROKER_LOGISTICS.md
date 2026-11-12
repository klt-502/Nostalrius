# Collector and courier redesign

## Audit and persistence

Previously the one actor disappeared throughout account states 1/2, and `prepare`
rejected claims in those states. `settle` consumed every unsold row. This made
continuous collection impossible without a durable batch boundary.

Schema v2 (`migrations/002_broker_courier.sql`, after 001, server stopped) adds an
immutable nullable `escrow.trip` and two account notification flags. Under the
existing exclusive DB lease and account row lock, handoff assigns only unassigned
rows and updates the one active courier slot in one transaction. New field claims
remain unassigned. Settlement consumes only that trip. Reusing a handoff key,
including after return, cannot assign new items. No second item image is created
at handoff. Existing v1 outbound batches are backfilled without changing their
deadlines, prices, balances or settlement identities. Historical notices are muted.
The server requires schema v2 only when the broker is enabled.

A departure/sale notification is durably claimed before sending, after confirmed
financial commits. This gives **at-most-once emission**, including after a lost
commit acknowledgement or restart. Atomic exactly-once delivery to an unmodified
Tibia client is impossible: a crash between the notification commit and socket
send may omit the summary. `!broker balance` and the immutable sale ledger remain
authoritative. No notice is emitted for an uncommitted sale.

## Missed brass helmet

`easyloot.cpp` originally stored spilled ground witnesses inside `CorpseEntry`.
`valid()` expired that entry 300,000 ms after generation; inserting the 1,025th
corpse also evicted the oldest entry. `brokerCandidates()` only consulted these
entries, so an untouched helmet could never qualify again after retirement.
Visibility exit itself did not invalidate a witness. Separately, the old collector
paused hiring when it fell outside its leash or changed floor. These are distinct
coverage failures; neither justifies scanning arbitrary ground items.

Foundation validation: full native suite 29/29, including rollback/lost handoff
acknowledgement, key reuse, continued field claims, batch-only sale and notice replay.
