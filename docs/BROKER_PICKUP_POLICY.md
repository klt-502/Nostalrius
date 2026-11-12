# Field pickup captions and item policy

## Native speech experiment

The collector no longer has `nextPickupSpeech` or the three-second suppression
check. Each normal successfully completed field claim attempts one
`Game::internalCreatureSay(..., TALKTYPE_MONSTER_SAY, ...)` after the real item has
entered private escrow. Failed approaches, invalid/lost claims and uncertain
unacknowledged claims do not produce a success caption. Startup hydration is
ledger recovery, not a new field pickup, and does not fabricate captions.
Routine pickups have no persistent chat/status copy; significant courier/sale
messages retain their accepted console-visible channel and durable receipts.

Source audit: `Game::internalCreatureSay` selects same-floor viewport spectators,
sends through `Player::sendCreatureSay`, `ProtocolGame::sendCreatureSay` and
`AddCreatureSpeak`, and invokes the normal creature speech hooks. Every send
appends a separate 0xAA positional speech event with a statement ID to the output
buffer. This path has no speech-rate timer, same-speaker text replacement or
caption-count cap. Incoming player-chat mute/flood controls are a separate path;
no such controls or native output behavior were changed. Normal socket buffering
and network/disconnect limits remain in place.

The collector still completes at most one ordinary claim per one-second decision
tick, with native approach/path delays. This change does not accelerate pickup.
The proprietary original 7.72 client's display lifetime, overlap, replacement and
maximum simultaneously visible captions cannot be established from this server
source. No executable inspection/modification or custom UI is introduced: test
three adjacent pickups over approximately two seconds and observe native rendering.
Wire tests verify one monster-speech packet per pickup, including the second and
third pickup inside the old suppression interval, and no persistent pickup text.

Caption validation: full normal suite 33/33 passed, including rapid actual speech
packets, failed preparation, invalid items and a manual pickup winning the race.

## Food: corpse contents, never broker stock

Removed from the broker catalog: **3577 meat, 3579 salmon, 3582 ham, 3590 cherry,
3600 bread, 3607 cheese**. These were the only foods among the prior 58 entries.
The approved catalog now has 52 identities and eight buyers; no prices for the
remaining identities changed. The food-only Willie route is removed. Normal NPC
trade, food/eating Lua, nutrition, monster loot generation/rates and fishing/worm
behavior are untouched. Source evidence for the excluded offers remains in the
audit JSON, outside the approved array.

Removed from visual spill eligibility (retained explicitly as corpse contents in
the legacy `corpse-junk` XML class): **3577–3602, 3606–3607, 3723–3732** (38 IDs).
This keeps food inspectable in the corpse/generated ordinary bag; generated bag
shells and manual retrieval remain normal. Native `ItemType::nutrition` also blocks
spill independently of XML category. It uses the existing classification/transfer
checks, so food has no witness/price/claim path and consumes no new broker slots.
The protected quest carrot of doom 3250 was never eligible and stays protected.

| ID | Ordinary food retained inside |
| --- | --- |
| 3577 | meat |
| 3578 | fish |
| 3579 | salmon |
| 3580 | fish |
| 3581 | shrimp |
| 3582 | ham |
| 3583 | dragon ham |
| 3584 | pear |
| 3585 | red apple |
| 3586 | orange |
| 3587 | banana |
| 3588 | blueberry |
| 3589 | coconut |
| 3590 | cherry |
| 3591 | strawberry |
| 3592 | grapes |
| 3593 | melon |
| 3594 | pumpkin |
| 3595 | carrot |
| 3596 | tomato |
| 3597 | corncob |
| 3598 | cookie |
| 3599 | candy cane |
| 3600 | bread |
| 3601 | roll |
| 3602 | brown bread |
| 3606 | egg |
| 3607 | cheese |
| 3723 | white mushroom |
| 3724 | red mushroom |
| 3725 | brown mushroom |
| 3726 | orange mushroom |
| 3727 | wood mushroom |
| 3728 | dark mushroom |
| 3729 | some mushrooms |
| 3730 | some mushrooms |
| 3731 | fire mushroom |
| 3732 | green mushroom |

The version-2 classification still reviews 254 identities: 52 fodder, 157
player-interest and 45 corpse contents. Of these, 209 may spill after every
existing provenance/state/handler/tile check passes. The internal `CorpseJunk`
name is retained for compatibility and now also covers normal food.

## Rings: exhaustive native-slot audit

All 26 `SlotType=RING` identities were checked against the actual `.srv` definitions,
registered use/equipment/movement handlers, spill policy and broker catalog.
None was in the prior broker catalog; none is in the narrowed catalog.
`EasyMode::lootClass` now enforces player-interest for a safe native ring slot,
even if a future XML edit accidentally says broker-fodder. Both the broker price/
claim path and shared private-escrow transfer helper already use this same class
API. Unsafe rings still fail the original state/handler/provenance checks first.

