# Loot broker implementation audit

Inspected `zedas-easy-mode` at `ce1fed2169e6bcc330b67cfd74ea6c55a18e22bf`.
This audit precedes broker gameplay/database changes. It establishes the required
ownership transitions and the checks that must pass before enabling the service.
The Classic branch and checkpoint tags are outside the change set.

## Findings that rule out a simple Lua collector

| Existing source | Finding / implication |
| --- | --- |
| `Monster::dropLoot`, `src/monster.cpp`; `EasyMode::onLootGenerated`, `src/easyloot.cpp` | A completed generated-loot snapshot already exists. Reuse it; never scan arbitrary ground items or later corpse insertions. |
| `spillEntry`, `CorpseEntry`, `onLootItemMove`, `src/easyloot.cpp` | Successful spill currently releases the snapshot reference. A broker needs a third, bounded set of witnessed ground references, retired by the same manual-mutation hooks. Grace starts at actual exposure, including delayed overflow. |
| `EasyMode::isSpillItem` / `isSpillContainer`, `src/easyspill.cpp` | These checks establish movement safety, not sale value. Sale membership must be separate. Worm 3492 must leave the spill policy as well as every broker path. |
| `Game::internalMoveItem`, `src/game.cpp` | Stackables can be cloned even for complete transfers. Broker pickup must retain the complete instance through native Cylinder remove/add/notifications, as spill does. No partial claim or autostack is needed for initial escrow. |
| `Map::save`, `src/map.cpp`; `IOMapSerialize::saveHouseItems`, `src/iomapserialize.cpp` | Generated non-house ground drops are transient across restart; house tiles have durable item storage. Exclude house/persistent destinations from broker claims. The recovery proof must not assume all ground is transient. |
| `Item::serializeAttr`; `IOLoginData::saveItems` / `loadItems`, `src/item.cpp`, `src/iologindata.cpp` | Existing item ID/count/attribute serialization supports an SQL-backed, server-owned escrow. Live pickup preserves the instance; restart restores its serialized ownership as ordinary persistence does. |
| `Database::connect`, `executeQuery`, `storeQuery`, `src/database.cpp` | Automatic reconnect and retry can repeat a mutating statement after losing a transaction. Existing transaction wrappers are insufficient for broker money operations. Use a separate bounded, no-replay connection and distinguish an unsuccessful statement from an unknown COMMIT outcome. |
| `DBTransaction`, `src/database.h` | A false COMMIT result does not prove rollback. Never credit/release/retry under a new operation ID on that assumption. Reconcile a stable operation key. |
| `IOLoginData::loadPlayerById` / `loadPlayerByName` / `savePlayer`, `src/iologindata.cpp`; `Player::bankBalance`, `src/player.h` | Player saving overwrites SQL bank balance with an in-memory value. Direct SQL payout alone can be lost on the next save. A payout ledger/watermark must protect later saves, including an unacknowledged committed payout. |
| `DatabaseTasks`, `src/databasetasks.cpp` | This uses another retrying connection. It cannot perform exactly-once broker writes independently of ownership transitions. Dispatcher state changes must not be driven by an assumed-success async callback. |
| `Npc`, `src/npc.h` / `src/npc.cpp` | Npc is final, nonattackable and immune. An opt-in controller on an engine-created Npc is preferable to a player summon or duplicate creature system. Do not add an auto-loaded `.npc` file that would spawn a broker in Classic. |
| `Npc::doMoveTo`, `Creature::getPathTo` / `startAutoWalk`, `src/npc.cpp`, `src/creature.cpp` | Reuse native pathing/step scheduling. Revalidate leash, owner session, floor and target before collection and before queued steps. No teleport-to-loot or remote pickup. |
| `Tile::queryAdd`, `src/tile.cpp` | Ordinary NPC bodies block movement. A broker-specific pass-through exception must leave every existing creature unchanged. The broker must avoid occupied/scripted/hazardous destinations. |

## Selected ownership architecture

One opt-in broker account per authenticated **player GUID**, with a separate
session binding to the online player instance. Creature IDs are session-only.
The physical noncombatant dwarf uses an existing look type and the NPC ID space;
it has no master/summon relationship, attack target, XP award or damage role.
Commands operate only on the invoking player's GUID. Multiple brokers have no
exclusive intent reservation over a ground item; the first valid completed claim
wins. Manual moves invalidate the witness and naturally win when processed first.

