# Generated corpse loot spill

`easyModeEnabled = true` and startup-only `easyModeLootSpill = true` permit
conservative exposure of generated monster loot. Both default false. No player
corpse, loot generation, drop probability, item definition or global move rule
is changed. The policy is `data/zedas/lootspill.xml`, read once on first use;
restart after changing it. A missing/invalid identity policy fails closed.

## Provenance and exclusions

`Monster::dropLoot` already calls `EasyMode::onLootGenerated` after the existing
loot generator finishes. The same bounded reference-counted corpse registry as
auto-gold captures eligible existing instances at that point. It is never
rescanned to capture later player insertions. Empty processed entries remain
as tombstones until the registry's normal expiry/eviction. Generated ordinary
unmarked bags are traversed, but the bags themselves remain in the corpse.
An unsafe/marked nested container protects its entire subtree from spill.

The version-2 identity policy reviews **254 identities**: **52 broker fodder**,
**157 player-interest** and **45 corpse contents** (legacy XML class `corpse-junk`). The 209 spill identities still
require exact ID/name, safe state, safe handlers and tile room. A class does not
bypass provenance or native movement checks. Food, worms and corpse junk stay inside. Only
ordinary generated shells 2853–2872 are traversed; special/quest/custom shells
protect their entire subtree.

Eight reviewed decorative/quest identities were added: 2994, 3001, 3002, 3231,
3232, 3233, 3234 and 3243. The last five may preserve inert description/text/date/
writer attributes. All Action IDs, Movement IDs, quest chest/key/charge/decay
state and Easy Mode markings remain protected. See `NPC_BUY_AUDIT.md` for exact
source mechanics; these additions create no quest/drop-table entries.

The narrowly reviewed stock `misc/weapons.lua` handler may be compatible with
ground movement; its onUse function does not dispatch from this transfer. Loaded
registration and the action library must retain their expected fingerprints.
`MultiUse` remains limited to reviewed ordinary melee/distance/ammunition with
known compatible handlers. Ordinary food is explicitly non-spill, including the
broader reviewed food list. Native nutrition is also a hard exclusion, so a data
misclassification cannot re-enable food spill. `misc/food.lua`, food registrations,
item nutrition and fishing/worm mechanics are unchanged. Native equipment-only
handlers retain their existing safety rules; no other movement handler is relaxed.

Charged amulets, ice rapiers, active/dormant transforming rings, runes, fluids,
unsafe quest tools and unidentified items intentionally remain inside. Passive rings
and amulets without that state can move unchanged. No charge/duration/transform
exception is introduced. See [LOOT_SPILL_POLICY.md](LOOT_SPILL_POLICY.md) for
category/ID details, the source audit and the exact retest checklist. Custom data
changes need a fresh review; names alone do not establish safety.

Currency remains in the corpse and its generated containers for the accepted
auto-gold path or manual pickup. Spill never moves, credits or recreates coins.
Consequently coins successfully auto-collected cannot also be spilled; coins
that cannot fit remain available in the normal corpse tree. Noncurrency spill
runs immediately after the completed post-loot snapshot. Pending overflow is
retried on normal player think while the same registry entry remains valid.

## Instance movement and the 7.72 limit

The historical `Game::internalMoveItem` clones stackable remainders even for a
whole-stack move. The narrowly scoped spill helper therefore transfers complete
existing instances with native Cylinder `queryRemove`, Tile `queryAdd` and
`queryMaxCount`, `removeThing`/`addThing`, and the normal post-remove/post-add
notifications. It never calls an item factory, changes the stack count, merges,
splits, deletes or converts an item. The destination receives the same pointer.
Ordinary down items are inserted at `Tile::getBeginDownItem`, above the corpse
already there. Historical manual movement and container stacking are untouched.

The engine accepts far more items than the client can describe: `Tile::queryAdd`
allows up to 65535 tile items, while `ProtocolGame::GetTileDescription` stops at
**ten total things**. Ground, all creatures and all items count toward a strict
conservative ten-thing budget before each spill instance. Overflow stays inside
the corpse and retries when room becomes available; there is no ground fallback
to another tile. This keeps both the newly exposed items and existing corpse
within the current ten-thing description. Later unrelated manual drops or
creature arrivals can still crowd a tile under historical map behavior.

No spill goes to a tile without ground, blocked/hazardous/special tile flags,
registered position movement handlers, marked/unsafe resident items or
collision/separation handlers. Native destination queries still run with no
limit-bypass flags. Uncertain destinations preserve the loot in the corpse.

## Races and lifetime

Game operations run on the existing dispatcher. Each candidate is checked again
for original-corpse containment, eligibility, trade state and destination safety
before movement. A complete transfer removes it from the pending list. Manual
pickup, a competing collector, repeated notifications and retry cannot credit
or duplicate it. Corpse relocation, removal, ID change/decay, five-minute expiry
or oldest-entry eviction cancels pending exposure without touching remaining
loot. The registry is session-only and does not persist across server restart.

Native item move/add/remove/transform paths conservatively invalidate touched
snapshot instances (and moved-container descendants), including a destination
stack receiving a manual merge. This prevents newly inserted quantities from
inheriting generated-loot eligibility through an existing stack. A failed manual
move attempt may also retire that candidate; the item remains available manually.
The collector's own scoped source moves retain their existing partial-stack
eligibility. Hooks are inert when both registry features are disabled.

Collected inventory uses historical player saving and item serialization.
Ground loot and corpse decay/save behavior remain historical: spill does not
introduce permanent map-drop storage or guarantee survival across server restarts.
Loot on the ground is open to normal first-come pickup. No owner entitlement,
broker, NPC valuation, sale or pending balance is introduced.

## Validation

Native tests exercise pointer/count preservation, multiple/nested/rare equipment,
protected and subsequently inserted items, currency conservation, scripted item
exclusion, tile overflow/retry, manual pickup, repeated processing, competing
players, relocated/removed/decayed corpses and engine cleanup. They use real item
definitions and the production movement code, without a database or client.
`engine_spill_hunting` validates all 254 classified identities against the real item catalog,
stock use handlers and native equipment registrations. It covers mixed nested
weapon/armor/product/ammunition/jewelry loot and retained food, unchanged instances/counts,
auto-gold in the same bag, weapon-filled tile overflow, an actual monster death,
and refusal after custom use/movement handler reloads or stock-file changes.

The original Tibia 7.72 client must verify visual ordering, click/drag behavior,
ten-stack rendering with creatures present, container updates and relog/save.
Use the complete [Wave 2 checklist](WAVE2_TEST_CHECKLIST.md) before deployment.

Worms (3492) deliberately remain inside corpses/generated bags for classic fishing.
No fishing action or consumption behavior is changed.

Current optional broker integration is documented in [BROKER_LOGISTICS.md](BROKER_LOGISTICS.md).
Qualified generated fodder has a separate bounded witness lifetime; this does not
change the short-lived currency/spill snapshots or the spill eligibility policy.