| ID | Ring | Current outcome |
| --- | --- | --- |
| 3004 | wedding ring | Safe passive: player-interest spill; never broker stock |
| 3006 | ring of the sky | Safe passive: player-interest spill; never broker stock |
| 3007 | crystal ring | Safe passive: player-interest spill; never broker stock |
| 3048 | might ring | Protected: retained inside; no broker claim |
| 3049 | stealth ring | Protected: retained inside; no broker claim |
| 3050 | power ring | Protected: retained inside; no broker claim |
| 3051 | energy ring | Protected: retained inside; no broker claim |
| 3052 | life ring | Protected: retained inside; no broker claim |
| 3053 | time ring | Protected: retained inside; no broker claim |
| 3063 | gold ring | Safe passive: player-interest spill; never broker stock |
| 3086 | stealth ring | Protected: retained inside; no broker claim |
| 3087 | power ring | Protected: retained inside; no broker claim |
| 3088 | energy ring | Protected: retained inside; no broker claim |
| 3089 | life ring | Protected: retained inside; no broker claim |
| 3090 | time ring | Protected: retained inside; no broker claim |
| 3091 | sword ring | Protected: retained inside; no broker claim |
| 3092 | axe ring | Protected: retained inside; no broker claim |
| 3093 | club ring | Protected: retained inside; no broker claim |
| 3094 | sword ring | Protected: retained inside; no broker claim |
| 3095 | axe ring | Protected: retained inside; no broker claim |
| 3096 | club ring | Protected: retained inside; no broker claim |
| 3097 | dwarven ring | Protected: retained inside; no broker claim |
| 3098 | ring of healing | Protected: retained inside; no broker claim |
| 3099 | dwarven ring | Protected: retained inside; no broker claim |
| 3100 | ring of healing | Protected: retained inside; no broker claim |
| 3245 | ring of wishes | Protected: retained inside; no broker claim |

3048 has native charges; 3049–3053 and 3086–3100 carry equip transforms, duration,
decay or active state. Their original guards remain unchanged. Special ring of
wishes 3245 remains excluded by exact identity despite its charges being described
in item text. No name-only or zero-current-charge loophole is added. Marking or
scripting a passive ring also defeats spill normally. Other jewelry remains
player-interest under the existing reviewed allowlist; it is not promoted to stock.

## Upgrade and old escrow

**No new database migration.** Schema v2, batch/trip ownership, prices of retained
items, sale settlement, pending/fund balances and bank payout guarantees stay the
same. The collector/courier roles, 20-slot batches, dispatch thresholds, persistent
notices, floor reappearance and exact-witness lifetime are unchanged.

Before installing this narrower catalog, use the accepted previous build to
finish any old batches containing the six food IDs. Stop hunting while draining,
wait for any active runner and explicitly sell the remaining old batch. Check the
backed-up Easy Mode DB with this read-only query before stopping/updating:

```sql
SELECT owner,item,SUM(count) AS quantity
FROM zedas_broker_escrow
WHERE sold=0 AND item IN (3577,3579,3582,3590,3600,3607)
GROUP BY owner,item;
```

The result must be empty for seamless account recovery. Existing pending/fund/
payout/sold records remain untouched; no special cashout or SQL deletion is needed.
Custom configured catalogs must also drop the six food entries/Willie buyer; the
unchanged reviewed equipment-only subsets remain compatible.

If old unsold food is present after updating, the existing catalog-validation
path **pauses that account's recovery and retains its original rows/value**. It
does not silently sell, consume, return, recreate into the world or reprice them.
Use the accepted prior server/data build (fba5113, with the same schema v2), finish
those existing batches, then update again. Never delete/rewrite escrow or ledger
rows as a shortcut. The legacy-food regression proves fail-closed retention with
no sale or balance loss. This is existing recovery behavior, not a new handoff,
refund, disposition or money-generation path.

## Tests and original-client checks

`engine_broker_food_rings` traverses all 39 native edible identities (including the
protected carrot) and all 26 ring slots using generated nested bags. It verifies
unchanged food counts/ring state, passive ring exposure, no food/ring claim or
slot use, ordinary equipment pickup, real manual bread retrieval/eating, and
legacy-food escrow retention. `engine_broker_food_ring_guard` deliberately
mislabels food/rings and verifies the centralized native guards still hold.
The static NPC audit proves the 52 approved entries contain no food or rings,
while preserving the excluded foods' original purchase evidence.

Retest rapid equipment pickups in the original client, observe every requested
caption's native overlap/replacement/lifetime, confirm food remains inside and
manual eating works, check passive versus charged/special rings, then repeat the
accepted runner sale, floor transition, restart and disabled-mode cases.

Validation for this policy update: **35/35 normal tests**, **35/35 ASan/UBSan**
with the disposable MariaDB fixture, including accepted collector/courier,
receipt replay, floor and restart tests. ASan uses `detect_leaks=0` for existing
process-lifetime fixtures; UBSan uses `halt_on_error=1`. No leak-check claim is
made. All 81 compile commands remain free of cotire/prefix/PCH paths. Ubuntu
24.04 CI runs the full suite in both LuaJIT ON and Lua 5.2 OFF configurations.
