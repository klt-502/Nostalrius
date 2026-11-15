# Historical world-spawn matrix and respawn audit

**Historical snapshot audit at 9100ddf. The original audit changed no gameplay.
Later runtime mechanics are documented in [EASY_RESPAWN.md](EASY_RESPAWN.md).
No proposed area below has been encounter-verified or activated.**

The runtime source was inspected at `9100ddf015aac7b57a69ba46af3a1c273861a607` on
`zedas-easy-mode`. Input SHA-256 hashes, all source groups, monster definitions,
flag provenance, diagnostics, cluster membership and proposed pilot membership
are recorded in [SPAWN_MATRIX.json](SPAWN_MATRIX.json). The runtime contract makes
default regeneration fail if the inspected snapshot changes. Use
`python3 tests/audit/spawn_matrix.py --historical --check` to regenerate this frozen
audit from its pinned Git source/config snapshot; world map/XML hashes must still
match. This does not describe subsequent runtime modifications.
There is no generated timestamp or moving HEAD hash, so unchanged inputs produce
byte-identical artifacts.

## Authoritative source and scope

The tracked example selects `mapName = "map"`.
`Game::loadMainMap` opens **`data/world/map.otbm`**. Its actual decoded
OTBM map-data attribute **11** contains **`spawns.xml`**;
`IOMap::loadMap` prepends the map's directory. The authoritative world monster
source is therefore **`data/world/spawns.xml`**, not the Easy manifest. The map
header describes "Saved with Remere's Map Editor 3.6", 'TibiaClassic Map',
OTBM version 2, dimensions
65000 × 65000.
If that OTBM attribute were absent, `IOMap::loadSpawns` would use the literal
`mapName + "-spawn.xml"` relative to the working directory, with **no** automatic
`data/world/` prefix. That fallback is not used by this map.

This is the tracked repository/example world, not an inspection of a running
operator's private `config.lua`. No live configuration was supplied. A deployment
using another map/config needs its own audit. Raids, summons and script-created
encounters are separate creation paths and are not added to the world-slot matrix.

- Monster `x`/`y` attributes are signed offsets from XML `centerx`/`centery`.
  Absolute `x = centerx + offset_x`, `y = centery + offset_y`; absolute `z = centerz`.
  The child monster `z` attribute is **ignored** by this loader. All current child
  Z values agree with their group (0 mismatches).
- The source casts offsets through `boost::lexical_cast<uint16_t>` and then
  narrows the final `Position` coordinates. Negative offsets wrap in that cast;
  for every current valid position the result equals signed center + offset.
  A compiled probe of the actual `pugicast.h` confirmed `-1 -> 65535` and
  `-19 -> 65517`. The extractor checks both calculations and rejects coordinate
  overflow rather than silently wrapping malformed future data.
- `spawntime` is integer **seconds**, multiplied by 1000 for `sb.interval`.
  The loader accepts only raw XML intervals strictly above 1000 ms. It is an
  eligibility interval, not a promised wall-clock death-to-reappearance time.
- All 23,311 current slots omit direction. The engine's
  explicit default is north (`0`); CSV `direction_defaulted=true` records that
  this value was supplied by the loader, not present in the source.
- XML radius is retained as source metadata. Missing radius would mean `-1`.
  `Spawns::isInZone` describes an inclusive XY square and ignores Z; `-1` means
  unrestricted. However, **`Spawn::isInSpawnZone` has no production callers** in
  this tree. Monster despawn range instead uses its slot `masterPos` and the
  global `deSpawnRadius`/`deSpawnRange` (example 50 tiles / 2 floors). Do not
  mistake the XML radius for a respawn timer, global monster cap or proven room.
- Crucially, the loader creates a separate runtime `Spawn(pos, radius)` **inside
  the monster-child loop**, centered on the monster's absolute position. An XML
  group containing several monsters does not become one shared runtime group.
  957 empty XML groups create no runtime `Spawn` objects.

### Global override

The example explicitly sets **`rateSpawn = 0`**:
no override is active in the tracked example. A nonzero value **replaces each
slot's interval in seconds**; it is not a multiplier or population setting.
The compiled ConfigManager fallback when the key is omitted is **1**, which would
activate a one-second replacement after the XML validity check. Keep the
example's explicit zero when preserving these historical intervals. This audit
does not change either default or claim to know a private live configuration.

## Outputs, ordering and row identity

- [SPAWN_MATRIX.csv](SPAWN_MATRIX.csv): UTF-8 comma-delimited header plus one row per
  monster child. Integers remain integers; booleans are `true`/`false`.
- [SPAWN_MATRIX.json](SPAWN_MATRIX.json): `columns` plus row arrays (one array per
  line), avoiding millions of repeated keys. Recover objects with
  `dict(zip(data["columns"], row))`. Metadata contains the underlying source,
  group records, definition-file/flag sources, hashes and complete summaries.
- Sort: **Z → Y → X → case-insensitive monster → source group → source child**.
  Group and child indexes are one-based XML **element** positions; child indexes
  would include NPC children as well. `source_line` and the two indexes trace
  each row directly to its original monster element.
- Offset coordinates, ignored source Z, XML center/radius, direction default and
  dwarf cluster ID are retained. No same-species or same-coordinate rows are
  collapsed. There are 0 repeated monster/XYZ keys
  in the current input; future duplicates remain separate and are surfaced.
- `hostile` and `attackable` come from the registered monster definition selected
  through `data/monster/monsters.xml`, using only each flag element's **first**
  attribute as the engine does. Missing flags use the actual constructor defaults
  (`true`/`true`), and provenance is explicit in JSON. They are not inferred from
  monster names, damage, HP or appearance.

## Overall counts

| Metric | Value |
| --- | --- |
| XML spawn groups | 9,615 |
| Nonempty XML groups | 8,658 |
| Empty XML groups | 957 |
| Raw monster slots / potential runtime Spawn objects | 23,311 |
| Unique case-insensitive monster species | 135 |
| Non-hostile slots | 1053 |
| Non-attackable slots | 0 |
| Minimum original interval | 60 seconds |
| Maximum original interval | 20000 seconds |
| Median original interval | 600 seconds |
| Mode original interval | 600 seconds: 19,074 slots (81.82%) |
| Species with one interval | 55 |
| Species with multiple intervals | 80 |


These are definitions, not a claim that every slot successfully places a live
monster at startup or that tiles are navigable. Placement can fail independently.

### Distribution by floor

| Z | Slots |
| --- | --- |
| 1 | 23 |
| 2 | 51 |
| 3 | 93 |
| 4 | 195 |
| 5 | 299 |
| 6 | 643 |
| 7 | 5216 |
| 8 | 3064 |
| 9 | 3370 |
| 10 | 2493 |
| 11 | 1696 |
| 12 | 1251 |
| 13 | 1844 |
| 14 | 1990 |
| 15 | 1083 |


