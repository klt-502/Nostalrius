# Reviewed Easy Mode respawns

Production `data/zedas/spawns.xml` remains empty: **zero verified slots**. No area
is accelerated, including the 915 dwarf candidates and group 8455. Hostility,
attackability, species, historical timer, coordinates and clustering are necessary
filters or review aids, never proof of an ordinary encounter. Do not activate an
area without encounter-level visual/gameplay review of quest/boss/script roles.

The authoritative historical data remains the audit at `9100ddf`; see
[SPAWN_AUDIT.md](SPAWN_AUDIT.md). Its source-era mechanics differ from this wave.
`data/world/spawns.xml` and the map are unchanged. Keep `rateSpawn = 0` to use
historical intervals. This feature never changes that setting or raid scheduling.

## Configuration and runtime control

| Option | Generic example default | Meaning |
| --- | --- | --- |
| `easyModeEnabled` | `false` | Existing master switch. |
| `easyModeCommands` | `false` | Enables the command bridge. |
| `easyModeRespawn` | `false` | Initial session on/off state. |
| `easyModeRespawnPercent` | `50` | Strict integer 1–100; global percentage for reviewed slots. |
| `easyModeRespawnPolicyFile` | `"data/zedas/spawns.xml"` | Startup-only reviewed manifest path. |

Configuration defaults are startup-only. When the master is enabled the manifest
is validated even if initial respawn state is off, permitting a later `on`.
When the master is disabled the manifest is not opened. Missing/malformed files
fail closed to Classic; malformed files reject all entries. Startup reports mode,
global percentage, normal minimum (120 seconds), matched reviewed count, rejected
or unmatched entries/errors and validity. Stale/mismatched slots remain Classic.
No database migration or per-player state is used.

Commands are local SAY only:

- `!respawn status` (or `!respawn`) reports current session state, percentage,
  reviewed/rejected count, minimum and restart behavior; available to players.
- `!respawn on` / `!respawn off` require both access-group and GM-or-higher account.
- `!respawn 50`, `!respawn 40`, `!respawn 33` set a strict integer 1–100 and require
  the same GM access. Invalid input changes nothing. Setting a percentage does not
  implicitly enable a disabled mode.

Runtime changes are **session-only**; restart restores `config.lua`. No file is
rewritten. Percentage changes apply when the next missing-slot deadline is
sampled; existing sampled deadlines and live monsters are untouched. Mode changes
rearm missing reviewed slots once from the transition time; repeated on/off with
no state change does nothing. Unclassified slot timestamps remain untouched. Transitions first clean removed
occupants before checking missing slot IDs, so a death awaiting its queued check
cannot leave a slot unscheduled.

## Exact-slot policies and timing

Manifest entries require exact monster (case-insensitive engine lookup), absolute
X/Y/Z and effective historical base interval. A `rateSpawn` override or map timer
change invalidates matching. There are no area/species wildcards. The monster
must additionally be hostile and attackable. Example **synthetic test data**:

```xml
<easyspawns version="1">
  <spawn monster="Synthetic Ordinary" x="101" y="102" z="7"
         classification="ordinary" baseSeconds="600" minSeconds="120" />
</easyspawns>
```

`intervalPercent` is now optional: omit it to inherit the global runtime value.
An explicit 1–100 value is an exceptional per-slot override and is unaffected by
runtime percentage changes. Explicit zero is invalid. Initial future ordinary
production entries should omit it. `minSeconds` remains required (2..baseSeconds
for backward-compatible parsing), but the actual world loader enforces at least
120 seconds. Duration is capped at the historical base, so the minimum can never
lengthen a short historical interval. At 50%, base 600 becomes 300 seconds; base
500 becomes 250; base 900 becomes 450. No production slot is yet selected.

Optional `populationFloor` (0..10000, default 0) retains the existing explicitly
opted-in population curve/random sample; normal global-percentage entries should
omit it. One duration is selected per empty cycle. Cleanup after disappearance
starts eligibility, usually discovered within one second for enabled reviewed
slots; convincing uses the existing removal callback. Polls never reroll it.

At the deadline the **unchanged same-floor ±11 player exclusion** is checked
(including the existing ignored-by-monsters exception). A blocking player causes
a deterministic five-second retry, without resampling/restarting the original
interval. Failed tile placement also retries in five seconds. Dispatcher load
may delay a check; no tight scheduler loop is introduced. Success clears the
pending deadline. Classic/unclassified scheduling, timestamps and RNG call paths
retain their historical behavior when this layer is disabled.

## Review and deliberate promotion

[RESPAWN_REVIEW_CANDIDATES.csv](RESPAWN_REVIEW_CANDIDATES.csv) lists all 954 audited
Dwarf/Dwarf Soldier/Dwarf Guard slots as **UNVERIFIED_DO_NOT_ACTIVATE**. Of these,
915 have 600-second timers; 39 exceptional timers are held separately. Group 8455
has ten slots and is only the first proposed room to verify. No inferred town
name or area classification is used. All other world slots are unverified too.

Regenerate or validate without changing gameplay:

```sh
python3 tests/audit/spawn_matrix.py --historical --check
python3 tests/audit/review_respawns.py
python3 tests/audit/review_respawns.py --check
```

After actual encounter verification, select explicit IDs from the CSV (one-based
source group/child positions). The tool verifies the historical world hash,
rejects duplicate/unknown IDs and exceptional/non-hostile/non-attackable slots,
and emits a **draft to stdout only**:

```sh
python3 tests/audit/review_respawns.py \
  --slots G08455/C01 \
  --verified-evidence 'REPLACE with actual encounter review record' > /tmp/respawn-draft.xml
```

This example is syntax, **not authorization to promote group 8455**. The evidence
is a human assertion, not machine-certified ordinariness. Review the draft and
make a separate deliberate production-manifest change only after verification.
No command here writes the production manifest. XML uses exact audited positions
and base intervals, global percentage inheritance and the 120-second minimum.

## Acceptance and original-client checks

Automated tests cover strict config/manifest parsing, exact matching, global and
per-slot percentages, retained pending deadlines, minimum/base cap, five-second
retry/no resampling, native player blocking, admin authorization, runtime range
validation, empty/missing/invalid manifests and Classic scheduler RNG parity.
Synthetic manifest entries live only in tests or throwaway native fixtures.

With the production manifest, enabling mode must report zero reviewed slots and
change no spawn. Verify normal players cannot change settings, GM `status/on/off/
33/50` works, malformed values fail, and restart restores configured defaults.
In an isolated test world with an independently reviewed synthetic 600-second
slot: kill it, expect eligibility around 300 seconds at 50%; stay nearby and
observe no spawn, then clear the exclusion area and expect replacement within
about five seconds. Block/unblock the tile too. Set 33% while a deadline is
pending: retain that deadline; the following cycle uses 198 seconds. Confirm
unclassified and exceptional slots retain Classic timing. Observe the actual
7.72 client/map for proximity, visibility and obstacle behavior before promotion.
