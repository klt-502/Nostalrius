# Wave 2 local acceptance — original Tibia 7.72

Wave 1 actual-client acceptance is preserved at `v0.6-easy-mode-wave1`
(`ed3c7c6`). Use a separate Easy Mode server/config/database and unchanged 7.72
client. Do not replace the Classic deployment. Record commit, client version,
configuration, before/after coin counts, observed timing and pass/fail for each
check. Native/CI tests do not establish client rendering or live save/relog.

## Build and startup

1. Install the packages and run the ON and OFF builds/CTest commands in
   [BUILD.md](BUILD.md). Both Lua development packages should be installed.
   Configure OFF without `LUA_LIBRARY` or `LUA_INCLUDE_DIR`; verify CMake reports
   Lua 5.2 headers and `liblua5.2`, then builds. In one scratch build directory,
   switch ON → OFF → ON and build/test each selection. An old mixed cache must
   recover without manually setting a library path.
2. Start with the generic example's Easy Mode switches false. Verify normal
   marked/unmarked consumable behavior and normal corpse contents/coin handling.
   With master false, setting the individual Wave 2 flags true must do nothing.
   Invalid nonboolean feature values or a non-string policy path must reject
   startup. `/reload config` must not change any Easy Mode startup switch.
3. For the enabled session use:

   ```lua
   easyModeEnabled = true
   easyModeEndlessRunes = true
   easyModeEndlessManaFluid = true
   easyModeCommands = true
   easyModeAutoTarget = true
   easyModeAutoGold = true
   easyModeGoldBag = true
   easyModeLootSpill = true
   easyModeRespawn = false
   easyModeRespawnPolicyFile = "data/zedas/spawns.xml"
   ```

   Restart. Keep the shipped empty spawn manifest. Retain historical stacking
   settings; no global stacking change is needed.

## Gold Bag

4. As GM create **`/i 2856,1,65005`**. Confirm it is the existing present sprite,
   visually recognizable, opens with five slots and has normal weight. Carry it
   inside two ordinary bags. Create comparison items `/i 2856,1`,
   `/i 2853,1,65005` and `/i 2856,1,65004`; these receive no dedicated preference.
   The reserved ID was audited against this repository's map/scripts, not any
   separately modified live database/map.
5. Put 95 gold in an ordinary backpack and 95 in the marked bag. Kill a normal
   coin-dropping monster within five tiles and line of sight. The marked stack
   fills first; gold/platinum/crystal coins keep their denomination. Repeat with
   the marked bag nested, closed, moved between bags, and stored outside carried
   inventory. Only carried inventory receives preference.
6. Fill all five marked slots. Leave a partial same-denomination stack inside:
   it must fill before fallback. With no remaining merge/slot room, ordinary
   carried destinations receive coins. Fill all destinations and capacity:
   undeliverable coins remain in the corpse. Free capacity for only a few coins
   and repeat: only that actual partial amount moves. Free more and retry.
7. Carry two marked presents: the first in equipped-slot/container breadth-first
   order receives dedicated preference. Rearranging them may change that order.
   Offer the selected bag or its ancestor in trade; it must be skipped while
   offered. Cancel trade and confirm preference resumes.
8. Race a second ordinary player and manual pickup against the same corpse.
   Count all three denominations across both inventories, ground and corpse:
   total value stays constant. Repeated collection cannot award it twice.
   Coins inserted or manually merged into registered corpse stacks after death
   remain manual loot. Transfer the marked bag, save/logout/relog and restart:
   verify Action ID, existing contents and normal item persistence.
9. Restart with only `easyModeGoldBag=false`. Coins follow the accepted Wave 1
   destinations even while the marked present is carried. Test auto-gold off and
   master off separately; the present remains a normal historical container.

## Generated loot spill

10. Kill normal monsters on open ground until they generate reviewed ordinary
    equipment/gems (for example dragons' steel shields/helmets, crossbows or
    diamonds). Keep production loot probabilities unchanged. Verify each exposed
    item appears **above** its corpse, is visible without opening it, and can be
    clicked, dragged and picked up. Verify normal inventory weight/updates.
    An item created with `/i` and later placed into a corpse is intentionally
    **not** a generated-loot fixture and must stay inside.
