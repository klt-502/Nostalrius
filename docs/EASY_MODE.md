# Zedas Easy Mode — Wave 1

Work branch: `zedas-easy-mode`. Preserved checkpoint:
`v0.5-endless-consumables` (`409f173caa7d4279af8a84ee944f78ce06cd7a85`).
Keep the Classic checkout, configuration, database and binary separate.
Wave 1 does not change spawns, NPC sales, XP, skills, monster statistics or drop rates.

## Configuration

All six options are boolean, default to `false`, and require a server restart.
`/reload config` does not change them. Missing options are false; nonboolean
values are rejected on startup. Both master and feature switches must be true.

| Option | Enables |
| --- | --- |
| `easyModeEnabled` | Master permission for all Easy Mode features |
| `easyModeEndlessRunes` | Exact marked UH, GFB and Explosion rune charge exemptions |
| `easyModeEndlessManaFluid` | Exact marked mana-vial drink exemption |
| `easyModeCommands` | Checked consumable commands, as implemented below |
| `easyModeAutoTarget` | Session-only automatic targeting, as implemented below |
| `easyModeAutoGold` | Nearby generated-corpse currency collection, as implemented below |

To preserve the v0.5 endless functionality in an Easy deployment, explicitly
enable the master and both endless-consumable flags. The generic example keeps
everything disabled. With either required flag off, marked items consume
normally, subject to existing historical settings such as `removeChargesFromRunes`.

## Reserved Action IDs

`src/easymode.h` is the authoritative registry. No other Action IDs are reserved.

| Item ID | Action ID | Meaning | Additional requirement |
| --- | --- | --- | --- |
| 3160 | 65001 | Endless Ultimate Healing | Existing rune requirements |
| 3191 | 65002 | Endless Great Fireball | Existing rune requirements |
| 3200 | 65003 | Endless Explosion | Existing rune requirements |
| 2874 | 65004 | Endless Mana Fluid | Mana subtype 10 |

Wrong pairings, ordinary items and other fluid containers receive no exemption.
Existing spell/action scripts still apply effects, costs and exhaustion. Mana
endlessness applies to drinking; ordinary pouring/filling behavior remains.

## Verification

`/i item,count,actionId` validates the optional Action ID before creating items.
Only integers 1–65535 are accepted. Malformed, fractional and out-of-range input
creates nothing. The two-argument form and key-number argument keep their meaning.
Empty comma fields retain their positions; additional arguments are rejected.
`create_item_validation` exercises the real Lua script with a recording item factory.

See [BUILD.md](BUILD.md) for compile/CTest commands. The consumable registry test
checks correct pairs, cross-pair failures, unrelated items and wrong fluid types.
Actual-client testing: use marked and ordinary consumables with master/feature
flags on and off, verify rune exhaustion and fluid timing, then relog and restart
to verify Action IDs, charges and subtype persistence. Use a separate test DB.
