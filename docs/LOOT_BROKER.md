# Physical loot broker

The first opt-in broker follows one online player GUID, approaches visible
routine leftover loot immediately, carries the actual items in private escrow, represents a trip to
the reviewed town buyers, and settles their actual purchase values. Worms 3492 stay inside
corpses/generated bags. Classic fishing scripts and consumption are unchanged.

The preceding audit is in `BROKER_AUDIT.md`; SQL details are in
`BROKER_PERSISTENCE.md`. This phase adds no market, broker purchases, combat,
respawn changes, denomination compression, item generation, loot sale at death,
XP/skill/stat/drop-rate changes or client executable modifications.

## Enable on an Easy Mode database

Back up the Easy Mode database and stop that world. Apply
`migrations/001_broker.sql` explicitly to that database using your normal database
administration tool, then configure and restart. Never apply it to Classic.
Do not re-import `nostalrius.sql` into a live world; CI imports that only into an
empty disposable fixture. The additive migration leaves `players` unchanged and
adds `zedas_broker_schema`, `zedas_broker_accounts`, `zedas_broker_escrow`,
`zedas_broker_sales`, `zedas_broker_payouts`. Keep the ledgers when disabling the
feature. Do not delete escrow rows or edit balances as a recovery shortcut.

All settings below are startup-only. Missing values use these defaults; malformed
values reject configuration, including numeric strings, fractions and NaN.

| Key | Default | Validation / effect |
| --- | --- | --- |
| `easyModeLootBroker` | `false` | Actual boolean; requires `easyModeEnabled=true` and `easyModeLootSpill=true` to start |
| `easyModeBrokerGraceSeconds` | `45` | Legacy integer 5–240 retained for future categories; approved routine fodder always uses zero grace |
| `easyModeBrokerCommissionBps` | `2500` | Integer 0–10000; retained economy share of each sale batch |
| `easyModeBrokerLeashTiles` | `7` | Integer 2–12; Chebyshev distance on the owner's floor |
| `easyModeBrokerCatalogFile` | `data/zedas/broker.xml` | Nonempty actual string; the whole catalog must pass validation |

`allowClones=false` and one live world process per database are required. Startup
requires the explicit schema, InnoDB and an exclusive broker database lease.
Invalid catalog/schema/lease prevents a broker-enabled world from starting.
With broker/master disabled, no broker connection, catalog or migration is needed;
historical bank load/save SQL and accepted Wave 2 behavior are preserved.
No new Action IDs, storage keys, client opcodes or items are reserved.

## Interface and behavior

Use normal local speech, not a channel message:

- `!broker hire`: opt into collection and resume after logout/travel/death.
- `!broker balance` or `!broker status`: inspect carried slots, pending proceeds,
  retained economy funds and travel state (0 hunting, 1 outbound, 2 returning).
- `!broker sell`: send a nonempty carried batch on its represented town trip.
- `!broker cashout`: atomically deposit your pending proceeds into your bank.
- `!broker dismiss`: stop collection; preserve escrow, trips and balances.

Near your dwarf, local `balance`, `cash out` or `cashout` also works. Another
player's dialogue cannot access your account. Commands have no target GUID.
Hiring is session-only; after relog, explicitly hire again. Durable inventory,
pending proceeds, commission and town-trip deadlines survive restart.

Routine pickup uses `TALKTYPE_MONSTER_SAY`, the same positional speech packet as
classic troll/orc noises. It says `Got a hatchet.` (using the actual item
description), at most once every **3000 ms per broker**. Additional pickups still
succeed silently; no captions are queued for later spam. `Off to town!` is sent
before departure removal and `Back from town.` after successful return placement.
Departure/return bypass pickup throttling and occur once per visible transition;
return speech waits if placement is blocked. There is no pickup/departure/return
status/chat text. Explicit hire/dismiss/balance/cashout responses and recovery
errors still use normal feedback. The original client controls noise lifetime and
rendering; no new opcode or executable change is introduced. Speech timers are
session-only and never gate item or money operations.

