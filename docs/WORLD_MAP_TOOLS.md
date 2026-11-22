# Historical 7.72 Automap and local GM World Map

Implemented for `zedas-easy-mode` after accepted `6eeef3f`. The generators read
source data only. They do not start the game, connect to its DB, issue teleports,
edit map/client files, approve encounters/houses or change gameplay. Generated
assets and the operator's client data are not committed.

## Exact inputs and generation

This branch uses **`data/items/items.srv`**, loaded by `Items::loadItems` in
`src/items.cpp`; it does not use `items.otb`. `NetworkMessage::addItem` transmits
the item ID directly, except the explicit single-hop `DisguiseTarget` mapping.
The reader also matches native `Item::CreateItem(PropStream&)` PvP-field
normalization and `IOMap`'s exclusion of movable items from HouseTiles. Nested
container contents are validated but never treated as objects on the ground.

Use Python 3.10+; the generator requires only the standard library. Run from the
repository root, with the **actual unencrypted 7.72 DAT used by your client**:

```bash
python3 -B tools/automap/generate.py \
  --map data/world/map.otbm \
  --items data/items/items.srv \
  --client-dat /home/kyle/Project-Zedas/01_Client/Tibia772/Tibia.dat \
  --output generated/Automap \
  --viewer-output generated/WorldMap
```

Both destinations must be new, separate directories. Existing destinations are
rejected, even when empty. For repeat builds use new destination names; nothing
is silently overwritten. No installer/copy operation is provided. `--audit-only`
validates everything and prints the full JSON report without writing outputs.
Normal generation prints counts and writes `REPORT.json` into each bundle.
Each complete bundle is published by rename. A failure publishing the optional
viewer does not remove an already completed Automap bundle.

The OTBM, DAT and all referenced source identities are hashed in the report.
Tiles, XML coordinates, node/property bounds and mappings are checked; duplicate
source tiles, unknown attributes, truncation and unresolved IDs fail generation.
A wrong/newer/encrypted DAT is not guessed or partially accepted. All four DAT
categories are consumed, including sprite-index bounds and exact EOF. A signature
alone is not proof of client version: the operator must supply the matching file.

## Generated format and path semantics

`xxxyyyzz.map`: `xxx = X//256`, `yyy = Y//256`, `zz = Z`, padded 3/3/2 digits.
Each file covers 256×256 tiles and has:

1. 65,536 color bytes in **column-major order**: `(X%256)*256 + Y%256`.
2. 65,536 raw friction bytes in the same order.
3. Four little-endian zero bytes: no user-created map markers.

Total **131,076 bytes** per marker-free file. No OTMM header, PNG data, modern
minimap filename scheme or sprite screenshots are used in client `.map` files.
Colors come from DAT minimap attributes and the native stationary stack order,
limited to ten transmitted things. The browser's PNGs use the same color plane,
transposed to row-major scanlines and the classic 6×6×6 RGB palette.

Absent tiles stay color 0 / cost 250 (unknown). Static blocking/path-blocking and
floor-change tiles are conservatively cost 255. Walkable tiles use the **DAT's
normal ground friction**, bounded to 1–249 so reserved values remain unambiguous.
All passable grounds in the supplied data already fall in that range. DAT and
server friction differ for sandstone IDs **426/427** (DAT 70; server 10): both
source values are reported; no server correction is made. The generated client
cost matches the value the client normally sees from its DAT.

Only chunks with source tiles are emitted. Mapped black/blocked chunks are kept
because their path information is meaningful. Black pixels do not mean missing
mapping: 4,074,498 source tiles have no minimap color, chiefly blocked earth ID101
(4,042,174 tiles). No invented fallback colors paint otherwise blank terrain.

Source-static walkability is not live route authorization. Doors, access checks,
creatures, movable obstructions, decay and Easy house overlays can differ at
runtime. Classic files do not encode a full inter-floor route graph. Native-client
import, minimap refresh behavior and map-click walking require actual 7.72 testing.

## Validated authoritative dataset

