# Broker persistence foundation

This foundation is inactive until the physical broker is enabled and connected.
Apply `migrations/001_broker.sql`, then `migrations/002_broker_courier.sql`, explicitly to a backed-up **Easy Mode** database;
never apply it to Classic. The migration intentionally fails on repeat application.
The original `players` schema/data are unchanged. All participating tables must
be InnoDB. Startup checks schema version and takes one connection-scoped broker
lease per database. One live world process per database is supported.

`brokerstore.cpp` uses a dedicated two-second-timeout connection with reconnect
and statement replay disabled. Failure pauses ownership/money operations;
reconnection occurs only at an operation boundary. Stable UUID/sequence keys
identify claims, trips and payouts. An unsuccessful COMMIT is treated as unknown,
never as proof of rollback. The gameplay layer must quarantine an uncertain
source and reconcile its claim before allowing movement or restoring inventory.

Escrow rows preserve item ID/count and reviewed unit price. Only attribute-free,
non-decaying, non-container identities are supported: these fields are their
complete durable image. Live items remain real instances until the sale commits.
Unsold rows recover on restart; sold rows never restore inventory. This relies on
claiming transient generated ground loot only, excluding house/persistent tiles.

The account records player `pending` and separately retained economy `funds`.
Each immutable sale records gross, net, commission and basis points. No spending
path exists. Round commission down once per sale batch, with the remainder going
to the player. Use quotient/remainder integer arithmetic; reject balances above
2^53-1, matching the historical Lua bank API's exact integer range. No clamping,
conversion, physical coin fallback or player storage keys are used.

Payouts lock the player and account and atomically credit bank, append a unique
payout ledger row and zero pending. A live bank watermark plus `savedBank()`
protects broker credits not yet incorporated into live memory. The physical feature wires that expression into IOLoginData load/save;
login obtains bank and payout watermark in one consistent SELECT. Existing SQL helpers with automatic reconnect must not perform broker
transactions.

CI uses a disposable MariaDB 10.11 service, public fixture credentials and the
repository schema; no live credentials, secrets or client are required. The
`broker_store` test is skipped without `ZEDAS_BROKER_TEST_PORT`. Its connection is
restricted in code to database `zedas_broker_test` with fixture credentials. It
checks lost acknowledgements before/after COMMIT, replay/ownership, commission,
capacity, overflow, bank-save expression and child-process restart. The gameplay layer additionally tests actual IOLoginData and physical claims.
`engine_broker_restart` abruptly exits four separate engine processes around
prepared-claim, sale and bank-payout commits, then checks one recovery/credit.
Fault injection is a test-only Store subclass; production uses the normal
no-replay connection and has no configurable failure hook.

Schema v2 separates immutable courier batches from new field claims; see
[BROKER_LOGISTICS.md](BROKER_LOGISTICS.md) for handoff and notification guarantees.