The dwarf uses existing look type 69 and NPC walking/scheduling, with its
walking pace following the owner through the normal speed API. It has no master,
summon slot, attack, damage, XP role or push interaction. Its body permits player
and monster movement; normal NPC/creature behavior is unchanged. Each queued step
checks owner session, floor and leash. Routes avoid houses, marked/quest tiles,
movement triggers, hazards and tiles already at the client visibility budget.
Loot search uses the NPC's existing same-floor visibility and a bounded native
path search. A blocked approach reserves nothing and expires after five seconds.
It can retry eligible leftovers, without remote pickup or teleporting to loot.

Manual pickup wins if processed first. Multiple brokers are first completed
claim wins. Only witnessed generated items actually exposed by spill qualify;
manual movement/merge, trade, corpse movement/decay/removal, disappearance or the
five-minute/1024-corpse registry bound retire unclaimed eligibility. Overflow becomes a candidate only when exposed; routine fodder then has zero grace. Later corpse insertions, dropped player items and
special-container subtrees are never discovered by ground scanning.

The live state model is FOLLOW/APPROACH, bounded COLLECT, OUTBOUND/SELL/RETURNING,
DISMISSED and RECOVERY_PAUSED. Pickup requires an adjacent dwarf, clear sight,
owner leash, exact witness and current eligibility. The private escrow has 20
whole-instance slots: no split, clone, autostack, conversion or bag dropped on the
map. A full batch or 15 seconds without another pickup automatically departs.
`!broker sell` can depart sooner. Town travel is explicitly represented, not a
claim that an NPC walked the entire map: the dwarf leaves, a durable 60-second
outbound deadline elapses, the sale settles, and a separate 60-second return
precedes reappearance near the owner. It does not collect during travel.

Far teleport/floor change pauses collection and despawns the actor; it does not
teleport to loot. Logout/death/dismiss stops collection. Already collected escrow
remains owned and saved. Trip progress resumes when the owner hires again; offline
collection and offline invisible sales are not performed.

## Reviewed sale catalog

The full audit scans 337 NPC files plus includes. The fixed catalog now contains
**58 routine identities from nine ordinary merchants**, with **33 additions**.
See `NPC_BUY_AUDIT.md` and `NPC_BUY_AUDIT.json` for every ID, exact name, buyer,
town, GP price, literal offer/confirmation, source line and access decision.
Sam's original 25 identities/prices remain, preserving existing escrow images.

The explicit route covers Sam, Shanar, Kroox, Timur, Uzgod, Al Dee, Willard,
Willie and Zaidal. A canonical fingerprint binds the reviewed XML; each buyer's
actual NPC file is separately fingerprinted, including its purchase confirmation.
A custom configured catalog may narrow that version-2 data, but may not invent
buyers/prices or promote player-interest items. Old custom version-1 catalogs
need conversion to the reviewed version-2 subset format. Never choose maximum
NPC prices automatically. The represented 60-second town trip covers this reviewed
route; it is not a claim of full-map walking or live NPC dialogue.

Approved fodder has **zero grace**, even if the legacy grace setting is 45.
The normal one-second decision tick and native approach provide only a technical
delay. Approach reserves nothing; a manual pickup completed first still wins.
The grace-aware witness API remains available for future categories.

Djinn/faction/quest/premium-gated and high-value/specialty items remain entirely
player-controlled. Generic gem/jewelry offers do not turn pretty loot into fodder.
Studded helmet 3376 and leather legs 3559 have proven ordinary purchase offers;
studded armor 3378, studded legs 3362 and soldier helmet 3375 have only gated/non-town
paths and are excluded. Bow/crossbow have valid Timur offers; arrows/bolts have
no proven ordinary buyback offer and remain player-only. Price evidence alone
does not bypass movement/script/state safety. Worms and corpse junk stay inside.

## Money, durability and recovery

Gross is the sum of reviewed unit prices times actual counts. Commission is
`floor(gross * bps / 10000)` once per batch; player net is the exact remainder.
10000 GP at 2500 bps gives 7500 pending / 2500 economy funds; a 2 GP dagger gives
2 / 0. Separate persistent per-owner `funds` and immutable sale commission records
retain the economy share. Summing `funds` gives the world retained fund; no spending
path exists. Unsold items and pending/bank/fund balances fail closed on overflow;
new broker money is limited to 2^53-1 to match the legacy Lua bank API.

