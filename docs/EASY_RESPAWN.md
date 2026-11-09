# Reviewed Easy Mode respawns

This infrastructure changes only explicitly reviewed world-spawn slots. The
shipped policy, `data/zedas/spawns.xml`, is empty: enabling the feature alone
accelerates nothing. It changes no monster definitions, world-spawn data, raids,
loot rates, experience, or skills.

## Startup configuration

| Option | Default | Meaning |
| --- | --- | --- |
| `easyModeEnabled` | `false` | Existing master switch; must be enabled. |
| `easyModeRespawn` | `false` | Enables loading the reviewed-slot manifest. |
| `easyModeRespawnPolicyFile` | `"data/zedas/spawns.xml"` | XML path relative to the server's working directory. |

All three options are startup-only. Restart the server after changing the
configuration or manifest. With either switch disabled, the manifest is not
opened. Missing or invalid manifests log a warning and enable no policies.
No Action IDs are reserved by this feature. No per-player/database state exists.

## Eligibility and manifest format

Review the actual encounter before adding a slot. Bosses, rare monsters, quest
encounters and scripted special encounters must remain unclassified. There is no
reliable engine-wide boss/quest flag; species name, health, loot and spawn time
cannot safely establish that an encounter is ordinary. The loader additionally
refuses non-hostile or non-attackable monster types. Raid scheduling never reads
this policy.

The exact monster name, absolute spawn coordinates, floor and effective base
interval must all match. A nearby spawn or another spawn of the same species
does not inherit classification. `baseSeconds` means the world XML's interval,
or `rateSpawn` when the historical seconds override is nonzero. An interval
change therefore invalidates the old classification rather than silently
accelerating the changed encounter.

Example for a **reviewed test slot only**:

```xml
<easyspawns version="1">
    <spawn monster="Rat" x="101" y="102" z="7"
           classification="ordinary" baseSeconds="60"
           intervalPercent="50" minSeconds="5" />
</easyspawns>
```

| Attribute | Requirement |
| --- | --- |
| `monster` | Required nonempty exact name, matched case-insensitively as in monster lookup; no leading/trailing spaces. No wildcards. |
| `x`, `y` | Required absolute integer coordinates, 0–65535. |
| `z` | Required integer floor, 0–15. |
| `classification` | Required literal `ordinary`; an operator's explicit review assertion. |
| `baseSeconds` | Required effective base interval, 2–2147483 seconds. |
| `intervalPercent` | Required integer percentage, 1–100. Applied to the sampled duration. |
| `minSeconds` | Required minimum, 2 seconds through `baseSeconds`. The final duration never exceeds the base interval. |
| `populationFloor` | Optional integer 0–10000; defaults to 0, disabling population scaling and randomness for this policy. |

Unknown elements/attributes, duplicate attributes, duplicate monster/position
entries (including case variants), missing required fields, fractional/negative
numbers and out-of-range values reject the entire manifest. A malformed file
cannot leave partly loaded or previously loaded policies active. Unmatched or
stale entries simply authorize no matching slot. Production classifications are
deliberately absent; `tests/fixtures/easyspawns.xml` is synthetic test data.

## Eligibility deadline and population curve

Classic `Spawn::getInterval()` adjusts the **polling period** only when its
member interval exceeds 500000 ms. That member starts at 60000 ms and is reduced
by `min`, so the current ordinary load path never reaches that population
branch. This change preserves that Classic path, its polling calls and its RNG
use. It does not repair or activate it globally.

For an eligible slot, the first disappearance check occurs within one second.
The existing disappearance callback runs before `isRemoved()` becomes true;
cleanup then records the empty slot and selects its duration once. The actual
eligibility deadline is `cleanup time + selected duration`, so short intervals
are effective rather than waiting for a historical 60-second poll. Timing starts
at cleanup, with up to approximately one second of discovery delay plus normal
dispatcher load. Convincing an eligible monster also starts a replacement cycle;
Classic convincing behavior is preserved.

When `populationFloor` is positive **and** the base interval exceeds 500 seconds,
the policy uses `max(players online, populationFloor)` in the existing curve:

- At most 200 players: use the base interval.
- 201–800 players: `200 * base / (players / 2 + 100)`, using integer division.
- More than 800 players: `2 * base / 5`.

The existing `normal_random(adjusted / 2, adjusted)` sampler then runs once.
With `populationFloor=0`, or a base interval at most 500 seconds, this step makes
no RNG call. Multiply by `intervalPercent / 100`, clamp to `minSeconds` and cap
at the base interval. All arithmetic avoids intermediate 32-bit overflow.

Subsequent checks retain that same deadline and duration. A nearby ordinary
player still prevents spawning; the eligibility time moves forward by the
**same sampled duration**, without another random draw. The existing
`PlayerFlag_IgnoredByMonsters` exception is preserved. Failed placement retries
within the smaller of the Classic polling interval and one second, without
rerolling or a tight scheduler loop. A successful spawn clears the pending
deadline. Timers are session-only.

## Acceptance tests

Automated `easyspawn_policy_and_deadline` uses a synthetic manifest, controlled
clock values and an injected sampler. It checks exact slot matching; stale
interval/floor/name rejection; invalid/duplicate/partial XML rejection; the
historical population boundaries; one random sample per cycle; actual shortened
eligibility; minimum duration; nearby-player postponement without reroll; failed
placement retry bounds; empty policies; and overflow-safe deadlines.

`engine_respawn_enabled` also places/removes actual engine monsters, exercises
the real accelerated deadline and nearby-player suppression, and compares
Classic scheduler RNG advancement with the original `getInterval` call pattern.
It checks deterministic 60-second polling, missing/empty/exact policies through
the actual world XML loader, and refusal of a passive type's supplied policy.
All fixture XML/config changes stay inside the runner's temporary data directory.

For local gameplay, use an isolated copy of the map and a single reviewed
ordinary spawn. Keep the shipped production manifest empty until review:

1. With both switches disabled, kill the monster and record Classic replacement
   timing, including standing nearby. Confirm no policy file is opened/warned
   about even when its configured path is missing.
2. Enable both switches with the empty manifest. Repeat: every spawn remains
   Classic. Check a boss/rare/quest encounter and raid scheduling remain unchanged.
3. Add only the reviewed test slot with a 60-second base, 50 percent, five-second
   minimum. Restart, kill it, leave the normal player-suppression area and expect
   eligibility about 30–31 seconds after disappearance (plus server load).
4. Remain nearby past eligibility: no replacement appears. Leave the area and
   verify the same 30-second interval applies after the last suppression check.
5. Kill a second spawn of the same species at another coordinate: it remains
   Classic. Change the reviewed slot's base interval without updating its policy:
   it also remains Classic.
6. Block the spawn tile, kill the monster and wait through eligibility. Unblock
   it: spawning resumes without repeated random draws or rapid scheduler churn.
7. Try malformed/duplicate policies and restart. Verify the warning and Classic
   behavior for all slots; restore the empty production policy afterward.

Client observation is required to confirm visible disappearance/respawn,
ordinary player proximity behavior and real map obstacles with Tibia 7.72.
Automated timing/manifest tests need no database, live account, or Tibia client.