11. Check one item, multiple items, a gem stack and generated nested bags.
    The generated bags may remain; approved ordinary contents can be exposed.
    A dragon shield, royal helmet or magic plate armor is eligible when it
    actually drops and passes safety checks. Unknown items, scripted melee
    weapons, consumables, keys, marked/quest/custom items and contents of marked
    nested bags must stay in the corpse. See the exact 46-item reviewed list in
    `data/zedas/lootspill.xml`. Automated fixtures cover rare/nested cases without
    changing production rates; original-client rendering of these needs a
    controlled test encounter if waiting for a drop is impractical.
12. With auto-gold on, equipment spills while collectible coins arrive only in
    inventory. Fill capacity/bags and repeat: remaining coins stay in the corpse,
    never duplicated on the ground. Disable only auto-gold and restart:
    noncurrency still spills; coins remain normal manual corpse loot.
13. Crowd a prospective death tile with existing ordinary items. Include a
    player/creature on the tile. The budget is **ten total things**, including
    ground, corpse, creatures and other items. Kill there: excess loot must stay
    inside. Remove one filler: at most one additional item should expose on a
    normal player think; repeat. Verify the corpse and currently exposed items
    remain visible/clickable. Existing manual drops/creature arrivals can still
    crowd a tile under the historical protocol limit.
14. While overflow is pending, manually pick up a generated item, partially move
    a stack, insert a different item, and merge new gems/coins into an existing
    corpse stack. Free tile space and wait. Manually touched/mixed instances and
    later insertions must not inherit automation eligibility. Loot remains
    available manually; repeated notifications/retries cannot duplicate value.
15. Race two clients/manual pickup with retries. Move a pending corpse away and
    back, let a pending corpse decay, and logout/relog while overflow waits.
    No stale callback may expose already moved/decayed loot. Unhandled loot keeps
    historical corpse decay behavior; ground drops do not gain permanent map
    storage. Pick up exposed items, save/relog and verify normal persistence.
16. Test obstructed/special/scripted tiles and crowded/full corpse contents.
    Unsafe destinations retain loot. Restart with only `easyModeLootSpill=false`
    and verify all noncurrency stays in normal corpses, including generated bags.
    Repeat with master false. A missing/invalid spill policy must fail closed
    with a warning while auto-gold retains its independent behavior.

## Respawn infrastructure — isolated reviewed test slot only

17. With respawn off, record an ordinary spawn's Classic disappearance/replacement
    time and nearby-player suppression. Enable it with the **empty** shipped
    manifest and repeat: no world slot accelerates. Boss, rare, quest and raid
    behavior remains historical. A missing policy path must not be read when
    either switch is off.
18. In an isolated map copy, select one verified ordinary slot with a 60-second
    effective base interval. Add only its exact name/absolute XYZ to a separate
    policy using `classification="ordinary"`, `baseSeconds="60"`,
    `intervalPercent="50"`, `minSeconds="5"`, as shown in
    [EASY_RESPAWN.md](EASY_RESPAWN.md). Point the startup path at that test file
    and enable `easyModeRespawn`. Restart. Kill and leave the normal suppression
    area: expect eligibility at about 30–31 seconds plus dispatcher load.
19. Remain nearby past eligibility: no replacement. Leave and verify the same
    sampled duration applies after the last suppression. Test blocked placement,
    then unblock. Another slot of the same species stays Classic. A changed base
    interval, malformed/duplicate policy or unmatched coordinate must leave the
    slot Classic. Population scaling, if deliberately tested, needs an explicit
    `populationFloor` on a base over 500 seconds and samples once per cycle.
    Restore the empty production policy and respawn=false after testing.

## Accepted commands and targeting regression

20. Repeat the essential [Wave 1 checklist](WAVE1_TEST_CHECKLIST.md): `!uh`,
    `!mana`, `!explo`, `!gfb`, exact nested consumables, ordinary consumable
    depletion and shared exhaustion. Auto-target retains its target, manual
    selection overrides it, Escape suspends until `!autotarget on`, and session
    state resets on logout. Verify both Lua server builds where practical.
    The expected UI limitation remains: server auto-targeting need not display
    the original client's normal red square/battle-list selection. No packet or
    executable change was made; see [AUTO_TARGET_PROTOCOL.md](AUTO_TARGET_PROTOCOL.md).

Do not enable a broader respawn policy or begin broker/economy work as part of
this checklist. Wave 3 needs a separate review.