Claims use a durable unique operation key **before** native ground-to-private-bag
movement. The ID/count image is complete because all attributes, charges, decay,
containers and unreviewed identities are excluded. Restart restores each unsold
image once; sold rows never restore. A live item is consumed only after a known
committed sale. Trip/account updates, the sale ledger, item sold flags, player
net and economy commission commit together. No duplicate player/house save owns
these items. Only transient generated non-house ground can supply a claim.

A dedicated connection disables automatic reconnect and statement replay and uses
bounded two-second network timeouts. Failed money/ownership operations pause.
After a lost prepare acknowledgement, the exact ground source is temporarily
quarantined against native move/merge/remove/transform until the same claim key
is reconciled. This exceptional recovery lock never applies during intention,
approach or grace. If the insert rolled back, the untouched source unlocks. If
it committed, the actual original enters escrow once. DB outage can temporarily
leave such an item visible but immovable. A changed/unsafe source remains paused
for operator review rather than risking a second copy. Do not manually duplicate
it or delete the journal. Ordinary unclaimed ground retains Classic crash/decay
semantics; a crash before a durable claim is established does not constitute a
completed broker pickup.

Bank payout, immutable payout ID and pending zeroing are one transaction. Login
loads bank plus payout watermark in one consistent SELECT. Player saves include
unseen ledger credits, so a committed payout with a lost acknowledgement cannot
be overwritten by stale live bank memory. Reconciliation folds those credits
into memory once; repeated saves/cashouts are idempotent. No physical coin fallback
exists. Normal independent bank changes stay part of the live baseline.

The crash proof assumes the same InnoDB database/ledgers are retained, exclusive
world ownership and durable database commits. Restoring only part of a database
backup, manually editing ledgers, database corruption and changing server scripts
under a quarantined source require operator recovery; they are not automatic
exactly-once guarantees. No remote live-database migration was performed here.

## Automated and client acceptance

`engine_broker` uses the real engine and a disposable MariaDB: actual generated
nested loot, immediate fodder pickup, manual/two-broker races, currency/worm/protected exclusions,
instance identity, native path steps, unreachable/disappearing loot, floor/leash,
corpse removal, house exclusion, sale travel, exact prices, commission, replay,
prepare/sale/payout lost acknowledgements, source quarantine and actual
IOLoginData save/load with stale bank memory. `broker_store` additionally covers
20-slot capacity, overflow, lease contention and child-process recovery.
`engine_broker_catalog` checks malformed/mismatched prices, conditional/unreviewed
items and changed source rules. Configuration tests reject each malformed option.
Existing Wave 1/2 tests remain in the suite. SQL tests skip without the explicit
fixture port; CI initializes a disposable database for both Lua backends.

Use `BROKER_TEST_CHECKLIST.md` for original Tibia 7.72 acceptance. Appearance,
clicking overlapping creatures, smooth following, helpful diversion/feedback and
travel pacing still require that client. Automated tests do not establish those
visual/gameplay judgments. No broader economy phase is authorized by this feature.

Local verification uses a full no-cotire GCC build and isolated MariaDB 10.11.
ASan/UBSan fixture leak checking is disabled because process-lifetime engine
globals are not torn down by the harness. This does not claim leak-detector or
actual-client approval. Both Ubuntu 24.04 Lua variants run the full suite through
the same secret-free workflow and disposable SQL fixture.

Already-loaded escrow is revalidated against its durable ID/count/price image
and current safety handlers before departure or settlement. A newly marked item
or unsafe action reload pauses the operation and retains the instance and image.
The native regression checks both changes through the actual sale command.

The expanded delivery passes **29/29** native/source/SQL tests normally and with
ASan/UBSan (fixture leak checking disabled). `engine_broker_feedback` inspects
actual outgoing positional monster speech and verifies no routine status/chat
packet, pickup throttling, departure, return and explicit balance replies.
Floor reappearance remains audit-only; see `NPC_BUY_AUDIT.md`.
