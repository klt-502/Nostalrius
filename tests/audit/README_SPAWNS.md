# World spawn audit tools

Run from the repository root with Python 3.9+ (standard library only):

```sh
python3 tests/audit/spawn_matrix.py
python3 tests/audit/spawn_matrix.py --check
python3 -m unittest discover -s tests/audit -p 'test_spawn_matrix.py'
```

The first command writes `docs/SPAWN_MATRIX.csv`, `docs/SPAWN_MATRIX.json` and
`docs/SPAWN_AUDIT.md`. `--check` rebuilds them in memory and rejects missing/stale
bytes without overwriting. There are no database, client, credential or network
requirements. A sparse checkout must include `data/world`, `data/monster`,
`data/zedas`, `config.lua.example`, `src` and this tool directory.

The tool reads the actual map selected by the tracked example and decodes its
OTBM external-spawn reference, town anchors and waypoints. It retains one row per
monster source element, including duplicate locations; XML group/child indexes
and line numbers are explicit. Effective flags follow the registered monster
files and native defaults. All nonstandard legacy XML encoding declarations are
surfaced in diagnostics without rewriting those source files.

CSV is headered UTF-8 with literal integer fields and `true`/`false` booleans.
JSON stores a `columns` list and row arrays; reconstruct a row object with
`dict(zip(bundle['columns'], row))`. It additionally records all groups, inputs
and hashes, flag provenance, distributions, geometric cluster membership and
proposed pilot membership. Outputs have no timestamp or changing HEAD hash.

`spawn_report.py` renders the report from that bundle. The source-level
architecture, formulas and cohort recommendation were reviewed at the commit in
`spawn_runtime_contract.json`. If those runtime files or the audited map/config/manifest snapshot change,
generation fails until the source, data-specific report and pilot are reviewed
again and the contract deliberately updated. Do not
blindly refresh the contract: changed scheduling or coordinate semantics may
require changing the extractor, report and tests too.

The dwarf clustering is geometry only, with documented distance/floor assumptions.
The 600-second candidate pool and group-8455 pilot are analysis, not certified
ordinary encounters or active respawn entries. Nothing writes an engine manifest,
world spawn file, map, monster definition, configuration or runtime source file.

Tests cover signed offsets, source identity, NPC/empty groups, preserved duplicates,
ignored child Z, native optional defaults, registered flags and first-attribute
semantics, malformed/missing fields, OTBM escaping/truncation/metadata placement,
source-contract drift, stale outputs, deterministic clusters, CSV/JSON roundtrips
and an independent ElementTree comparison of every actual world monster slot.
