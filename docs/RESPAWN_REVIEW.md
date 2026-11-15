# Respawn encounter verification queue

Production Easy manifest remains empty. This is a review aid, never eligibility.

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