### Every actual original interval

No invented bins or rounding are used. The complete distribution is:

| Seconds | Slots |
| --- | --- |
| 60 | 112 |
| 70 | 12 |
| 100 | 13 |
| 190 | 1 |
| 200 | 39 |
| 210 | 8 |
| 300 | 148 |
| 350 | 26 |
| 380 | 3 |
| 400 | 302 |
| 444 | 3 |
| 450 | 55 |
| 460 | 4 |
| 500 | 1252 |
| 543 | 2 |
| 550 | 67 |
| 580 | 2 |
| 590 | 2 |
| 600 | 19074 |
| 650 | 263 |
| 680 | 8 |
| 700 | 572 |
| 720 | 2 |
| 750 | 128 |
| 760 | 1 |
| 770 | 4 |
| 777 | 10 |
| 800 | 359 |
| 831 | 2 |
| 850 | 48 |
| 880 | 2 |
| 888 | 1 |
| 890 | 2 |
| 900 | 398 |
| 950 | 27 |
| 990 | 8 |
| 999 | 13 |
| 1000 | 81 |
| 1001 | 1 |
| 1100 | 15 |
| 1200 | 95 |
| 1300 | 8 |
| 1400 | 5 |
| 1500 | 13 |
| 1600 | 18 |
| 1700 | 1 |
| 1800 | 15 |
| 1900 | 1 |
| 2000 | 15 |
| 2200 | 3 |
| 2400 | 5 |
| 2600 | 7 |
| 2700 | 1 |
| 2800 | 2 |
| 3000 | 2 |
| 3600 | 19 |
| 6000 | 8 |
| 7200 | 5 |
| 7500 | 1 |
| 8000 | 3 |
| 9000 | 3 |
| 9999 | 6 |
| 19200 | 1 |
| 20000 | 4 |


The requested illustrative 30/120/180-second bins do not occur in this data.

## Requested hunting species

| Exact source name | Slots | Original intervals (seconds: slot count) |
| --- | --- | --- |
| dwarf | 247 | 500s: 3; 600s: 238; 700s: 6 |
| dwarf soldier | 369 | 500s: 9; 550s: 3; 600s: 348; 750s: 6; 800s: 3 |
| dwarf guard | 338 | 600s: 329; 700s: 2; 900s: 5; 8000s: 2 |
| dragon | 179 | 500s: 1; 550s: 2; 600s: 81; 700s: 8; 800s: 6; 850s: 1; 900s: 32; 950s: 10; 999s: 3; 1000s: 10; 1100s: 6; 1200s: 3; 1500s: 2; 1600s: 5; 1800s: 1; 2000s: 1; 2600s: 2; 3000s: 1; 3600s: 3; 7500s: 1 |
| dragon lord | 98 | 550s: 1; 600s: 69; 900s: 2; 950s: 1; 999s: 1; 1000s: 4; 1100s: 3; 1200s: 2; 1600s: 1; 1700s: 1; 1800s: 3; 2200s: 3; 2600s: 1; 2800s: 2; 3000s: 1; 3600s: 1; 7200s: 2 |
| behemoth | 57 | 500s: 1; 600s: 19; 700s: 14; 800s: 1; 900s: 21; 6000s: 1 |
| hydra | 40 | 600s: 39; 2400s: 1 |
| orc | 205 | 300s: 3; 350s: 6; 400s: 16; 500s: 2; 600s: 143; 650s: 4; 700s: 8; 800s: 14; 900s: 2; 1000s: 5; 1200s: 2 |
| minotaur | 343 | 300s: 5; 400s: 14; 450s: 5; 500s: 19; 600s: 202; 650s: 25; 700s: 57; 750s: 9; 800s: 2; 850s: 4; 900s: 1 |
| cyclops | 263 | 300s: 2; 400s: 13; 500s: 16; 550s: 12; 600s: 143; 650s: 4; 700s: 19; 750s: 20; 800s: 21; 850s: 6; 900s: 7 |
| demon | 23 | 600s: 10; 700s: 2; 800s: 2; 999s: 1; 1000s: 1; 6000s: 3; 20000s: 4 |
| giant spider | 86 | 600s: 60; 700s: 1; 800s: 3; 850s: 1; 900s: 5; 999s: 1; 1000s: 1; 1200s: 2; 1400s: 3; 1500s: 5; 1600s: 1; 2600s: 1; 6000s: 1; 19200s: 1 |


All twelve requested names are present. Case and spacing in CSV match the actual
spawn file; registered display names/definition files are separately available in JSON.

## Species consistency

55 species have a single historical interval;
80 use different values at different locations. A species-wide
replacement would erase intentional location differences. Notable examples:

| Species | Slots | Intervals | Minimum | Maximum |
| --- | --- | --- | --- | --- |
| dwarf | 247 | 3 | 500 | 700 |
| dwarf soldier | 369 | 5 | 500 | 800 |
| dwarf guard | 338 | 4 | 600 | 8000 |
| dragon | 179 | 20 | 500 | 7500 |
| dragon lord | 98 | 17 | 550 | 7200 |
| behemoth | 57 | 6 | 500 | 6000 |
| hydra | 40 | 2 | 600 | 2400 |
| demon | 23 | 7 | 600 | 20000 |
| giant spider | 86 | 14 | 600 | 19200 |


Hydra has 39 slots at 600 seconds and one at 2400. Dwarf Guard has two 8000-second
slots despite 329 at 600. Demon includes four 20000-second slots; Giant Spider
includes one at 19200. These are evidence of special timing, **not proof of the
encounter's quest/boss identity**. Keep them out of a generic ordinary-hunt policy.
The full species table appears below, so both uniform and variable species can
be reviewed without opening the raw matrix.

## Spatial grouping and dwarf coordinates

The OTBM has 11 named town/temple anchors and
0 named waypoints. Tile areas are storage chunks,
and house metadata describes properties; neither supplies authoritative hunting
region polygons. XML groups are useful exact source cohorts, but the runtime
splits them into independent monster spawns. No mine/dungeon names are invented.

For the three dwarf species together, the audit computes deterministic
single-link connected components under **Chebyshev XY distance ≤64 tiles**,
projecting all floors together. This groups nearby coordinates, not walkable
routes. Floor lists/distributions remain explicit. Chains can merge distinct
rooms/caves; a 64-tile edge is not evidence of stairs, doors or access. IDs are
stable under unchanged inputs and numbered by size then source/spatial order.
Nearest town labels below mean only nearest **projected town-temple point to the
cluster centroid**; they are not hunting-area names or ownership claims.

