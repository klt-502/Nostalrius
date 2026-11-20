# Easy Mode house/interior overlay

**Ownable-house update:** the initial blanket HouseTile exclusion described below
is superseded only for explicitly reviewed, durably activated fixture-only houses.
See [OWNABLE_HOUSE_FURNISHING.md](OWNABLE_HOUSE_FURNISHING.md) for the current
ownership/save/reset contract. Historical source facts and zero production approvals
remain unchanged.

Production pilot: **0 interiors**, no location promoted from the review queue.
All 861 ownable HouseTile identities are excluded. Read
[HOUSE_SYSTEM_AUDIT.md](HOUSE_SYSTEM_AUDIT.md) for persistence evidence, counts,
limits and the at-most-once award tradeoff. Original map/XML files are unchanged.

## Startup controls

| Setting | Default | Meaning |
| --- | --- | --- |
| `easyModeEnabled` | false | Existing master gate |
| `easyModeHouseFurnishing` | false | Load exact reviewed inert fixture placements |
| `easyModeHouseLoot` | false | Permit one-time loose rewards; explicit migration 003 required |

Both new flags are strict booleans, startup-only. Restart after changing config or
manifest. No new Action IDs, player storage keys, commands or client opcodes.
When master/both flags are off, no manifest/hash/database work occurs. With the
empty manifest, enabling both still changes no tile and needs no award database.
The file is fixed at `data/zedas/houses.xml`, consistent with the small fixed
reviewed loot catalog pattern; no extra path setting is needed.

## Version 1 schema

Root: `<easyhouses version="1" mapSha1="EXACT_WHOLE_OTBM_SHA1">`.
Child profiles define reusable named roles:

```xml
<profile id="poor-residence">
  <fixture role="table" item="2322" name="table" count="1" />
  <loot role="food" item="3600" name="bread" count="1" />
</profile>
```

The shipped five profiles are **not approvals**. Initial vocabulary is narrowly
reviewed and runtime validated against native flags/actions/movement handlers.
Food remains ordinary food; coin reward cap is five GP total per interior. Count
is 1 for non-coins, 1–5 for GP; no nested content, containers, beds, doors, depots,
runes, keys, marked, charged, decaying, scripted, transforming or unknown types.
Native immovable fixtures cannot be stripped or sold as movable decoration.
Functional beds and most chairs/clutter are deliberately held for later review.

