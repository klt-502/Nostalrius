# Broker acceptance on the original Tibia 7.72 client

Use a disposable copy of the Easy Mode database for crash/failure tests. Keep
Classic and its database separate. Do not delete journals or restore a partial
backup to simulate a normal crash. No client executable changes are needed.

1. Stop Easy Mode, back up its database, apply `migrations/001_broker.sql` once
   with your database administration tool. Enable `easyModeEnabled`,
   `easyModeLootSpill` and `easyModeLootBroker`. Leave `allowClones=false`. Start
   with grace 45, commission 2500, leash 7 and the shipped broker catalog.
   Confirm malformed catalog, missing migration and a second broker-enabled
   process using the same DB fail startup safely. Broker=false must start without
   broker tables/catalog and preserve accepted Wave 2 behavior.
2. Say `!broker hire`. Confirm exactly one visible dwarf (look 69), repeated hire
   creates no additional actor, no summon slot is used, and normal commands/runes
   and auto-target still work. Walk around monsters and the dwarf; check pass-through
   movement, targeting/clicking overlapping creatures, combat damage, path quality,
   no random wandering, no attacking/XP/damage contribution and no lootable bag.
3. Kill a monster whose actual generated bag contains catalog loot: sword 3264,
   leather helmet 3355, plate armor 3357, or another listed Sam item. It should
   spill visibly with the same ten-thing overflow budget. Wait under 45 seconds:
   the dwarf must leave it alone. Take one manually. After 45 seconds, verify the
   dwarf walks within one tile of remaining catalog loot before the actual ground
   instance disappears. It must not pick up remotely or create replacement items.
4. Put worms 3492, gold/platinum/crystal currency, marked/quest/rune/key loot and a
   special quest container in generated test loot. Worms and protected subtrees
   stay inside; auto-gold/Gold Bag alone handles currency. Fish with ordinary worms:
   historical worm consumption and fishing behavior must still work. Food, gems,
   creature products and noncatalog ordinary gear may spill but stay for manual
   pickup; the broker has no invented prices for them.
5. Add ordinary items yourself to a corpse after generation, or drop your own
   sword on its tile. The dwarf must never sell them. Manually pick up a generated
   sword and drop it back: its provenance must remain invalid. Have two players
   hire brokers beside the same exposed loot. Manual pickup and the first broker
   to complete a claim must win once, with one escrow entry and no duplicate value.
6. Block every approach to leftover loot; remove loot while the dwarf is walking;
   move/remove/decay the generating corpse. The dwarf must abandon those attempts
   without reserving, recreating or remotely collecting items. Delay an overflow
   item inside a full corpse tile: when it finally spills, it gets its own full
   grace. Generated items on a house tile must never enter broker escrow.
7. Move quickly beyond seven tiles, teleport, change floor, log out and die during
   an approach. Collection must stop; the dwarf must not follow across unsupported
   transitions by teleporting. Use `!broker hire` to resume. Already collected
   escrow and balances must survive. Observe whether pausing/re-hiring and normal
   following feel useful during a real hunting route.
8. Inspect `!broker balance`. Carry up to 20 whole item instances; the dwarf should
   depart at capacity, after 15 seconds without another pickup, or on `!broker sell`.
   It leaves the hunting scene, represents 60 seconds to Sam in Thais, sells once,
   and represents another 60 seconds before returning. It must collect nothing
   during the trip. Dismiss/logout and re-hire during travel; deadlines and batch
   ownership must persist without repeat sales.
9. For clean arithmetic, sell only one leather helmet: gross 4, pending 3, retained
   economy funds 1. One dagger: gross 2, pending 2, retained 0. A sword: gross 25,
   pending 19, retained 6. Rounding occurs once per batch, so two swords together
   give gross 50, pending 38, retained 12. Inspect the immutable sale records and
   separate retained `funds`; the commission must not disappear.
10. Say `!broker cashout` twice. Pending becomes zero only with bank credit; the
    second command adds nothing. Check bank, normal deposits/withdrawals, save,
    relog and restart. Speak `balance` / `cash out` near your own dwarf. Another
    player's speech cannot access your account; commands cannot name another GUID.
    No physical coins are created, dropped or used as fallback.
11. On the disposable world, interrupt DB connectivity at collection/sale/payout
    boundaries, then restore it. An uncertain prepared source may remain visible
    but immovable until reconciled. A rolled-back claim unlocks its untouched
    source; a committed claim moves its one existing instance into escrow. Private
    items remain unavailable during uncertain sales. Check escrow/sale/payout
    uniqueness and item+pending+funds+bank conservation across server restarts.
    Automated `engine_broker_restart` exercises abrupt process exits at all three
    committed-but-unacknowledged boundaries; the manual run additionally checks
    client feedback and reconnect behavior under your local database/network.
12. Disable the broker and restart. Confirm ordinary loot spill, worms, auto-gold,
    Gold Bag, endless items, manual target override and Classic consumable behavior
    remain accepted. Keep broker tables/ledgers so re-enabling recovers inventory
    and balances. Do not start the broader market/buying/economy phase yet.