| Cluster | Slots | Dwarf / Soldier / Guard | X range | Y range | Floors | Intervals | Nearest mapped temple (XY only) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| D01 | 888 | 235 / 335 / 318 | 32365–32730 | 31825–32040 | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 | 500s: 3; 600s: 875; 700s: 2; 750s: 6; 8000s: 2 | Kazordoon (123.7 tiles) |
| D02 | 39 | 0 / 30 / 9 | 33210–33314 | 31648–31712 | 8, 9, 11, 13, 15 | 500s: 9; 550s: 3; 600s: 27 | Edron (131.1 tiles) |
| D03 | 9 | 4 / 1 / 4 | 32928–32940 | 32646–32654 | 7 | 600s: 9 | Ankrahmun (331.3 tiles) |
| D04 | 7 | 4 / 3 / 0 | 32399–32407 | 31582–31590 | 12 | 700s: 4; 800s: 3 | Carlin (200.8 tiles) |
| D05 | 4 | 0 / 0 / 4 | 32403–32408 | 31721–31723 | 11 | 900s: 4 | Carlin (75.8 tiles) |
| D06 | 2 | 2 / 0 / 0 | 32597–32599 | 32068–32068 | 9 | 600s: 2 | Kazordoon (151.8 tiles) |
| D07 | 2 | 0 / 0 / 2 | 32505–32506 | 31722–31722 | 15 | 700s: 2 | Carlin (157.4 tiles) |
| D08 | 1 | 1 / 0 / 0 | 32588–32588 | 31639–31639 | 8 | 600s: 1 | Ab'Dendriel (144.1 tiles) |
| D09 | 1 | 0 / 0 / 1 | 33332–33332 | 31829–31829 | 9 | 900s: 1 | Edron (116 tiles) |
| D10 | 1 | 1 / 0 / 0 | 33157–33157 | 32603–32603 | 11 | 600s: 1 | Darashia (159.2 tiles) |


Sensitivity check (largest-first component sizes):

| XY threshold | Components | Sizes |
| --- | --- | --- |
| 32 | 12 | 882, 39, 9, 7, 4, 4, 2, 2, 2, 1, 1, 1 |
| 64 | 10 | 888, 39, 9, 7, 4, 2, 2, 1, 1, 1 |
| 96 | 8 | 890, 39, 9, 7, 4, 3, 1, 1 |


A floor-separated 32-tile calculation produces 42
components. The large projected D01 should therefore be treated as a broad
coordinate complex, not a single certified hunting room. Full source membership
for every cluster is in JSON; raw CSV rows carry their dwarf cluster ID.

### Mapped town anchors

| ID | Actual map name | Temple X | Temple Y | Temple Z |
| --- | --- | --- | --- | --- |
| 1 | Thais | 32369 | 32241 | 7 |
| 2 | Carlin | 32360 | 31782 | 7 |
| 3 | Kazordoon | 32649 | 31925 | 11 |
| 4 | Ab'Dendriel | 32732 | 31634 | 7 |
| 5 | Edron | 33217 | 31814 | 8 |
| 6 | Darashia | 33213 | 32454 | 1 |
| 7 | Venore | 32957 | 32076 | 7 |
| 8 | Ankrahmun | 33195 | 32853 | 8 |
| 9 | Port Hope | 32595 | 32744 | 7 |
| 10 | Home | 32316 | 31942 | 7 |
| 11 | Rookgaard | 32097 | 32219 | 7 |


The named Cormaya dwarf raid exists separately in
`data/raids/cormayadwarfattack.xml`; its event areas are not imported as ordinary
world slots or used to rename coordinate clusters. The normal world definitions,
raid schedule and script-created encounters remain distinct datasets.

## Classic runtime architecture

The following distinctions are essential:

| Quantity | Source / actual role |
| --- | --- |
| XML `spawntime` | Original seconds in this matrix |
| `sb.interval` | Effective per-slot eligibility interval in milliseconds; XML ×1000 or the nonzero global replacement |
| `Spawn::interval` | Member initialized to **60000**, then `min(member, added slot interval)`; polling basis, not eligibility |
| `getInterval()` | Returns the member unless it exceeds 500000 ms; legacy population/random branch is conditional on that member |
| Scheduler delay | Classic initial `getInterval()` and subsequent `nextSpawnCheck()`; event execution can be late under load |
| Actual eligibility | Missing slot and `now >= sb.lastSpawn + sb.interval`, then player check and placement |

All current XML intervals are at least 60 seconds. With the explicit zero override,
**every normally loaded current runtime Spawn has `interval=60000` ms**. Because
that member is initialized at 60000 and only reduced, the `>500000` population
branch cannot be reached by this loader at any player count. Existing engine
tests deliberately assign 600000 through a test friend to exercise that branch;
that synthetic coverage is not evidence that production world loading reaches it.

### Timestamp and scheduling sequence

1. `addMonster` sets `sb.lastSpawn=0`. `startup` attempts immediate placement,
   without the normal nearby-player check. A successful `spawnMonster` sets
   `lastSpawn=now`; startup failure is separate from slot-definition validity.
2. On monster removal, `Monster::onRemoveCreature` calls `startSpawnCheck` before
   `Game::removeCreature` marks the creature removed. Classic schedules the first
   check using `getInterval()` (60 seconds for this world).
3. At that first check, `cleanup()` sees the removed monster, resets
   **`lastSpawn=now`**, releases it from `spawnedMap`, and only then can the missing
   slot begin its normal eligibility wait. Therefore the timer is normally based
   on removal **detection/cleanup**, not precisely the death timestamp.
4. Missing Classic slots are checked against `lastSpawn + sb.interval`. A player
   near a due slot resets `lastSpawn` to that poll's time and postpones eligibility
   for the **entire slot interval**, not just one poll. A player leaving does not
   cause immediate refill; the next eligible poll must still occur.
5. `findPlayer` uses same-floor player spectators within the default
   **±11 X / ±11 Y** viewport, with no wall/line-of-sight test. A player with
   `PlayerFlag_IgnoredByMonsters` does not block. The XML radius and normal client
   ±8/±6 view are not this nearby-player rectangle.
6. While any slot is missing, checks reschedule with `nextSpawnCheck()`. Once all
   are occupied, polling stops until another disappearance starts it. Failed
   placement leaves the slot missing and retries on polling; it does not credit
   a successful spawn timestamp.

