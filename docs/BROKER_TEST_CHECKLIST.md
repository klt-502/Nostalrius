# Broker acceptance on the original Tibia 7.72 client

Use a disposable copy of the Easy Mode database for crash/failure tests. Keep
Classic and its database separate. Do not delete journals or restore a partial
backup to simulate a normal crash. No client executable changes are needed.

1. Stop Easy Mode, back up its database, apply `migrations/001_broker.sql` once
   with your database administration tool. Enable `easyModeEnabled`,
   `easyModeLootSpill` and `easyModeLootBroker`. Leave `allowClones=false`. Start
   with commission 2500, leash 7 and the shipped version-2 58-item catalog.
   Routine fodder must collect immediately even if the legacy grace is 45.
   Confirm malformed catalog, missing migration and a second broker-enabled
   process using the same DB fail startup safely. Broker=false must start without
   broker tables/catalog and preserve accepted Wave 2 behavior.
2. Say `!broker hire`. Confirm exactly one visible dwarf (look 69), repeated hire
   creates no additional actor, no summon slot is used, and normal commands/runes
   and auto-target still work. Walk around monsters and the dwarf; check pass-through
   movement, targeting/clicking overlapping creatures, combat damage, path quality,
   no random wandering, no attacking/XP/damage contribution and no lootable bag.
3. Kill monsters producing hatchet 3276, brass/iron/viking helmet 3354/3353/3367,
   brass/plate legs 3372/3557, plate/brass/dwarven shield 3410/3411/3425,
   bow/crossbow 3350/3349, or another approved item. It spills with the ten-thing
   limit, then the dwarf approaches within one tile and can claim immediately.
   Grab one before its claim completes: manual pickup must win naturally. No
   remote reservation, clone, split or forced 45-second window is allowed.
4. In a disposable generated-loot fixture, test Djinn knight armor 3370, a model
   ship 2994 and a spyreport 3232 with inert text/description/writer/date: they
   spill unchanged but are never collected. Existing drops are unchanged; player
   insertion afterward is not a valid generated fixture. Pharaoh decay piece 3235,
   marked quest item, Movement ID item, keys/runes and Sam's special backpack
   3244 stay protected. Rotten meat 3112, bones 3115/3116, moldy cheese 3120,
   dirty cape 3122, twigs 3130 and worms 3492 stay inside. Fish using worms and
   verify classic consumption. Currency still goes only through auto-gold/Gold Bag.
   Approved food/tusks may now be collected at reviewed prices; gems, ordinary
   ammunition and other noncatalog items stay entirely player-controlled.
5. Add ordinary items yourself to a corpse after generation, or drop your own
   sword on its tile. The dwarf must never sell them. Manually pick up a generated
   sword and drop it back: its provenance must remain invalid. Have two players
   hire brokers beside the same exposed loot. Manual pickup and the first broker
   to complete a claim must win once, with one escrow entry and no duplicate value.
6. Block every approach to leftover loot; remove loot while the dwarf is walking;
   move/remove/decay the generating corpse. The dwarf must abandon those attempts
   without reserving, recreating or remotely collecting items. Delay an overflow
   item inside a full corpse tile: when it finally spills, only then may it be claimed. Generated items on a house tile must never enter broker escrow.
7. Move quickly beyond seven tiles, teleport, change floor, log out and die during
   an approach. Collection must stop; the dwarf must not follow across unsupported
   transitions by teleporting. Use `!broker hire` to resume. Already collected
   escrow and balances must survive. Observe whether pausing/re-hiring and normal
   following feel useful during a real hunting route.
8. Inspect `!broker balance`. Carry up to 20 whole item instances; the dwarf should
   depart at capacity, after 15 seconds without another pickup, or on `!broker sell`.
   It leaves the hunting scene, represents 60 seconds for the reviewed town route, sells once,
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

13. Watch pickup: `Got a hatchet.` appears briefly above/near the dwarf in the
    same style as monster noises, with no console/chat copy. Collect three nearby
    items rapidly: no more than one pickup caption per three seconds; items still
    move during silence. Departure says `Off to town!` before disappearance;
    return says `Back from town.` after placement. Neither becomes chat. Check
    `!broker balance` still provides accounting details normally.
14. Verify excluded examples: studded helmet 3376 is approved (20 GP Al Dee),
    leather legs 3559 approved (1 GP Shanar); studded armor/legs and soldier helmet
    are excluded because their proven offers are gated/non-town. Arrows/bolts
    have no proven ordinary buyback offer. Rare/high-value loot must not be
    liquidated at a cheap town fallback. Compare all prices with `NPC_BUY_AUDIT.md`.
15. Floor behavior remains unchanged: pause/despawn and explicit re-hire. The
    proposed delayed floor reappearance is audited only, not implemented.