Only actual spilled, generated, still-valid item witnesses qualify. Currency,
worms, marked/custom/quest items, runes, keys, special container subtrees,
unidentified items, unsafe handlers and unreviewed state never enter escrow.
The reviewed sale catalog is additionally required. No death-time conversion,
price-maximizing NPC scan, physical money creation or denomination exchange occurs.

Grace is measured from each instance's actual visible spill time, not merely
corpse creation. A delayed overflow item gets a fresh full grace interval. Merely
approaching an item reserves nothing. Corpse movement/decay, a manual merge or
item move, trade, disappearance or expired registry entry retires eligibility.
Unclaimed loot remains subject to historical ground/corpse persistence and decay.

After physical approach to within one tile, use **controlled server-owned escrow**,
bounded by a fixed/validated slot limit. This is neither a player's inventory nor
a lootable/tradable creature bag. Actual items remain real, private instances
until a successful sale; the database journal is their durable recovery image.
Never save the same escrow into player inventory/depot/house tables.

## Pickup durability boundary

1. On the dispatcher, revalidate exact witness, current item/count/parent,
   unmarked sale-safe identity, grace, floor, distance, leash, source tile safety
   and escrow room. Exclude house tiles and any source with unsafe notifications.
2. Allocate a stable claim key from a durable server-session ID plus a local
   sequence. Insert a serialized `PREPARED` escrow record and commit it **before**
   detaching the live item. Do not automatically replay individual SQL statements.
3. If preparation is definitely unsuccessful, leave the item untouched. If its
   COMMIT result is unknown, pause this claim and quarantine only that instance
   against native move/merge/remove/transform paths until the same key is read
   back. This is transaction recovery, not an intent reservation. Do not let a
   possibly durable copy coexist with a claimable original.
4. Once preparation is known durable, transfer the complete existing instance
   into private escrow using prevalidated native Cylinder primitives and normal
   notifications; no clone, split, factory, conversion or price credit. Then mark
   it `READY`. A crash between those steps restores the one serialized instance
   from `PREPARED` or `READY`; its original non-house generated ground copy does
   not survive restart. A failed final status update does not lose ownership.
5. On restart, recover prior-session prepared/ready escrow once. A single active
   broker-server database lease prevents another live process consuming the same
   ledger. A lost lease/connection pauses mutations and sales. Recovery validates
   every record and fails closed on inconsistent IDs/counts/catalog state.

If the player leaves during an uncertain claim, cancel/reconcile it before
unlocking its ground item. A crash before a durable prepared claim is established
has the existing unclaimed-ground restart semantics; it must not be reported as
a completed broker pickup. Once ownership is durable, recovery is mandatory.

## Sale and represented travel

Use deterministic states `FOLLOW`, `APPROACH`, `OUTBOUND`, `SELL`, `RETURNING`,
`IDLE`, plus `RECOVERY_PAUSED`. Waiting for grace is an idle/follow decision;
there is no reason to reserve or continually chase an ineligible item. Collection
stops on owner logout/death, incompatible floor/travel, leash failure, blocked
path timeout or target invalidation. Existing escrow remains durable.

Initial town travel may be represented: the dwarf announces departure and leaves
the hunting scene, a durable outbound deadline represents travel to the reviewed
buyer's town, SQL settlement occurs in that sale state, and a durable return
deadline precedes its reappearance near the owner. No live NPC dialogue is
automated. This avoids pretending a local pickup is already an NPC sale or
letting a long town route violate the hunting leash. Offline collection stops;
durable trips can resume on the next owner session without re-selling anything.

A stable trip/sale key identifies a bounded group of escrow records. In one
InnoDB transaction, lock the account/trip/escrow rows, validate prices/counts and
overflow limits, mark those records sold, insert one sale ledger entry, add the
player net to pending balance, and add commission to a separate economy fund.
Only after a known committed result may private live instances be consumed.
An unknown result leaves instances private and unavailable until that same sale
key is reconciled. Repeated settlement observes the existing sale and never
credits it again. A crash after commit cannot restore sold rows as inventory.

Commission defaults to 2500 basis points. Compute `floor(gross * bps / 10000)`
using quotient/remainder arithmetic to avoid intermediate overflow. The player
gets the exact remainder, so player net plus retained commission equals gross.
Round once per sale batch, not separately per item. E.g. 10000 gp splits 7500/2500;
1 gp at 25% splits 1/0. Retained commission has a durable ledger/total and no
spending implementation in this phase.

## Bank payout boundary