Under ideal idle scheduler/no blocker/normal removal, current-slot delay from
removal to refill is approximately `60 + ceil(baseSeconds/60)*60` seconds. Thus
600 seconds often behaves near **660 seconds**, 60 near **120**, and 70 near
**180**. This is source-derived timing, not a live-server latency measurement.
The scheduler's minimum task delay is 50 ms; scheduling/dispatcher load and
placement can add delay. `Spawn::removeMonster` on convincing/conversion is a
separate path: Classic erases tracking without the Easy deadline/start handling,
so do not equate its behavior with the normal removed-creature sequence.

### What the legacy population code would do if reachable

For a polling member `I > 500000`, `getInterval` derives a pre-random bound:

- online players ≤200: `B=I`;
- 201–800: `B=floor(200*I/(floor(players/2)+100))`;
- >800: `B=floor(2*I/5)`.

It returns `normal_random(B/2,B)`. This is a bounded normal-shaped sample, **not
uniform randomness**: `tools.cpp` uses mean 0.5 / deviation 0.25 with out-of-range
samples mapped toward the midpoint. Even if reached, this changes polling,
**not `sb.interval` or the per-slot due inequality**. In this world its member
threshold is never reached, so no Classic population compensation is active
through this path. No timer is changed by the audit.

## Existing Easy layer (currently inert)

The production file **`data/zedas/spawns.xml` contains 0 entries**.
The example master and respawn flags are both false. Turning flags on with this
empty manifest would still change no slot. Missing/invalid policy fails closed;
no inferred species/area membership is applied.

- `RespawnManifest` keys an entry by case-insensitive exact monster name plus
  absolute X/Y/Z and requires the **effective base interval** to match
  `baseSeconds`. A global nonzero override changes that match value. There are
  no wildcards, inherited XML-group rules or automatic region classifiers.
- `classification="ordinary"` is an explicit review assertion, not a discovered
  boss/quest flag. `Spawn::addMonster` additionally requires the actual monster
  type to be both hostile and attackable. Such flags do not themselves prove an
  encounter is ordinary. Raid scheduling does not consult the manifest.
- `intervalPercent` is required integer 1–100. `minSeconds` is required integer
  2 through `baseSeconds`. Optional `populationFloor` is 0–10000, default 0.
  Invalid/duplicate keys or unknown policy attributes reject the entire manifest.
- `RespawnDeadline::begin` starts from the effective slot interval. Only when
  `populationFloor != 0` **and the slot interval exceeds 500000 ms** does it apply
  the population curve using `max(online, populationFloor)` and sample
  `normal_random(B/2,B)`. Otherwise no population scaling/randomness is used.
- It then applies integer percentage and clamps:
  `delay = min(base, max(floor(sample*intervalPercent/100), minSeconds*1000))`.
  This is an actual **eligibility deadline**, unlike Classic's population-only
  polling adjustment. PopulationFloor is not a guaranteed number of players,
  multiplier or world population simulation.
- A selected slot starts a pending deadline at removal cleanup or the explicit
  conversion-removal path; success clears it. Initial selected polling is capped
  at 1000 ms, so cleanup normally detects removal much sooner than Classic.
  `nextSpawnCheck` wakes by the remaining deadline; expired failed placements
  retry at most once per second, with the scheduler minimum still applying.
- A nearby blocking player at the due time sets `lastSpawn=now` and moves the
  deadline to `now + the same sampled delay`. It does **not** resample or bypass
  nearby-player protection. A normal attempt after that player leaves still
  waits for the postponed deadline.

A 600-second selected slot with 50%, min 120 and populationFloor 0 would have a
300-second deterministic deadline after roughly one-second cleanup, rather than
Classic's roughly 660-second normal-removal cycle. This comparison includes the
cleanup/polling distinction; it is not merely 600 versus 300.

## Policy design options — proposals only

| Strategy | Possible settings / feel | Replenishment and rarity risk | Maintenance |
| --- | --- | --- | --- |
| Reviewed-slot flat percentage | 50%, min 60, populationFloor 0; predictable solo refill | Does not add slots, but raises replacement pressure. Preserves interval ratios above the floor; shortest slots clamp. Exclude special/outlier encounters. | Low policy complexity, still requires explicit slot review |
| Historical interval tiers | Example ≤300s: 75%; 301–900s: 50%; >900s unchanged until separate review | Avoids sweeping rare long timers into the hunt policy. Tier boundaries alter historical ratios and can produce abrupt local differences. | Moderate; deterministic tiers can propose entries but cannot certify encounters |
| Species plus coordinate cohorts | Start with one reviewed XML source group or bounded floor-specific area; keep local original intervals | Best containment for testing real route feel. Similar species elsewhere remain historical. Geometry alone cannot establish safety. | Highest review burden; exact identity/base matches protect against drift |
| Population compensation | For reviewed >500s slots, populationFloor 400 and 100%, min 60; sample the existing curve once | 600s pre-random bound becomes 400s, then a 200–400s bounded normal-shaped eligibility sample. Maintains long-base proportionality until floors/clamps; adds variability and is stronger than simply activating Classic polling. | Moderate; population assumptions and random outcomes complicate acceptance |

Faster refill does not create more configured simultaneous slots. It can keep
more of those slots occupied during hunting and increase replacement/encounter
frequency. Room density, escape routes and adjacent encounters still need live
observation. No policy should automatically promote bosses, raids, rare/quest
encounters or every hostile type based only on this dataset.

## Dwarf-area pilot proposal — not an activation

The three exact species have **954 total slots**. A conservative
source-level screen keeps only hostile/attackable, unique-key **600-second** slots:
**915 candidates**. Their encounter ordinariness is still
unproven: the XML has no quest/boss tag, so this is a review pool, not permission
to classify all of them. 39 non-600-second dwarf slots
are held out for location-specific review; no unusual local interval is erased.

Start smaller: **XML group 8455**, center **(32597,31927,14)**, radius **6**, has
**10 candidates: 5 Dwarfs, 3 Dwarf Soldiers, 2 Dwarf Guards**, all originally
600 seconds, in projected cluster D01. It is a compact exact source cohort, not a
named or guaranteed ordinary room. The nearest mapped temple is Kazordoon; that
is coordinate context, not proof of a mine name, route or encounter safety.
Inspect these positions in the actual world before any later activation.

Suggested first reviewed settings: **intervalPercent 50, minSeconds 120,
populationFloor 0** → **300-second eligibility**, same nearby-player protection.
The minimum is valid for the 600-second base and does not bind this result.
Observe field density and clearing time before considering 33% (~198 seconds).
Keep neighboring rooms and all non-reviewed source slots unchanged.