World SHA-256: `4afa6b85543df78989a123172aea9d2c57b4be1c0b3c0e4860233305157cdb22`.
Supplied DAT SHA-256: `3c5e857ff72fd1e52879eb8845adc72c0991786ad0eaf85f6847595c9677aedd`;
signature `439d5a33`; item limit 5089; 254 creatures, 25 effects, 15 missiles.

- **7,839,330** source tiles, **16** floors, **456** chunks; **0 unresolved mappings**.
- **3,093** distinct server IDs appear in top-level source tile records.
- 11 towns, 861 houses, 337 NPC-home references, 23,311 individual monster slots.
- No OTBM waypoints are present; waypoint indexing is supported without inventing any.

| Z | Source tiles | Chunks |
| --- | ---: | ---: |
| 0 | 11,393 | 13 |
| 1 | 16,792 | 20 |
| 2 | 24,830 | 26 |
| 3 | 37,044 | 29 |
| 4 | 62,022 | 31 |
| 5 | 109,694 | 31 |
| 6 | 154,492 | 33 |
| 7 | 2,359,296 | 42 |
| 8 | 1,019,504 | 34 |
| 9 | 964,057 | 34 |
| 10 | 745,299 | 35 |
| 11 | 606,436 | 32 |
| 12 | 516,512 | 29 |
| 13 | 430,106 | 25 |
| 14 | 418,655 | 23 |
| 15 | 363,198 | 19 |

## Local viewer and coordinate workflow

The viewer is static HTML/CSS/JavaScript plus generated PNG chunks and a generated
entity index. It has no dependencies/CDN, analytics, credentials, WebSocket,
remote teleport function or game-server endpoint. Content Security Policy blocks
outbound connections. It draws only source-backed minimap geometry and loads
visible chunks as needed. It does not render full item sprites (no SPR required).

Serve **only the generated viewer directory**, explicitly on loopback:

```bash
python3 -m http.server 8000 --bind 127.0.0.1 --directory generated/WorldMap
```

Open `http://127.0.0.1:8000/`. Stop the server with Ctrl+C when finished. Direct
`file://` opening may depend on browser policy; loopback is the tested workflow.
This static file server cannot control Tibia and is separate from the game server.

1. Select Z, or search a town/house/NPC/monster species and click a result.
2. Drag to pan; wheel or +/− to zoom. Arrow keys pan when the canvas has focus.
   Fit floor shows that floor's source bounds.
3. Hover to inspect X/Y/Z; click to select one exact tile. Coordinates use north-up
   X-right/Y-down geometry on each actual floor, not a perspective floor offset.
4. Click **Copy** beside `/gotopos X,Y,Z`. Clipboard fallback selects the text for
   Ctrl+C. Coordinates can also be entered as `X,Y,Z` or `/gotopos X,Y,Z`.
5. Paste into the GM character. The existing access check in
   `data/talkactions/scripts/teleport_to_pos.lua` remains authoritative. Copying
   text performs no teleport. Confirm walls/placement/access in-game.

Search indexes OTBM towns/waypoints, actual XML house IDs/names/entry/footprints,
`.npc` Home positions and every XML monster slot using center+offset X/Y and group
Z exactly as `Spawns::loadFromXml` does. House selection highlights the footprint
on the selected floor; **live ownership is unknown**. Spawn results preserve group,
child slot and original interval. A species search marks matching positions on the
chosen floor; first 200 are listed and up to 5,000 are marked to bound UI work.
No hunting/quest safety classification or modern location names are inferred.

## Ankrahmun / north-desert exploration

The OTBM's actual Ankrahmun town entry is **33195,32853,8**, not floor 7. It is
present and rendered stone color129/friction100. Thais **32369,32241,7** and Carlin
**32360,31782,7** independently provide source-backed comparison points with the
same normal stone values. The report checks every town reference.

To inspect the surface, select **Z7** while retaining Ankrahmun's X/Y, then pan
north (decreasing Y). Actual generated coordinate probes include:

| Coordinate | DAT-derived color / friction | Interpretation |
| --- | --- | --- |
| 33195,32803,7 | 186 / 255 | blocked wall; not a suggested landing |
| 33195,32753,7 | 207 / 160 | sand in source geometry |
| 33195,32653,7 | 207 / 160 | sand farther north in source geometry |
| 33095,32653,7 | 86 / 255 | blocked rock; not a suggested landing |

