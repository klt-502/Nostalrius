"""Human-readable counterpart to spawn_matrix.py; statistics come only from its bundle."""
import collections


def render(b):
    s=b["summary"];stats=s["species_statistics"]
    rows=[dict(zip(b["columns"],r)) for r in b["rows"]]
    mode=max(s["interval_distribution"],key=lambda v:s["interval_distribution"][v])
    def dist(d):return "; ".join(f"{key}s: {value}" for key,value in d.items())
    def table(headers,data):
        return "| "+" | ".join(headers)+" |\n| "+" | ".join("---" for _ in headers)+" |\n"+"".join("| "+" | ".join(str(v).replace('|','\\|') for v in row)+" |\n" for row in data)+"\n"
    text=f'''# Historical world-spawn matrix and respawn audit

**Data audit only. No spawn definitions, map, configuration defaults, runtime code or
Easy manifest were changed. No proposed policy below is active.**

The runtime source was inspected at `{b['audited_runtime_commit']}` on
`zedas-easy-mode`. Input SHA-256 hashes, all source groups, monster definitions,
flag provenance, diagnostics, cluster membership and proposed pilot membership
are recorded in [SPAWN_MATRIX.json](SPAWN_MATRIX.json). The runtime contract makes
regeneration fail if the inspected engine or audited map/config/manifest snapshot
changes without a new review of source semantics, data-specific prose and pilot.
There is no generated timestamp or moving HEAD hash, so unchanged inputs produce
byte-identical artifacts.

## Authoritative source and scope

The tracked example selects `mapName = "{b['example_configuration']['mapName']}"`.
`Game::loadMainMap` opens **`{b['map_metadata']['map_file']}`**. Its actual decoded
OTBM map-data attribute **11** contains **`{b['map_metadata']['spawn_reference']}`**;
`IOMap::loadMap` prepends the map's directory. The authoritative world monster
source is therefore **`{b['source_file']}`**, not the Easy manifest. The map
header describes {', '.join(repr(x) for x in b['map_metadata']['descriptions'])},
OTBM version {b['map_metadata']['version']}, dimensions
{b['map_metadata']['width']} × {b['map_metadata']['height']}.
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
  Z values agree with their group ({s['source_z_mismatches']} mismatches).
- The source casts offsets through `boost::lexical_cast<uint16_t>` and then
  narrows the final `Position` coordinates. Negative offsets wrap in that cast;
  for every current valid position the result equals signed center + offset.
  A compiled probe of the actual `pugicast.h` confirmed `-1 -> 65535` and
  `-19 -> 65517`. The extractor checks both calculations and rejects coordinate
  overflow rather than silently wrapping malformed future data.
- `spawntime` is integer **seconds**, multiplied by 1000 for `sb.interval`.
  The loader accepts only raw XML intervals strictly above 1000 ms. It is an
  eligibility interval, not a promised wall-clock death-to-reappearance time.
- All {s['direction_defaults']:,} current slots omit direction. The engine's
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
  {s['empty_groups']:,} empty XML groups create no runtime `Spawn` objects.

### Global override

The example explicitly sets **`rateSpawn = {b['example_configuration']['rateSpawn']}`**:
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
  collapsed. There are {len(s['duplicate_logical_keys'])} repeated monster/XYZ keys
  in the current input; future duplicates remain separate and are surfaced.
- `hostile` and `attackable` come from the registered monster definition selected
  through `data/monster/monsters.xml`, using only each flag element's **first**
  attribute as the engine does. Missing flags use the actual constructor defaults
  (`true`/`true`), and provenance is explicit in JSON. They are not inferred from
  monster names, damage, HP or appearance.

## Overall counts

{table(['Metric','Value'],[
 ('XML spawn groups',f"{s['groups']:,}"),('Nonempty XML groups',f"{s['nonempty_groups']:,}"),
 ('Empty XML groups',f"{s['empty_groups']:,}"),('Raw monster slots / potential runtime Spawn objects',f"{s['slots']:,}"),
 ('Unique case-insensitive monster species',s['species']),('Non-hostile slots',s['nonhostile_slots']),
 ('Non-attackable slots',s['nonattackable_slots']),('Minimum original interval',f"{s['minimum_seconds']} seconds"),
 ('Maximum original interval',f"{s['maximum_seconds']} seconds"),('Median original interval',f"{s['median_seconds']:g} seconds"),
 ('Mode original interval',f"{mode} seconds: {s['interval_distribution'][mode]:,} slots ({100*s['interval_distribution'][mode]/s['slots']:.2f}%)"),
 ('Species with one interval',s['uniform_species']),('Species with multiple intervals',s['variable_species'])])}
These are definitions, not a claim that every slot successfully places a live
monster at startup or that tiles are navigable. Placement can fail independently.

### Distribution by floor

{table(['Z','Slots'],s['floor_distribution'].items())}
### Every actual original interval

No invented bins or rounding are used. The complete distribution is:

{table(['Seconds','Slots'],s['interval_distribution'].items())}
The requested illustrative 30/120/180-second bins do not occur in this data.

## Requested hunting species

{table(['Exact source name','Slots','Original intervals (seconds: slot count)'],[
 (name,stats[name]['slots'],dist(stats[name]['intervals'])) if name in stats else (name,0,'absent')
 for name in ('dwarf','dwarf soldier','dwarf guard','dragon','dragon lord','behemoth','hydra','orc','minotaur','cyclops','demon','giant spider')])}
All twelve requested names are present. Case and spacing in CSV match the actual
spawn file; registered display names/definition files are separately available in JSON.

## Species consistency

{s['uniform_species']} species have a single historical interval;
{s['variable_species']} use different values at different locations. A species-wide
replacement would erase intentional location differences. Notable examples:

{table(['Species','Slots','Intervals','Minimum','Maximum'],[
 (name,stats[name]['slots'],len(stats[name]['intervals']),min(map(int,stats[name]['intervals'])),max(map(int,stats[name]['intervals'])))
 for name in ('dwarf','dwarf soldier','dwarf guard','dragon','dragon lord','behemoth','hydra','demon','giant spider')])}
Hydra has 39 slots at 600 seconds and one at 2400. Dwarf Guard has two 8000-second
slots despite 329 at 600. Demon includes four 20000-second slots; Giant Spider
includes one at 19200. These are evidence of special timing, **not proof of the
encounter's quest/boss identity**. Keep them out of a generic ordinary-hunt policy.
The full species table appears below, so both uniform and variable species can
be reviewed without opening the raw matrix.

## Spatial grouping and dwarf coordinates

The OTBM has {len(b['map_metadata']['towns'])} named town/temple anchors and
{len(b['map_metadata']['waypoints'])} named waypoints. Tile areas are storage chunks,
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

{table(['Cluster','Slots','Dwarf / Soldier / Guard','X range','Y range','Floors','Intervals','Nearest mapped temple (XY only)'],[
 (c['id'],c['slots'],f"{c['species'].get('dwarf',0)} / {c['species'].get('dwarf soldier',0)} / {c['species'].get('dwarf guard',0)}",
 f"{c['bounds_xy'][0]}–{c['bounds_xy'][1]}",f"{c['bounds_xy'][2]}–{c['bounds_xy'][3]}",
 ', '.join(c['floors']),dist(c['intervals']),f"{c['nearest_projected_town_temple']} ({c['nearest_temple_xy_distance']:g} tiles)")
 for c in s['dwarf_clusters']])}
Sensitivity check (largest-first component sizes):

{table(['XY threshold','Components','Sizes'],[(distance,len(sizes),', '.join(map(str,sizes))) for distance,sizes in s['dwarf_cluster_sensitivity'].items()])}
A floor-separated 32-tile calculation produces {s['dwarf_same_floor_components_32']}
components. The large projected D01 should therefore be treated as a broad
coordinate complex, not a single certified hunting room. Full source membership
for every cluster is in JSON; raw CSV rows carry their dwarf cluster ID.

### Mapped town anchors

{table(['ID','Actual map name','Temple X','Temple Y','Temple Z'],[(t['id'],t['name'],t['x'],t['y'],t['z']) for t in b['map_metadata']['towns']])}
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

The production file **`data/zedas/spawns.xml` contains {b['easy_manifest_entries']} entries**.
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

The three exact species have **{s['dwarf_total_slots']} total slots**. A conservative
source-level screen keeps only hostile/attackable, unique-key **600-second** slots:
**{s['dwarf_candidate_slots']} candidates**. Their encounter ordinariness is still
unproven: the XML has no quest/boss tag, so this is a review pool, not permission
to classify all of them. {len(s['dwarf_excluded_slots'])} non-600-second dwarf slots
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

{table(['Source slot','Monster','Absolute X','Absolute Y','Z','Historical seconds','Proposed seconds'],[
 (f"G{r['source_group_index']:05d}/C{r['source_child_index']:02d}",r['monster'],r['x'],r['y'],r['z'],r['spawntime_seconds'],300)
 for r in rows if r['source_group_index']==8455 and r['monster'].lower() in ('dwarf','dwarf soldier','dwarf guard')])}
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

{table(['Source slot','Monster','X/Y/Z','Original seconds'],[(r['source'],r['monster'],', '.join(map(str,r['position'])),r['seconds']) for r in s['dwarf_excluded_slots']])}
## Input diagnostics and validation

The generator surfaces **{len(b['diagnostics'])} diagnostics**. The repository
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

{table(['Kind','Input','Detail'],[(n['kind'],n['file'],n.get('declared',n.get('line',''))) for n in b['diagnostics']])}
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

{table(['Source species','Slots','Classification','Interval distribution (seconds: count)'],[
 (name,d['slots'],'uniform' if len(d['intervals'])==1 else 'location-dependent',dist(d['intervals'])) for name,d in stats.items()])}
'''
    return text