| Source slot | Monster | Absolute X | Absolute Y | Z | Historical seconds | Proposed seconds |
| --- | --- | --- | --- | --- | --- | --- |
| G08455/C01 | dwarf | 32597 | 31925 | 14 | 600 | 300 |
| G08455/C02 | dwarf | 32598 | 31926 | 14 | 600 | 300 |
| G08455/C03 | dwarf soldier | 32596 | 31927 | 14 | 600 | 300 |
| G08455/C04 | dwarf | 32597 | 31927 | 14 | 600 | 300 |
| G08455/C05 | dwarf guard | 32601 | 31928 | 14 | 600 | 300 |
| G08455/C06 | dwarf guard | 32603 | 31929 | 14 | 600 | 300 |
| G08455/C07 | dwarf | 32598 | 31930 | 14 | 600 | 300 |
| G08455/C08 | dwarf soldier | 32600 | 31930 | 14 | 600 | 300 |
| G08455/C09 | dwarf soldier | 32598 | 31931 | 14 | 600 | 300 |
| G08455/C10 | dwarf | 32601 | 31932 | 14 | 600 | 300 |


### Held-out dwarf slots

All 39 nonstandard-timer slots are held out initially, including historically
faster 500/550-second variants, 700/750/800/900 variants and the two 8000-second
Dwarf Guards. The last pair is at **(32452,32006,13)** and **(32451,32007,13)**,
source group **7697**; their unusually long timing is sufficient to exclude them
without inventing a quest label. Unknown special-room status, a changed base,
new duplicate source key, non-hostile/non-attackable data or any unresolved source
validation error also blocks a future candidate. Scripted/raid Dwarf appearances
are not world slots in this dataset. Dwarf Geomancer and named special dwarves
are outside the three-species scope, not silently added to the pilot.

| Source slot | Monster | X/Y/Z | Original seconds |
| --- | --- | --- | --- |
| G01055/C01 | dwarf soldier | 32482, 31989, 7 | 750 |
| G01055/C02 | dwarf soldier | 32488, 31989, 7 | 750 |
| G01055/C03 | dwarf soldier | 32478, 31993, 7 | 750 |
| G01055/C04 | dwarf soldier | 32483, 31994, 7 | 750 |
| G01055/C06 | dwarf soldier | 32484, 31998, 7 | 750 |
| G01055/C07 | dwarf soldier | 32479, 31999, 7 | 750 |
| G04083/C01 | dwarf guard | 33332, 31829, 9 | 900 |
| G05463/C01 | dwarf | 32622, 31925, 10 | 500 |
| G05463/C02 | dwarf | 32623, 31925, 10 | 500 |
| G05463/C03 | dwarf | 32623, 31927, 10 | 500 |
| G05485/C01 | dwarf | 32658, 31947, 10 | 700 |
| G05485/C02 | dwarf | 32660, 31947, 10 | 700 |
| G06293/C02 | dwarf soldier | 33312, 31666, 11 | 550 |
| G06299/C01 | dwarf soldier | 33314, 31669, 11 | 550 |
| G06299/C02 | dwarf soldier | 33312, 31672, 11 | 550 |
| G06309/C01 | dwarf guard | 32403, 31721, 11 | 900 |
| G06309/C02 | dwarf guard | 32406, 31721, 11 | 900 |
| G06309/C03 | dwarf guard | 32407, 31722, 11 | 900 |
| G06309/C04 | dwarf guard | 32408, 31723, 11 | 900 |
| G06972/C01 | dwarf | 32407, 31582, 12 | 700 |
| G06971/C01 | dwarf soldier | 32399, 31585, 12 | 800 |
| G06971/C02 | dwarf soldier | 32401, 31585, 12 | 800 |
| G06971/C03 | dwarf | 32401, 31586, 12 | 700 |
| G06971/C04 | dwarf soldier | 32402, 31586, 12 | 800 |
| G06971/C05 | dwarf | 32403, 31586, 12 | 700 |
| G06972/C02 | dwarf | 32402, 31590, 12 | 700 |
| G07579/C02 | dwarf soldier | 33300, 31691, 13 | 500 |
| G07579/C04 | dwarf soldier | 33303, 31693, 13 | 500 |
| G07580/C01 | dwarf soldier | 33306, 31695, 13 | 500 |
| G07697/C01 | dwarf guard | 32452, 32006, 13 | 8000 |
| G07697/C02 | dwarf guard | 32451, 32007, 13 | 8000 |
| G09195/C01 | dwarf soldier | 33294, 31684, 15 | 500 |
| G09195/C02 | dwarf soldier | 33292, 31686, 15 | 500 |
| G09195/C03 | dwarf soldier | 33295, 31687, 15 | 500 |
| G09195/C04 | dwarf soldier | 33288, 31690, 15 | 500 |
| G09205/C01 | dwarf soldier | 33302, 31706, 15 | 500 |
| G09205/C02 | dwarf soldier | 33301, 31708, 15 | 500 |
| G09212/C05 | dwarf guard | 32505, 31722, 15 | 700 |
| G09212/C06 | dwarf guard | 32506, 31722, 15 | 700 |


## Input diagnostics and validation

The generator surfaces **161 diagnostics**. The repository
contains legacy XML declarations such as `encoding="UTF - 8"`, accepted by its
native loader but rejected by strict Expat. The audit decodes the recognized
UTF-8 alias and replaces only the declaration with whitespace, preserving raw
source hashes and line numbers. It never rewrites input files. These declaration
normalizations are listed in JSON and below; they are not fabricated monster
flags. Missing hostile/attackable flags use the documented native default and
record that provenance. Missing optional directions are explicit defaults.
Malformed body XML, required numeric/name fields, unresolved monster definitions,
unknown relevant attributes and invalid/out-of-range coordinates fail the audit
without overwriting outputs.