These are coordinate probes, **not named tomb entrances or encounter approvals**.
Repository NPC dialogue explicitly refers to Kha'labal (for example Malor/Yaman).
Their exact indexed Home references are Malor **33044,32621,1** and Yaman
**33045,32620,2**; Melchior is **33143,32828,7**. Search the names to navigate without
inventing labels or assuming an NPC's live position is always its home.

No authoritative named tomb-entrance inventory was found in the indexed map data.
Use the surface geometry, yellow stair/floor-change pixels, floor selector and
copied coordinates for manual recognition in the original client. Do not attach
remembered modern names to unverified locations.

## Original-client installation and acceptance

The generator never touches your existing Automap. Locate the **active** directory
by exploring a small area with the client, exiting normally, and checking which
`.map` files were just updated. First inspect the client directory alongside
`/home/kyle/Project-Zedas/01_Client/Tibia772/Tibia.dat`. Depending on packaging and
Wine version, also inspect the selected Wine prefix's `drive_c/users/<wine-user>/`
under `Application Data/Tibia/Automap` or `AppData/Roaming/Tibia/Automap`.
Do not assume `~/.wine` if using another prefix. Native Linux clients may instead
use `~/.tibia/Automap`; that is not automatically Wine's directory.

With the client **closed**, make a complete backup of the confirmed active
Automap directory, including markers. Then explicitly copy the generated `.map`
files there. This replaces overlapping chunks and their marker sections; preserve
the backup to recover personal markers or original exploration. Restart the client.
No automatic install, merge or backup deletion is performed by these tools.

Retest in the original 7.72 client:

- Check Thais/Carlin and Ankrahmun's actual Z8 temple reference; compare visible
  walls, roads, water and stairs against the generated minimap at real player X/Y/Z.
- Switch to the desert surface Z7; use the two sand probes above and explore north.
  Confirm map orientation, chunk boundaries and no one-tile shift.
- Test normal map-click walking on a clear road, water/wall exclusion and manual
  floor changes. Verify sandstone friction refresh has normal client behavior.
- Restart the client and verify persistence; restore the backup to test rollback.
- Confirm the browser's copied `/gotopos` is accepted by a GM and the native command
  remains denied to an ordinary player. Native permissions are not changed here.

## Automated verification

`tests/automap/test_automap.py` constructs small asymmetric binary OTBM/DAT/SRV
fixtures independently: all DAT categories/EOF, escaped node boundaries, truncated
and duplicate records, direct/disguised IDs, native house stripping, nested loot,
layer order, stair/path costs, column-major byte offsets, unknown cells, zero
markers, PNG transpose/CRC, output refusal, source entities and two fresh builds'
byte identity. `test_viewer_math.js` verifies coordinate parsing/ranges/transforms
and exact command text. Both integrate with CTest (Node test when Node is found).
No proprietary DAT, client or live DB is required in CI.

The optional real-generated-world Playwright/Chromium smoke test is:

```bash
python3 tests/automap/browser_smoke.py --viewer generated/WorldMap
```

It binds a temporary localhost-only static server and exercises rendering,
16 floors, search, clipboard, click coordinates, pan, zoom and zero outbound
requests/console errors. It does not launch Tibia. The ordinary game and sanitizer
suites and Ubuntu LuaJIT ON/OFF builds remain separate required checks. Successful
automated generation does **not** replace native-client acceptance.

Format references (readers/format authors, no downloaded map packs):
[TibiaMaps binary map format](https://tibiamaps.io/guides/map-file-format),
[OTClient DAT reader and 7.55–7.72 attribute handling](https://github.com/edubart/otclient/blob/master/src/client/thingtype.cpp),
[OTClient stationary minimap color selection](https://github.com/edubart/otclient/blob/master/src/client/tile.cpp).
The TibiaMaps guide describes the broader legacy format through Tibia10; this
round's exact 7.72 native acceptance remains explicitly pending.