Cash-out locks the player's broker account and bank row and writes an immutable
payout ledger in the same InnoDB transaction as bank credit and pending-balance
zeroing. A second attempt sees zero/already-posted state; it creates no coins.
Commands cannot supply another GUID. Offline/restart balances stay durable.

Bank saving must additionally protect committed broker credits that the live
Player has not yet incorporated. Load bank balance and maximum posted payout ID
in one consistent SELECT. Keep that payout watermark in session memory. While
broker persistence is enabled, save bank as the live baseline plus the sum of
posted broker credits newer than that watermark, rather than blindly overwriting
SQL. A confirmed payout/reconciliation folds unseen credits into live memory and
advances the watermark. Repeating an ambiguous save writes the same total; it
does not add the same credit twice. Protect this across ordinary bank operations,
logout and an unknown COMMIT acknowledgement. Disabled-mode SQL remains unchanged.

Use 64-bit unsigned storage/arithmetic with explicit bounds. Broker-originated
balances/payouts should stay within Lua's exactly representable integer range
(2^53-1) until the historical Lua bank API is separately widened. Never silently
wrap, clamp away money or fall back to physical ground coins on failure.

## Reviewed NPC purchase catalog

Start with Sam in Thais, `data/npc/sam.npc`, Home `[32360,32199,7]`.
His explicit single-item `sell` rules at Topic 2 are backed by
`Count(Type)>=Amount -> Delete(Type), CreateMoney`. These are purchases from the
player. Normal greeting/location/conversation are the represented town service;
no quest, vocation or premium condition gates those particular purchase rules.
For example: sword 3264 = 25 gp; leather helmet 3355 = 4 gp; plate armor 3357 =
400 gp. Record exact item/subtype, price, source rule, buyer/station and access
assertion for every entry. Review the confirming transaction as well as text.

Do **not** import every file whose name ends `-b.ndb`: the inspected generic
`gen-t-shield-b.ndb` has sale-looking text but its confirmation uses
`DeleteMoney, Create(Type)`. It is not sufficient evidence of a legitimate NPC
purchase. Sam's backpack quest and other conditional/special offers are excluded.
Catalog changes require review and source identity checks, never a maximum-price
search or live dialogue against private escrow.

## Proposed schema and validated configuration

Use separate Easy Mode InnoDB tables: broker accounts (GUID, pending balance,
trip/session state); server sessions/lease identity; serialized escrow (unique
claim key, owner, item/count/blob, price/catalog snapshot, status, trip ID); trips
and unique sale ledger; payout ledger (owner, immutable credit amount/ID); and an
economy fund/commission ledger. Foreign keys must reference the actual player
schema. No 32-bit player storage keys or physical-container Action IDs are needed.
Migration must be explicit, additive and never auto-run against Classic data.

Requested startup defaults are `easyModeLootBroker=false`, grace 45 seconds,
commission 2500 bps, leash 7 tiles and catalog `data/zedas/broker.xml`. Require
`easyModeEnabled` and visible loot spill for collection. Validate actual booleans,
integers and nonempty paths. Suggested bounds: grace 5–240 seconds (below the
five-minute witness TTL), bps 0–10000, leash 2–12. Any capacity/travel setting
added by implementation also needs explicit bounds/default documentation.

## Required validation before gameplay release

- Native real-engine tests for grace boundaries, actual approach/pickup, exact
  instances, manual/two-broker races, nested/protected provenance, worm/currency
  exclusion, capacity, walls, floor/teleport/logout/death and corpse/item removal.
- Real isolated MariaDB/InnoDB tests with failure injection and process restart
  around prepared-claim commit, detach/status update, sale commit/consumption and
  payout commit/memory sync/save. Exercise lost acknowledgements, not just clean
  rollback. Verify operation replay, aggregate conservation and overflow checks.
- Test the actual player bank load/save integration, including stale live bank
  memory after a committed-but-unacknowledged payout and subsequent normal bank
  changes. A SQL-only payout test does not prove this boundary.
- Verify default catalog prices/confirmation/access against the actual NPC source;
  reject malformed, conditional, mismatched and unknown records.
- Build/test both Lua backends on Ubuntu 24.04 without cotire, plus sanitizers.
  CI databases must be disposable fixtures with no live data or secret dependency.
- Original 7.72 client acceptance for dwarf appearance, movement/leash/pass-through,
  diversion and pickup timing, departure/return, feedback and save/relog. No
  executable/protocol extension, combat, market, buying or denomination work.

These are implementation gates, not claims that the broker or crash recovery has
already been implemented or validated by this audit.