| Kind | Input | Detail |
| --- | --- | --- |
| legacy_xml_encoding_declaration | data/monster/monsters.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/raids/orc.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/raids/orcwarlord.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/amazon.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/ancientscarab.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/apocalypse.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/assassin.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/badger.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/bandit.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/banshee.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/bat.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/bazir.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/bear.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/behemoth.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/beholder.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/blackknight.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/blacksheep.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/bluedjinn.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/bonebeast.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/bug.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/butterflyblue.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/butterflypurple.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/butterflyred.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/butterflyyellow.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/carniphila.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/caverat.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/centipede.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/chicken.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/cobra.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/crab.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/crocodile.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/cryptshambler.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/cyclops.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/darkmonk.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/deathslicer.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/deer.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/demodras.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/demon.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/demonskeleton.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dharalion.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dog.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dragon.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dragonlord.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dwarf.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dwarfgeomancer.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dwarfguard.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dwarfsoldier.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dworcfleshhunter.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dworcvenomsniper.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/dworcvoodoomaster.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/efreet.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/elderbeholder.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/elephant.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/elf.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/elfarcanist.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/elfscout.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/evileye.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/fernfang.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/ferumbras.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/firedevil.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/fireelemental.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/flamethrower.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/flamingo.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/frosttroll.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/gamemaster.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/gargoyle.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/gazer.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/ghost.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/ghoul.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/giantspider.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/goblin.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/greendjinn.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/grorlam.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/halloweenhare.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/hero.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/hornedfox.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/hunter.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/hyaena.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/hydra.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/illusion.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/infernatil.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/kongra.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/larva.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/lich.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/lion.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/lizardsentinel.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/lizardsnakecharmer.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/lizardtemplar.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/magicthrower.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/marid.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/merlkin.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/mimic.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/minotaur.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/minotaurarcher.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/minotaurguard.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/minotaurmage.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/monk.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/morgaroth.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/mummy.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/murius.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/necromancer.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/necropharus.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/oldwidow.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orc.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orcberserker.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orcleader.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orcrider.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orcshaman.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orcspearman.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orcwarlord.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orcwarrior.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/orshabaal.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/panda.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/parrot.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pharaohashmunrah.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pharaohdipthrah.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pharaohmahrdis.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pharaohmorguthis.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pharaohomruc.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pharaohrahemos.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pharaohthalas.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pharaohvashresamun.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/pig.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/plaguethrower.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/poisonspider.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/polarbear.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/priestess.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/rabbit.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/rat.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/rotworm.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/scarab.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/scorpion.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/serpentspawn.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/sheep.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/shredderthrower.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/sibang.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/skeleton.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/skunk.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/slime.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/slime2.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/smuggler.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/snake.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/spider.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/spitnettle.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/stalker.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/stonegolem.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/swamptroll.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/tarantula.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/terrorbird.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/tiger.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/troll.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/valkyrie.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/vampire.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/warlock.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/warwolf.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/wasp.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/wildwarrior.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/winterwolf.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/witch.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/wolf.xml | UTF - 8 |
| legacy_xml_encoding_declaration | data/monster/yeti.xml | UTF - 8 |


Regenerate and verify without a database/client:

```sh
python3 tests/audit/spawn_matrix.py
python3 tests/audit/spawn_matrix.py --check
python3 -m unittest discover -s tests/audit -p 'test_spawn_matrix.py'
```

Validation includes every source-child identity exactly once, center-plus-offset
and native uint16 calculations, preserved duplicate records, strict malformed/
missing input rejection, default-flag/direction provenance, escaped/truncated OTBM
metadata fixtures, deterministic clustering and CSV/JSON roundtrips. `--check`
reconstructs all three artifacts and rejects stale bytes. Changed audited runtime
or audited map/config/manifest hashes require a fresh source/data/pilot review
before deliberately updating the contract snapshot.
The existing native respawn tests validate Easy deadline/population helper
behavior; their synthetic Classic long-poll setup is explicitly distinguished
above. No full-world game process or original client was launched for the audit.
Actual placement, blocked tiles, room/quest status and gameplay density remain
later live-review questions, not inferred facts.

## Complete species consistency matrix

