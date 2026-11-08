# Wave 1 local gameplay acceptance

These are manual tests for the original Tibia 7.72 client. Automated tests do not
establish the client UI, input binding, network updates or live SQL save behavior.

## Prepare an isolated Easy deployment

1. Use the separate Easy checkout (`12_Codex/Zedas-Easy-Mode`), not the Classic
   deployment. Fetch and fast-forward `zedas-easy-mode`. Keep the existing
   `v0.5-endless-consumables` tag/backup and Classic checkout untouched.
2. Follow [BUILD.md](BUILD.md), run CTest, then deploy the rebuilt `tfs` together
   with this branch's matching `data/` files. Do not run the new fluid scripts on
   the old v0.5 binary: they use a new native binding.
3. Use a separate test database and local `config.lua`. Keep historical XP,
   skills, loot, money and spawn settings unchanged. Do not put credentials in Git.
4. For the enabled tests, set the following and restart the Easy server:

   ```lua
   easyModeEnabled = true
   easyModeEndlessRunes = true
   easyModeEndlessManaFluid = true
   easyModeCommands = true
   easyModeAutoTarget = true
   easyModeAutoGold = true
   removeChargesFromRunes = true
   ```

5. Use a GM only to create/inspect test items. Perform gameplay with a regular
   character: GM flags can bypass magic levels, exhaustion, protection and capacity.

## Create and validate consumables

Create these with the GM and hand them to the regular test character:

```text
/i 3160,1,65001
/i 3191,4,65002
/i 3200,6,65003
/i 2874,10,65004
```

Also create ordinary versions without the third argument, plus a mismatched UH
with `/i 3160,2,65002`. Keep separate counts/containers so changes can be observed.
Put correctly marked items inside a bag inside a closed backpack.

- `/i 3160,1,65001.5`, `/i 3160,1,0`, `/i 3160,1,65536`,
  `/i 3160,1,nope` and `/i 3160,1,` must each create **zero** items.
- With only an ordinary/mismatched item carried, its corresponding Easy command
  must fail. Mouse use should retain normal consumption and restrictions.
- With the matching item carried, `!uh` heals self and dispels paralysis; `!mana`
  restores 50–100 mana to self. Charges/fluid remain. Verify ordinary versions
  still consume, including when used between command attempts.
- Test a regular character below the required magic level. Repeated commands and
  alternating command/mouse use must not bypass spell exhaustion or item timing.
- Offer the containing backpack in a trade. Its marked items must be unavailable
  to commands until the trade is canceled.

## Offensive commands and normal combat rules

- Attack a monster, then use `!explo`: damage must use the existing cross around
  that monster's current position. `!gfb` must use the existing circular fire area.
- Change targets and cast again after normal exhaustion. Verify no old target or
  old tile is reused. Clear the target, kill it, or let it disappear: commands fail
  safely and do not consume the endless item.
- Repeat behind a projectile-blocking wall, outside normal range, across floors,
  and from protection zones. Existing rejection behavior must hold.
- Player/NPC targets and player-controlled summons must not be command targets.
  Existing area-spell collateral rules still apply to other creatures in the area.
- Test typed local commands, client text-hotkey bindings where supported, held-key
  repetition, messages/effects, and stopping movement as with ordinary item use.
  Whisper/channel commands or commands with extra parameters must not cast.

## Auto-target

- On login, `!autotarget status` reports off. With nearby monsters, there should be
  no automatic acquisition until `!autotarget on`.
- Present healthy and wounded monsters of different species. Use a GM observer's
  look information to compare current absolute HP. The highest eligible **current**
  HP wins, not max HP or percentage. Players, NPCs, nonhostiles, unattackable or
  invisible creatures, other floors and player-controlled summons are excluded.
- Introduce a stronger monster after acquisition: it must not steal the target.
  Repeat `!autotarget on`: it must not reset ownership or the failure timer.
- Manually select another target: the selection must win. Press Escape/cancel:
  no automatic reacquisition occurs until `!autotarget on` is issued again.
- Try manual follow and `!autotarget off`. Turning automation off must not clear
  an unrelated manually selected target.
- Kill/remove the automatic target. Block its route and line of fire for over
  three seconds, then reopen them. Verify bounded release and subsequent normal
  acquisition. Brief blockage should retain the target.
- Verify attack speed, chase/stand mode, stair behavior, disconnect/reconnect and
  logout/login. A new/reconnected session must require explicit resume.
- Observe the red square and battle list separately from actual server attacks.
  Record any mismatch: an unchanged client may not mirror server-selected targets.

## Auto-gold

- Kill a monster near the character. Only generated ordinary gold/platinum/crystal
  coins should move, including coins in nested loot bags. Other loot remains.
- Existing compatible stacks should fill before new slots. Verify that unrelated
  historical manual stacking settings retain their normal behavior. Coin
  denominations are preserved; this wave does not exchange gold for platinum.
- Total coins/value in inventories plus corpse must remain constant. Wait through
  multiple think cycles; there must be no recurring credits.
- Fill capacity almost completely so only part of a stack fits. The exact remainder
  must stay in the corpse. With full bags/slots/capacity, no coins may appear on the
  ground. Free space and retry while the corpse remains eligible.
- Use two regular clients near one corpse and race automatic/manual pickup. Either
  may collect first; combined value must stay constant. No kill ownership is needed.
- Offer eligible currency in a trade; it must remain untouched until cancellation.
  Marked/customized currency and currency added to a registered corpse later must
  remain untouched. Player corpses must not receive automatic collection.
- Test walls, floors, range beyond five tiles, moving the corpse and corpse decay.
  The bounded registry expires entries after five minutes and retains at most 1024.
- Save, log out, log back in and restart the test server. Verify actual SQL-backed
  inventory persistence; the corpse registry itself is deliberately session-only.

## Disabled and regression checks

Restart with `easyModeEnabled = false`, then independently with each feature flag
false. Marked consumables must follow historical consumption; commands/automation
must be unavailable as applicable. `/reload config` does not change Easy flags.
Verify ordinary fluids, rune charges, manual target/cancel/follow, manual looting,
trading and inventory capacity. No database migration is required.

Do not begin broker, sales, market, respawn or client-executable work during this
acceptance pass. Report failures with the branch commit, configuration, character
flags, item ID/Action ID/subtype, positions and reproduction steps.
