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

## Consumable commands

`!uh` uses the exact marked UH on self. Commands require `easyModeEnabled`,
`easyModeCommands` and the relevant endless-consumable switch. Only carried
inventory (including closed nested bags) is searched; active trade items and
their contents are excluded. An ordinary item is never substituted or created.
Local SAY without parameters is required. The native bridge delegates to
`Actions::useItemEx` and existing rune/fluid scripts, sharing the existing action
clock and spell exhaustion with mouse use. Requests during action delay fail
without queuing. Offensive bridge requests require a living visible same-floor
monster and normal combat permission; player-controlled summons are excluded.
No separate spell formulas, areas, cooldowns or rune scripts are introduced.

`!mana` uses 2874/65004/subtype 10 on self through the existing fluid action,
restoring the historical random 50–100 mana and sharing normal item timing.
Native tests cover wrong fluid subtype, real mana gain, persistence and repeated
requests. Client check: `/i 2874,10,65004`, drain mana, use `!mana`, then alternate
with an ordinary mana vial; verify timing, retained marked fluid and ordinary
empty-vial behavior. Rune exhaustion is not newly imposed on historical fluids.

`!explo` uses 3200/65003 centered on the currently attacked monster. Explosion
retains its existing cross-shaped area; it is not converted into a single-target
spell. Native tests exercise missing/player targets, center/adjacent damage,
diagonal exclusion, charge persistence and repeat rejection. Client check:
`/i 3200,6,65003`, attack a monster and use the command; test target loss, walls,
range, floors, protection zones and switching targets before the next command.

`!gfb` uses 3191/65002 at the current monster target's position, preserving the
existing circular fire area and combat rules for every affected creature.
Native tests verify actual center/diagonal damage, outside-area exclusion,
wall blocking, protection zones and charge persistence. Client check:
`/i 3191,4,65002`, select a monster, use `!gfb`, then change targets and repeat
after normal exhaustion. Verify area placement, range/floor rejection, target
death/disappearance and alternating mouse/command usage. Existing area-spell
collateral/PvP behavior remains; selecting a monster is not a safe-area filter.

Client checks: mark a nested UH with `/i 3160,1,65001`; damage yourself and use
`!uh`; confirm healing, retained charge, paralysis removal and normal exhaustion.
Test missing/wrong items, low magic level, trading the containing bag, and
alternating the command with ordinary mouse use. Verify text-hotkey behavior
and movement stopping in the unchanged 7.72 client.

## Auto-target

Requires `easyModeEnabled` and `easyModeAutoTarget`. It starts OFF each login.
Use `!autotarget on` to start/resume, `!autotarget off` to stop, and
`!autotarget status` to inspect. Manual attack selection always takes priority;
manual cancel/Escape and manual follow suspend automation until explicit resume.
Turning automation off only clears its own automatic target, not a manual target.
Repeated `!autotarget on` is idempotent: automatic ownership and an ongoing
unreachable timer remain intact. A regression test repeats it before and during
a blockage and still requires release at the original deadline.

Each normal player think selects a living visible same-floor attackable hostile
monster in the 7-by-5 viewport only when no target exists. Player-controlled
summons (including master chains), players and NPCs are excluded. Highest current
absolute HP wins; ties use creature ID. Existing targets are retained when a
stronger monster appears. Automatic invalid/dead/disappeared targets are released;
unreachability must last 3000 ms (`AUTO_TARGET_UNREACHABLE_MS`) before release.
Reachability uses normal weapon sight/range or the existing bounded pathfinder.
Attack speed, chase mode, combat permission and exhaustion remain unchanged.
State/IDs/timers are session-only and never saved. Auto-target does not cast runes.
Disconnect also suspends automation and releases its automatic target. Native
tests exercise real protocol visibility, current-HP ranking, ineligible hostiles,
invisibility, summons, target retention, manual override/Escape/resume/follow,
death and a blocked route immediately before/at the three-second deadline.

Client checklist: enable/resume, observe attack/chase and target indicators;
introduce stronger and wounded monsters; manually select another target; press
Escape and verify no reacquisition; resume; block a route for over three seconds;
try stairs, protected tiles, invisibility, summons and logout/relogin. Server
target selection does not promise that an unchanged client's red target indicator
or battle-list selection mirrors it; validate those visuals in Tibia 7.72.

## Auto-gold

Requires `easyModeEnabled` and `easyModeAutoGold`. Collection runs during normal
player think, independently of auto-target and consumable commands. Only ordinary
3031/3035/3043 currency captured after monster loot generation is considered,
including generated nested bags. Access is first successful transfer wins; corpse
owner/kill credit does not restrict this feature. Player corpses are not registered.

The collector moves existing stacks using `Game::internalMoveItem`, merges matching
stacks, searches carried nested containers explicitly, and respects item capacity,
slot availability and active trades. It never calls a money/item factory, changes
coin denominations, falls back to the ground, or changes global stacking settings.
Partial source-stack changes are measured even if the move reports an error.
Currency that cannot fit remains in the corpse. Marked/customized currency and
currency inserted after registration are excluded.

Collection requires same-floor visible line of sight within 5 tiles. Corpse handles
are reference-counted and invalidated on relocation, removal, ID/decay change or
after 300000 ms. The session registry retains at most 1024 entries, evicting the
oldest without removing its loot. These limits are centralized in `easymode.h`.
No financial tables or player storage keys are introduced; normal inventory saving
persists pickups. No player action/exhaustion clock is changed by collection.

Native tests check deep nested coins, all three denominations, same-denomination
merging with historical stacking disabled, protected and subsequently inserted
currency, duplicate notifications/reprocessing, competing collectors, exact value
conservation, five-coin partial capacity and safe corpse relocation.
They also execute a real monster death/post-loot callback, protect an active
corpse-item trade, retry after trade cancellation, and handle manual removal and
corpse destruction across engine cleanup. Separate cases exercise defaults and
each command/target/gold feature gate; saturated destinations retain their coins.

Client checklist: kill monsters containing coins/nested loot; verify stack updates,
normal noncurrency loot, and value conservation. Fill bags and capacity, collect
part of a stack, free space and retry. Race another collector/manual looting, move
or decay a corpse, test walls/floors, relog after saving, and repeat with flags off.

## Automated verification

Use [WAVE1_TEST_CHECKLIST.md](WAVE1_TEST_CHECKLIST.md) for the full local gameplay
acceptance sequence, disabled-mode checks and exact GM item-creation commands.

Native CTest scenarios link the same engine library as the server and load real
item, rune, fluid and vocation definitions in a temporary synthetic map. They
need no database or client. Cases verify master/feature gates, rejected config
types, attribute round trips, ordinary consumption with Easy Mode disabled,
nested item selection, actual UH healing, magic-level checks and shared exhaustion.

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