| Source species | Slots | Classification | Interval distribution (seconds: count) |
| --- | --- | --- | --- |
| amazon | 70 | uniform | 600s: 70 |
| ancient scarab | 82 | uniform | 600s: 82 |
| ashmunrah | 1 | uniform | 600s: 1 |
| assassin | 72 | uniform | 600s: 72 |
| bandit | 55 | uniform | 600s: 55 |
| banshee | 60 | location-dependent | 300s: 2; 500s: 2; 600s: 54; 900s: 1; 3600s: 1 |
| bat | 68 | uniform | 600s: 68 |
| bear | 99 | location-dependent | 500s: 6; 550s: 1; 600s: 68; 700s: 13; 800s: 5; 900s: 2; 950s: 2; 1200s: 2 |
| behemoth | 57 | location-dependent | 500s: 1; 600s: 19; 700s: 14; 800s: 1; 900s: 21; 6000s: 1 |
| beholder | 192 | location-dependent | 500s: 6; 600s: 128; 700s: 5; 800s: 23; 850s: 2; 888s: 1; 900s: 8; 950s: 2; 1000s: 8; 1001s: 1; 1200s: 3; 1400s: 2; 1500s: 1; 2000s: 1; 6000s: 1 |
| black knight | 10 | location-dependent | 600s: 9; 2000s: 1 |
| black sheep | 8 | uniform | 600s: 8 |
| blue butterfly | 42 | uniform | 600s: 42 |
| blue djinn | 50 | uniform | 600s: 50 |
| bonebeast | 87 | location-dependent | 600s: 83; 700s: 1; 900s: 2; 999s: 1 |
| bug | 432 | location-dependent | 190s: 1; 300s: 4; 400s: 20; 444s: 3; 500s: 4; 550s: 11; 600s: 345; 650s: 8; 700s: 17; 777s: 10; 800s: 2; 831s: 2; 950s: 5 |
| carniphila | 60 | uniform | 600s: 60 |
| cave rat | 291 | location-dependent | 300s: 5; 400s: 2; 600s: 284 |
| centipede | 310 | uniform | 600s: 310 |
| chicken | 17 | location-dependent | 600s: 2; 1600s: 2; 3600s: 10; 7200s: 3 |
| cobra | 169 | location-dependent | 600s: 167; 900s: 1; 1600s: 1 |
| crab | 18 | location-dependent | 600s: 17; 3600s: 1 |
| crocodile | 77 | uniform | 600s: 77 |
| crypt shambler | 272 | location-dependent | 600s: 271; 700s: 1 |
| cyclops | 263 | location-dependent | 300s: 2; 400s: 13; 500s: 16; 550s: 12; 600s: 143; 650s: 4; 700s: 19; 750s: 20; 800s: 21; 850s: 6; 900s: 7 |
| dark monk | 72 | uniform | 600s: 72 |
| deathslicer | 5 | uniform | 60s: 5 |
| deer | 327 | location-dependent | 400s: 18; 450s: 4; 500s: 25; 600s: 229; 650s: 5; 700s: 22; 800s: 13; 900s: 10; 2000s: 1 |
| demon | 23 | location-dependent | 600s: 10; 700s: 2; 800s: 2; 999s: 1; 1000s: 1; 6000s: 3; 20000s: 4 |
| demon skeleton | 780 | location-dependent | 300s: 1; 400s: 11; 500s: 2; 600s: 714; 700s: 15; 750s: 3; 800s: 11; 850s: 3; 900s: 7; 950s: 7; 999s: 3; 1000s: 2; 3600s: 1 |
| dipthrah | 1 | uniform | 600s: 1 |
| dog | 8 | location-dependent | 400s: 1; 600s: 6; 800s: 1 |
| dragon | 179 | location-dependent | 500s: 1; 550s: 2; 600s: 81; 700s: 8; 800s: 6; 850s: 1; 900s: 32; 950s: 10; 999s: 3; 1000s: 10; 1100s: 6; 1200s: 3; 1500s: 2; 1600s: 5; 1800s: 1; 2000s: 1; 2600s: 2; 3000s: 1; 3600s: 3; 7500s: 1 |
| dragon lord | 98 | location-dependent | 550s: 1; 600s: 69; 900s: 2; 950s: 1; 999s: 1; 1000s: 4; 1100s: 3; 1200s: 2; 1600s: 1; 1700s: 1; 1800s: 3; 2200s: 3; 2600s: 1; 2800s: 2; 3000s: 1; 3600s: 1; 7200s: 2 |
| dwarf | 247 | location-dependent | 500s: 3; 600s: 238; 700s: 6 |
| dwarf geomancer | 14 | location-dependent | 600s: 12; 800s: 1; 8000s: 1 |
| dwarf guard | 338 | location-dependent | 600s: 329; 700s: 2; 900s: 5; 8000s: 2 |
| dwarf soldier | 369 | location-dependent | 500s: 9; 550s: 3; 600s: 348; 750s: 6; 800s: 3 |
| dworc fleshhunter | 262 | uniform | 600s: 262 |
| dworc venomsniper | 246 | uniform | 600s: 246 |
| dworc voodoomaster | 85 | uniform | 600s: 85 |
| efreet | 21 | uniform | 600s: 21 |
| elder beholder | 10 | uniform | 600s: 10 |
| elephant | 38 | uniform | 600s: 38 |
| elf | 102 | location-dependent | 400s: 3; 600s: 98; 9999s: 1 |
| elf arcanist | 31 | location-dependent | 600s: 17; 700s: 5; 800s: 6; 900s: 3 |
| elf scout | 88 | location-dependent | 600s: 82; 700s: 2; 750s: 2; 800s: 2 |
| fire devil | 88 | location-dependent | 600s: 74; 750s: 3; 800s: 4; 850s: 3; 900s: 4 |
| fire elemental | 168 | location-dependent | 300s: 6; 500s: 3; 600s: 129; 700s: 17; 800s: 7; 900s: 5; 1800s: 1 |
| flamethrower | 15 | uniform | 60s: 15 |
| flamingo | 95 | uniform | 600s: 95 |
| frost troll | 61 | location-dependent | 600s: 27; 650s: 21; 700s: 4; 750s: 3; 800s: 5; 900s: 1 |
| gargoyle | 158 | uniform | 600s: 158 |
| ghost | 474 | location-dependent | 500s: 4; 600s: 453; 900s: 9; 999s: 1; 1200s: 6; 1300s: 1 |
| ghoul | 901 | location-dependent | 380s: 3; 400s: 16; 450s: 2; 460s: 4; 500s: 12; 550s: 5; 590s: 2; 600s: 806; 650s: 4; 700s: 17; 750s: 5; 800s: 12; 900s: 13 |
| giant spider | 86 | location-dependent | 600s: 60; 700s: 1; 800s: 3; 850s: 1; 900s: 5; 999s: 1; 1000s: 1; 1200s: 2; 1400s: 3; 1500s: 5; 1600s: 1; 2600s: 1; 6000s: 1; 19200s: 1 |
| goblin | 253 | location-dependent | 300s: 1; 600s: 230; 650s: 8; 800s: 4; 900s: 1; 1200s: 9 |
| green djinn | 89 | location-dependent | 600s: 82; 1200s: 5; 1300s: 1; 9000s: 1 |
| hero | 9 | location-dependent | 600s: 2; 700s: 2; 750s: 3; 900s: 2 |
| hunter | 37 | location-dependent | 600s: 27; 800s: 3; 900s: 5; 1000s: 2 |
| hyaena | 102 | uniform | 600s: 102 |
| hydra | 40 | location-dependent | 600s: 39; 2400s: 1 |
| illusion | 9 | uniform | 600s: 9 |
| kongra | 223 | uniform | 600s: 223 |
| larva | 1439 | uniform | 600s: 1439 |
| lich | 25 | location-dependent | 600s: 24; 700s: 1 |
| lion | 69 | location-dependent | 600s: 49; 650s: 8; 700s: 8; 750s: 3; 800s: 1 |
| lizard sentinel | 99 | uniform | 600s: 99 |
| lizard snakecharmer | 23 | location-dependent | 600s: 18; 650s: 4; 900s: 1 |
| lizard templar | 78 | uniform | 600s: 78 |
| magicthrower | 54 | uniform | 60s: 54 |
| mahrdis | 1 | uniform | 600s: 1 |
| marid | 16 | uniform | 600s: 16 |
| merlkin | 92 | location-dependent | 600s: 80; 750s: 5; 800s: 2; 890s: 2; 900s: 3 |
| minotaur | 343 | location-dependent | 300s: 5; 400s: 14; 450s: 5; 500s: 19; 600s: 202; 650s: 25; 700s: 57; 750s: 9; 800s: 2; 850s: 4; 900s: 1 |
| minotaur archer | 118 | location-dependent | 500s: 3; 600s: 73; 650s: 3; 700s: 10; 750s: 9; 800s: 11; 900s: 6; 1300s: 3 |
| minotaur guard | 107 | location-dependent | 500s: 5; 600s: 58; 700s: 12; 800s: 14; 850s: 2; 900s: 16 |
| minotaur mage | 56 | location-dependent | 600s: 31; 700s: 1; 800s: 4; 850s: 5; 900s: 7; 1000s: 6; 1500s: 2 |
| monk | 81 | location-dependent | 450s: 5; 500s: 4; 600s: 42; 700s: 10; 750s: 6; 800s: 9; 900s: 2; 9999s: 3 |
| morguthis | 1 | uniform | 600s: 1 |
| mummy | 576 | location-dependent | 600s: 485; 700s: 2; 800s: 9; 900s: 40; 1100s: 1; 1200s: 20; 1500s: 3; 1600s: 3; 1800s: 1; 1900s: 1; 2000s: 1; 2400s: 4; 2600s: 3; 2700s: 1; 6000s: 1; 9999s: 1 |
| necromancer | 103 | location-dependent | 400s: 4; 600s: 99 |
| omruc | 1 | uniform | 600s: 1 |
| orc | 205 | location-dependent | 300s: 3; 350s: 6; 400s: 16; 500s: 2; 600s: 143; 650s: 4; 700s: 8; 800s: 14; 900s: 2; 1000s: 5; 1200s: 2 |
| orc berserker | 173 | location-dependent | 600s: 139; 700s: 3; 720s: 2; 770s: 4; 800s: 9; 900s: 2; 1000s: 10; 1200s: 4 |
| orc leader | 53 | location-dependent | 600s: 45; 800s: 2; 900s: 2; 1000s: 2; 1600s: 2 |
| orc rider | 24 | uniform | 600s: 24 |
| orc shaman | 97 | location-dependent | 600s: 83; 700s: 1; 750s: 3; 800s: 1; 880s: 2; 900s: 7 |
| orc spearman | 257 | location-dependent | 300s: 2; 400s: 2; 500s: 10; 550s: 5; 600s: 205; 650s: 10; 700s: 7; 750s: 5; 760s: 1; 800s: 10 |
| orc warlord | 15 | uniform | 600s: 15 |
| orc warrior | 232 | location-dependent | 300s: 1; 450s: 2; 500s: 8; 600s: 194; 650s: 1; 680s: 2; 700s: 17; 800s: 5; 1000s: 2 |
| panda | 25 | uniform | 600s: 25 |
| parrot | 23 | uniform | 600s: 23 |
| pig | 58 | location-dependent | 600s: 12; 900s: 30; 990s: 8; 1000s: 5; 1200s: 1; 9000s: 2 |
| plaguethrower | 16 | uniform | 60s: 16 |
| poison spider | 768 | location-dependent | 300s: 14; 400s: 18; 500s: 10; 600s: 687; 650s: 1; 680s: 6; 700s: 16; 750s: 7; 800s: 9 |
| polar bear | 13 | location-dependent | 600s: 4; 650s: 3; 700s: 4; 800s: 1; 850s: 1 |
| priestess | 38 | location-dependent | 600s: 17; 700s: 4; 750s: 1; 800s: 13; 850s: 2; 1200s: 1 |
| purple butterfly | 170 | location-dependent | 600s: 145; 900s: 5; 1200s: 11; 1600s: 2; 1800s: 7 |
| rabbit | 233 | location-dependent | 300s: 9; 400s: 2; 500s: 3; 600s: 184; 650s: 7; 700s: 15; 750s: 6; 900s: 7 |
| rahemos | 1 | uniform | 600s: 1 |
| rat | 362 | location-dependent | 60s: 1; 70s: 12; 200s: 21; 300s: 58; 350s: 2; 400s: 29; 500s: 30; 600s: 183; 700s: 10; 800s: 2; 850s: 3; 1000s: 1; 2000s: 10 |
| red butterfly | 34 | uniform | 600s: 34 |
| rotworm | 1255 | location-dependent | 300s: 8; 350s: 4; 400s: 16; 450s: 21; 500s: 922; 543s: 2; 550s: 16; 600s: 133; 650s: 42; 700s: 49; 750s: 10; 800s: 12; 900s: 11; 1000s: 5; 1100s: 4 |
| scarab | 870 | location-dependent | 600s: 867; 800s: 1; 900s: 2 |
| scorpion | 415 | location-dependent | 300s: 10; 400s: 6; 600s: 362; 650s: 6; 700s: 1; 800s: 9; 850s: 5; 900s: 10; 1000s: 4; 1200s: 2 |
| serpent spawn | 18 | uniform | 600s: 18 |
| sheep | 33 | location-dependent | 100s: 5; 600s: 15; 800s: 6; 850s: 3; 900s: 4 |
| shredderthrower | 20 | uniform | 60s: 20 |
| sibang | 239 | uniform | 600s: 239 |
| skeleton | 792 | location-dependent | 210s: 8; 400s: 19; 450s: 5; 500s: 11; 550s: 3; 600s: 693; 650s: 3; 700s: 28; 800s: 11; 900s: 11 |
| skunk | 30 | uniform | 600s: 30 |
| slime | 350 | location-dependent | 500s: 4; 600s: 311; 700s: 7; 800s: 17; 850s: 1; 900s: 6; 1200s: 2; 1300s: 2 |
| smuggler | 33 | uniform | 600s: 33 |
| snake | 693 | location-dependent | 300s: 2; 400s: 26; 500s: 11; 600s: 595; 650s: 1; 700s: 5; 750s: 1; 800s: 6; 850s: 2; 900s: 44 |
| spider | 391 | location-dependent | 200s: 18; 300s: 6; 350s: 8; 400s: 12; 500s: 18; 600s: 305; 700s: 13; 800s: 11 |
| spit nettle | 66 | uniform | 600s: 66 |
| stalker | 441 | location-dependent | 500s: 1; 600s: 430; 700s: 6; 900s: 1; 1300s: 1; 1800s: 2 |
| stone golem | 110 | location-dependent | 600s: 93; 700s: 1; 750s: 2; 800s: 5; 850s: 4; 900s: 3; 999s: 1; 1200s: 1 |
| swamp troll | 250 | uniform | 600s: 250 |
| tarantula | 101 | location-dependent | 600s: 86; 1200s: 15 |
| terror bird | 21 | uniform | 600s: 21 |
| thalas | 1 | uniform | 600s: 1 |
| tiger | 28 | uniform | 600s: 28 |
| troll | 604 | location-dependent | 300s: 5; 400s: 13; 450s: 5; 500s: 26; 600s: 467; 650s: 32; 700s: 28; 750s: 4; 800s: 10; 900s: 8; 1000s: 2; 1100s: 1; 1200s: 3 |
| valkyrie | 33 | uniform | 600s: 33 |
| vampire | 335 | location-dependent | 600s: 328; 900s: 2; 1000s: 1; 1600s: 1; 3600s: 1; 6000s: 1; 9999s: 1 |
| vashresamun | 1 | uniform | 600s: 1 |
| war wolf | 33 | uniform | 600s: 33 |
| warlock | 19 | location-dependent | 550s: 1; 800s: 1; 900s: 6; 1000s: 10; 1200s: 1 |
| wasp | 321 | location-dependent | 300s: 3; 400s: 18; 500s: 5; 550s: 3; 600s: 256; 650s: 7; 700s: 27; 900s: 2 |
| wild warrior | 104 | location-dependent | 60s: 1; 500s: 2; 600s: 78; 650s: 6; 700s: 9; 750s: 5; 800s: 3 |
| winter wolf | 28 | location-dependent | 600s: 9; 650s: 11; 700s: 3; 750s: 3; 800s: 2 |
| witch | 62 | location-dependent | 300s: 1; 600s: 53; 700s: 3; 800s: 3; 999s: 1; 3600s: 1 |
| wolf | 521 | location-dependent | 100s: 8; 350s: 6; 400s: 23; 450s: 6; 500s: 64; 550s: 4; 580s: 2; 600s: 312; 650s: 39; 700s: 33; 750s: 4; 800s: 9; 900s: 11 |