A synthetic example of an **approved non-ownable interior**, never production
coordinates to copy blindly (ground ID must be the exact synthetic map's ground):

```xml
<interior id="synthetic-room" profile="poor-residence"
          classification="reviewed-public" review="SYNTHETIC ENGINE TEST ONLY"
          houseId="0" x="105" y="100" z="7" ground="100">
  <place role="table" x="105" y="101" z="7" ground="100" />
  <place role="food" x="105" y="102" z="7" ground="100" />
</interior>
```

IDs/roles: lowercase ASCII letters, digits, hyphens, 1–48 characters; interior IDs
and profile IDs unique. Review: nonempty human evidence reference, max 256 chars,
not a machine claim that an area is ordinary. Coordinates: X/Y 1–65535, Z 0–15.
All placement points are unique, use different roles, lie on the anchor floor and
within 16 tiles of it. Profiles/interiors allow at most 16 roles/placements and a
pilot at most 64 interiors. No optional wildcard, ownership exception or region
fuzziness exists. All item names, roles, counts and the entire map identity must
validate before any tile changes; unknown/duplicate attributes and elements fail
closed. Extra XML roots are rejected.

Anchor and placement tiles must currently contain only the expected unmarked
native ground. All HouseTiles are rejected regardless of ownership. Adjacent
HouseTiles and creatures, refresh tiles, unsafe tile flags, position handlers and
ground item actions/movement events also block placement. An occupied target
rejects the entire interior without removing or moving anything. This is stricter
than typical furnishing; adding a location requires source and original-client
review of all its paths/encounter semantics, not just an empty tile test.

Startup reports approved interiors, fixtures placed, loose rewards placed, zero
loot containers, and rejected/retired/unavailable placements. Missing/malformed
manifest or stale map identity disables the overlay with a diagnostic. Operators
must investigate rejected entries; they are never forced into place.

## Lifetime and duplication protections

- Runtime flags are independent; furnishing-only does not access the loot DB.
- Reapplying/reloading a manifest in the same process never reissues processed
  interiors. Failed manifest loading clears authorization, not prior retirement.
- After a real restart, inert fixtures can be reconstructed on freshly loaded bare
  map tiles. They are not written into OTBM, HouseTile SQL or player inventories.
- A reward uses a permanent `interior-id/role` ledger key. Committing that key
  precedes the native item placement. Failure/uncertainty means no issuance, with
  no compensating deletion/retry. A subsequent restart, new profile, changed
  count or changed map never reuses an existing key.
- Loot is loose, one-time public-world issuance. Unclaimed rewards can disappear
  with the ordinary world on restart and **never replenish**; a pre-placement crash
  can forfeit a reward. This is intentionally at-most-once, not guaranteed delivery.
- No generated container or automatic reset exists. Player contents are never
  cleared, cloned or serialized by this feature. Existing containers are obstacles.
- Do not rename IDs/roles, delete ledger rows or restore pre-issuance DB snapshots
  to replenish loot. Such deliberate operator resets invalidate the guarantee.

Migration: review/apply `migrations/003_house_loot.sql` to the intended schema only
before activating reviewed loot. It is additive and isolated. No migration is
needed for disabled/empty/furnishing-only operation. Preserve the table in backups.

## Validation and original-client checklist

Automated synthetic engine tests cover parse failures, names/counts/containers,
map identity, master and loot gates, same-session apply/reload, restart simulation,
reserved-before-crash retirement, unrelated nested player contents, ownable and
owned tile rejection, native immobility and lack of broker witness. The SQL
scenario uses two actual processes against the disposable fixture to check a
primary-key issuance survives restart and cannot be reissued with a changed hash.
`house_matrix_regeneration` rebuilds all house coordinates and review artifacts.
Normal and sanitizer suites and both Ubuntu Lua backends must remain green.

1. Production: pull/build with the two new flags absent/false. Confirm disabled
   diagnostics and unchanged house inventory/ownership/save behavior. Enable both
   with the shipped manifest: expect **0 approved / 0 fixtures / 0 loose loot /
   0 containers**. No production home should change.
2. For gameplay testing use a separate DB/map copy, a synthetic ordinary public
   room outside HouseTiles and a separately computed map SHA-1. Never approve a
   real room from appearance or sample owner=0. Add exact anchor/role coordinates
   matching that test map. Apply migration 003 only to this disposable DB.
3. Enable furnishing only: inspect native table/carpet/bones appearance and paths.
   Try dragging/picking up/destroying/rotating fixtures: initial inert types must
   remain fixtures. Confirm normal adjacent historical objects are untouched.
4. Enable loot in the isolated fixture on a fresh stable role key. Pick up the
   actual bread/coins; furnishing should remain. Carrying/selling ordinary loot is
   a one-time native operation. The broker/auto-gold must not remotely claim it.
5. Save/restart, keeping the ledger. Fixtures reconstruct; taken or unclaimed
   issued loot must not reappear. Repeat startup/reload. Do not delete ledger rows
   to make this test pass. Inspect count=1 for the stable key in zedas_house_loot.
6. Put a player container with nested items on an approved point, or use a
   HouseTile/owned house point: the whole target interior must reject and preserve
   every exact unrelated instance/content. Generated container entries must fail.
7. Change a ground ID, position, source map hash, item name, count, role or manifest
   syntax; confirm appropriate fail-closed diagnostics and no forced placement.
8. Restore disabled flags and the empty production manifest. Confirm Classic
   save/reload, door/bed/depot behavior and accepted broker systems remain normal.

Actual 7.72 observation is required for sprite appearance, furniture orientation,
path obstruction, perceived room quality and local house/quest context. Automated
source evidence cannot certify an actual public room's quest/NPC/commerce role.
