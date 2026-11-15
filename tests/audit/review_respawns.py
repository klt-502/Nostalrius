#!/usr/bin/env python3
"""Generate unverified dwarf review rows, or a deliberately attested draft manifest.
Never writes production world data. A draft still requires a separately reviewed commit.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
BASELINE = "9100ddf015aac7b57a69ba46af3a1c273861a607"


def audited_rows(root):
    bundle = json.loads((root / "docs/SPAWN_MATRIX.json").read_text())
    if bundle["audited_runtime_commit"] != BASELINE:
        raise ValueError("unexpected audit baseline")
    source = root / bundle["source_file"]
    if hashlib.sha256(source.read_bytes()).hexdigest() != bundle["input_sha256"][bundle["source_file"]]:
        raise ValueError("world source no longer matches historical audit")
    rows = [dict(zip(bundle["columns"], row)) for row in bundle["rows"]]
    for row in rows:
        row["slot_id"] = f"G{row['source_group_index']:05d}/C{row['source_child_index']:02d}"
    if len({r["slot_id"] for r in rows}) != len(rows):
        raise ValueError("duplicate audited source IDs")
    return rows


def candidates(rows):
    output = io.StringIO(newline="")
    fields = ["slot_id", "review_status", "verification_priority", "monster", "x", "y", "z",
              "spawntime_seconds", "source_file", "source_group_index", "source_child_index", "hold_reason"]
    writer = csv.DictWriter(output, fields, lineterminator="\n")
    writer.writeheader()
    for row in sorted(rows, key=lambda r: (r["source_group_index"] != 8455, r["source_group_index"], r["source_child_index"])):
        if row["monster"].lower() not in {"dwarf", "dwarf soldier", "dwarf guard"}:
            continue
        row = dict(row, review_status="UNVERIFIED_DO_NOT_ACTIVATE",
                   verification_priority="first proposed room" if row["source_group_index"] == 8455 else "candidate only",
                   hold_reason="encounter-level verification required" +
                   ("; exceptional historical timer" if row["spawntime_seconds"] != 600 else ""))
        writer.writerow({key: row[key] for key in fields})
    return output.getvalue()


def draft(rows, ids, evidence):
    if not evidence.strip() or any(c in evidence for c in "\r\n"):
        raise ValueError("supply one-line encounter verification evidence")
    if len(ids) != len(set(ids)) or not ids:
        raise ValueError("provide distinct explicit source-slot IDs")
    selected = {r["slot_id"]: r for r in rows}
    def key(row):
        return (row["monster"].lower(), row["x"], row["y"], row["z"], row["spawntime_seconds"])
    from collections import Counter
    matches = Counter(key(row) for row in rows)
    xml = ET.Element("easyspawns", version="1")
    xml.append(ET.Comment(" DRAFT ONLY: requires separate encounter review and production promotion "))
    for slot in sorted(ids):
        if not re.fullmatch(r"G[0-9]{5}/C[0-9]{2,}", slot) or slot not in selected:
            raise ValueError("unknown source slot: " + slot)
        row = selected[slot]
        if matches[key(row)] != 1:
            raise ValueError("ambiguous exact world slot requires separate review: " + slot)
        # Necessary safety conditions, never evidence of ordinariness.
        if not row["hostile"] or not row["attackable"]:
            raise ValueError("non-hostile/non-attackable source slot: " + slot)
        if not 500 <= row["spawntime_seconds"] <= 900:
            raise ValueError("exceptional timer requires separate policy review: " + slot)
        # Attribute escaping handles quotes; comments reject XML comment delimiters.
        note = f" {slot}; verification: {evidence} "
        if "--" in note:
            raise ValueError("verification text cannot contain XML comment delimiter")
        xml.append(ET.Comment(note))
        ET.SubElement(xml, "spawn", monster=row["monster"], x=str(row["x"]), y=str(row["y"]),
                      z=str(row["z"]), classification="ordinary", baseSeconds=str(row["spawntime_seconds"]),
                      minSeconds="120")
    ET.indent(xml)
    return '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(xml, encoding="unicode") + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--check", action="store_true", help="verify candidate review CSV")
    parser.add_argument("--slots", nargs="+", help="explicit audited IDs; emits draft XML to stdout only")
    parser.add_argument("--verified-evidence", help="human encounter verification reference; never inferred from data")
    args = parser.parse_args()
    try:
        rows = audited_rows(args.root)
        if args.slots:
            if args.check or not args.verified_evidence:
                raise ValueError("draft requires --verified-evidence and cannot use --check")
            sys.stdout.write(draft(rows, args.slots, args.verified_evidence))
        else:
            if args.verified_evidence:
                raise ValueError("evidence requires explicit --slots")
            path = args.root / "docs/RESPAWN_REVIEW_CANDIDATES.csv"
            text = candidates(rows)
            if args.check:
                if path.read_text() != text:
                    raise ValueError("stale candidate review CSV")
            else:
                path.write_text(text)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"Respawn review failed: {error}\n")


if __name__ == "__main__":
    main()
