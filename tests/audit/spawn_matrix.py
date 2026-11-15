#!/usr/bin/env python3
"""Read-only, deterministic world-spawn audit. No server, database or client needed."""
from __future__ import annotations

import argparse
import codecs
import collections
import csv
from dataclasses import dataclass, field
import hashlib
import io
import json
import math
from pathlib import Path
import re
import statistics
import struct
import sys
sys.dont_write_bytecode = True
from xml.parsers import expat

ROOT = Path(__file__).resolve().parents[2]
DWARFS = ("dwarf", "dwarf soldier", "dwarf guard")
HUNTING = (*DWARFS, "dragon", "dragon lord", "behemoth", "hydra", "orc", "minotaur", "cyclops", "demon", "giant spider")
COLUMNS = ["monster", "x", "y", "z", "spawntime_seconds", "center_x", "center_y", "center_z", "radius", "direction",
           "offset_x", "offset_y", "source_z", "source_file", "source_group_index", "source_child_index", "source_line",
           "hostile", "attackable", "direction_defaulted", "dwarf_cluster"]
DIRECTIONS = {0:"north", 1:"east", 2:"south", 3:"west", 4:"southwest", 5:"southeast", 6:"northwest", 7:"northeast"}


class AuditError(ValueError):
    pass


@dataclass
class Node:
    tag: str
    attrs: dict
    line: int
    children: list = field(default_factory=list)


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_xml(path, notes):
    """Preserve source lines; explicitly surface the repository's legacy UTF - 8 headers."""
    try:
        raw = path.read_bytes()
        declaration = re.match(br"(?:\xef\xbb\xbf)?\s*<\?xml\b.*?\?>", raw, re.S)
        encoding = re.search(br"encoding\s*=\s*['\"]([^'\"]+)['\"]", declaration[0]) if declaration else None
        codec = encoding[1].decode("ascii") if encoding else "utf-8-sig"
        normalized = re.sub(r"\s+", "", codec)
        codecs.lookup(normalized)
        text = raw.decode(normalized)
        if normalized != codec:
            notes.append({"kind":"legacy_xml_encoding_declaration", "file":str(path),
                          "declared":codec, "decoded_as":normalized})
        # The invalid declaration cannot be fed to strict Expat. Replace only it,
        # preserving all newlines so source-slot line numbers remain exact.
        text = re.sub(r"^(?:\ufeff)?\s*<\?xml\b.*?\?>", lambda m: re.sub(r"[^\r\n]", " ", m[0]), text, count=1, flags=re.S)
        parser = expat.ParserCreate()
        stack, roots = [], []
        def start(tag, attrs):
            node = Node(tag, dict(attrs), parser.CurrentLineNumber)
            (stack[-1].children if stack else roots).append(node)
            stack.append(node)
        def end(_):
            stack.pop()
        def forbid(*_):
            raise AuditError(f"{path}: DTD/external entities are not supported in audit inputs")
        parser.StartElementHandler, parser.EndElementHandler = start, end
        parser.StartDoctypeDeclHandler = forbid
        parser.ExternalEntityRefHandler = forbid
        parser.Parse(text, True)
        if len(roots) != 1:
            raise AuditError(f"{path}: expected one XML root")
        return roots[0]
    except (OSError, UnicodeError, LookupError, expat.ExpatError) as error:
        raise AuditError(f"{path}: {error}") from error


def integer(node, key, minimum, maximum, source, default=None):
    text = node.attrs.get(key)
    if text is None and default is not None:
        return default
    if text is None or not re.fullmatch(r"-?[0-9]+", text):
        raise AuditError(f"{source}:{node.line}: missing/malformed integer {key}={text!r}")
    value = int(text)
    if not minimum <= value <= maximum:
        raise AuditError(f"{source}:{node.line}: out-of-range {key}={text}")
    return value


def boolean(text, source):
    if text.lower() not in {"0", "1", "true", "false", "yes", "no"}:
        raise AuditError(f"{source}: malformed boolean {text!r}")
    return text[0] in "1tTyY"


def read_otbm(path):
    """Walk the real escaped-node stream; decode only root/map/town/waypoint properties."""
    raw = path.read_bytes()
    if raw[:4] not in (b"OTBM", b"\0\0\0\0"):
        raise AuditError(f"{path}: invalid OTBM signature")
    special = re.compile(b"[\xfd-\xff]")
    stack, towns, waypoints, counts, metadata = [], [], [], collections.Counter(), {}
    def string(data, at):
        if at + 2 > len(data):
            raise AuditError(f"{path}: truncated OTBM string length")
        size = struct.unpack_from("<H", data, at)[0]
        at += 2
        if at + size > len(data):
            raise AuditError(f"{path}: truncated OTBM string")
        return data[at:at+size].decode("utf-8"), at + size
    def finish(kind, data):
        try:
            if kind == 0:
                if len(data) != 16:
                    raise AuditError(f"{path}: invalid packed root header")
                version, width, height, major, minor = struct.unpack("<IHHII", data)
                if version not in (1,2):
                    raise AuditError(f"{path}: unsupported OTBM version {version}")
                metadata.update(version=version, width=width, height=height, items_major=major, items_minor=minor)
            elif kind == 2:
                at, descriptions = 0, []
                while at < len(data):
                    attr = data[at]; at += 1
                    if attr not in (1,11,13):
                        raise AuditError(f"{path}: unsupported map-data attribute {attr}")
                    value, at = string(data,at)
                    if attr == 1: descriptions.append(value)
                    else: metadata[{11:"spawn_reference",13:"house_reference"}[attr]] = value
                metadata["descriptions"] = descriptions
            elif kind in (13,16):
                at, record = 0, {}
                if kind == 13:
                    record["id"] = struct.unpack_from("<I",data)[0]; at=4
                record["name"], at = string(data,at)
                if len(data) != at + 5:
                    raise AuditError(f"{path}: malformed town/waypoint coordinates")
                record["x"], record["y"], record["z"] = struct.unpack_from("<HHB",data,at)
                (towns if kind == 13 else waypoints).append(record)
        except (struct.error,UnicodeError) as error:
            raise AuditError(f"{path}: malformed OTBM metadata: {error}") from error
    at=4
    while at < len(raw):
        match=special.search(raw,at)
        if not match:
            raise AuditError(f"{path}: unterminated OTBM node")
        index=match.start()
        if stack and stack[-1][1] is not None: stack[-1][1].extend(raw[at:index])
        elif not stack and index != at: raise AuditError(f"{path}: bytes outside OTBM root")
        token=raw[index];at=index+1
        if token == 253:
            if not stack or at == len(raw): raise AuditError(f"{path}: truncated/unscoped OTBM escape")
            if stack[-1][1] is not None: stack[-1][1].append(raw[at])
            at+=1
        elif token == 254:
            if at == len(raw): raise AuditError(f"{path}: missing OTBM node type")
            if stack and stack[-1][1] is not None:
                stack[-1][2]=bytes(stack[-1][1]);stack[-1][1]=None
            kind=raw[at];at+=1;counts[kind]+=1
            parent=stack[-1][0] if stack else None
            expected={0:None,2:0,12:2,13:12,15:2,16:15}
            if kind in expected and parent!=expected[kind]:
                raise AuditError(f"{path}: misplaced OTBM metadata node {kind}")
            stack.append([kind,bytearray() if kind in (0,2,13,16) else None,None])
        else:
            if not stack: raise AuditError(f"{path}: unmatched OTBM node end")
            kind,props,frozen=stack.pop()
            if kind in (0,2,13,16): finish(kind,bytes(props) if props is not None else frozen)
    if stack or counts[0] != 1 or counts[2] != 1:
        raise AuditError(f"{path}: incomplete/duplicate OTBM root or map-data node")
    metadata.update(towns=sorted(towns,key=lambda t:t["id"]), waypoints=waypoints,
                    node_counts={str(k):v for k,v in sorted(counts.items())})
    return metadata


def authority(root):
    config=root/"config.lua.example"
    raw=config.read_text()
    def literal(key, pattern):
        found=re.findall(r"^"+key+r"\s*=\s*("+pattern+r")\s*(?:--.*)?$",raw,re.M)
        if len(found)!=1: raise AuditError(f"{config}: expected one literal {key}")
        return found[0]
    name=literal("mapName",r'"[^"\r\n]+"')[1:-1]
    map_path=root/"data/world"/(name+".otbm")
    metadata=read_otbm(map_path)
    # Match IOMap::loadSpawns' literal fallback, which does not prepend data/world.
    source=Path(str(map_path.parent)+"/"+metadata["spawn_reference"]) if metadata.get("spawn_reference") else root/(name+"-spawn.xml")
    if not source.is_file():raise AuditError(f"authoritative spawn file missing: {source}")
    metadata["map_file"]=str(map_path.relative_to(root))
    return source,metadata,{"file":"config.lua.example", "mapName":name,
                            "rateSpawn":int(literal("rateSpawn",r"[0-9]+")),
                            "easyModeEnabled":literal("easyModeEnabled",r"true|false")=="true",
                            "easyModeRespawn":literal("easyModeRespawn",r"true|false")=="true"}


def monsters(root, notes):
    registry=root/"data/monster/monsters.xml"
    node=parse_xml(registry,notes)
    if node.tag!="monsters":raise AuditError(f"{registry}: wrong root")
    result={}
    for entry in node.children:
        name,filename=entry.attrs.get("name",""),entry.attrs.get("file","")
        if not name or not filename or name.lower() in result:
            raise AuditError(f"{registry}:{entry.line}: missing/duplicate monster registration")
        path=root/"data/monster"/filename
        if not path.resolve().is_relative_to((root/"data/monster").resolve()):
            raise AuditError(f"{registry}:{entry.line}: monster path escapes data/monster")
        definition=parse_xml(path,notes)
        if definition.tag!="monster" or not definition.attrs.get("name"):
            raise AuditError(f"{path}: invalid monster definition")
        record={"registry_name":name,"definition_name":definition.attrs["name"],
                "file":str(path.relative_to(root)),"hostile":True,"attackable":True,
                "hostile_source":"MonsterType::info default","attackable_source":"MonsterType::info default"}
        for flags in definition.children:
            if flags.tag!="flags":continue
            for flag in flags.children:
                attrs=list(flag.attrs.items())
                if not attrs:continue
                key,value=attrs[0]
                if len(attrs)>1:
                    notes.append({"kind":"ignored_extra_flag_attributes","file":str(path),"line":flag.line})
                if key.lower() in ("hostile","attackable"):
                    record[key.lower()]=boolean(value,f"{path}:{flag.line}")
                    record[key.lower()+"_source"]=f"{record['file']}:{flag.line} @{key}"
        result[name.lower()]=record
    return result


def extract(source, root, definitions, notes):
    tree=parse_xml(source,notes)
    if tree.tag!="spawns":raise AuditError(f"{source}: expected spawns root")
    rows,groups=[],[]
    rel=str(source.relative_to(root))
    for group_index,group in enumerate(tree.children,1):
        if group.tag!="spawn":raise AuditError(f"{source}:{group.line}: unknown group {group.tag}")
        if set(group.attrs)-{"centerx","centery","centerz","radius"}:
            raise AuditError(f"{source}:{group.line}: unknown group attributes")
        cx=integer(group,"centerx",0,65535,source);cy=integer(group,"centery",0,65535,source)
        cz=integer(group,"centerz",0,15,source);radius=integer(group,"radius",-1,2147483647,source,default=-1)
        count=0
        for child_index,child in enumerate(group.children,1):
            if child.tag.lower()=="npc":continue  # Not a monster slot; index still includes it.
            if child.tag.lower()!="monster":raise AuditError(f"{source}:{child.line}: unknown spawn child")
            if set(child.attrs)-{"name","x","y","z","spawntime","direction"}:
                raise AuditError(f"{source}:{child.line}: unknown monster attributes")
            name=child.attrs.get("name","")
            if not name or name!=name.strip() or name.lower() not in definitions:
                raise AuditError(f"{source}:{child.line}: missing/unregistered monster {name!r}")
            ox=integer(child,"x",-65535,65535,source);oy=integer(child,"y",-65535,65535,source)
            x,y=cx+ox,cy+oy
            if not (0<=x<=65535 and 0<=y<=65535):raise AuditError(f"{source}:{child.line}: absolute coordinate wraps")
            # boost lexical_cast<uint16_t> accepts negative offsets modulo 65536;
            # Position's final uint16_t conversion yields this same signed sum.
            if ((cx+(ox%65536))%65536,(cy+(oy%65536))%65536)!=(x,y):
                raise AuditError(f"{source}:{child.line}: engine/offset calculation mismatch")
            seconds=integer(child,"spawntime",2,2147483,source)
            direction=integer(child,"direction",0,7,source,default=0)
            source_z=integer(child,"z",0,15,source) if "z" in child.attrs else None
            if source_z!=cz:
                notes.append({"kind":"ignored_monster_z","file":rel,"line":child.line,"value":source_z,"effective_z":cz})
            definition=definitions[name.lower()]
            rows.append(dict(zip(COLUMNS,[name,x,y,cz,seconds,cx,cy,cz,radius,direction,ox,oy,source_z,rel,
                                         group_index,child_index,child.line,definition["hostile"],definition["attackable"],
                                         "direction" not in child.attrs,""])))
            count+=1
        groups.append({"index":group_index,"line":group.line,"center":[cx,cy,cz],"radius":radius,"monster_slots":count,
                       "radius_defaulted":"radius" not in group.attrs})
    rows.sort(key=lambda r:(r["z"],r["y"],r["x"],r["monster"].lower(),r["source_group_index"],r["source_child_index"]))
    identities={(r["source_group_index"],r["source_child_index"]) for r in rows}
    if len(identities)!=len(rows) or len(rows)!=sum(g["monster_slots"] for g in groups):
        raise AuditError("source slot bijection failed; no deduplication is allowed")
    return rows,groups


def histogram(values):
    return {str(k):v for k,v in sorted(collections.Counter(values).items())}


def components(rows, threshold, same_floor=False):
    """Deterministic single-link Chebyshev components; geometry, not navigability."""
    cells=collections.defaultdict(list)
    for i,row in enumerate(rows):cells[(row["x"]//threshold,row["y"]//threshold)].append(i)
    unseen=set(range(len(rows)));answer=[]
    while unseen:
        seed=min(unseen);unseen.remove(seed);queue=[seed]
        for i in queue:
            row=rows[i];cell=(row["x"]//threshold,row["y"]//threshold)
            nearby=[]
            for dx in (-1,0,1):
                for dy in (-1,0,1):
                    for j in cells[(cell[0]+dx,cell[1]+dy)]:
                        other=rows[j]
                        if j in unseen and (not same_floor or row["z"]==other["z"]) and max(abs(row["x"]-other["x"]),abs(row["y"]-other["y"]))<=threshold:
                            unseen.remove(j);nearby.append(j)
            queue.extend(sorted(nearby))
        answer.append([rows[i] for i in sorted(queue)])
    return sorted(answer,key=lambda c:(-len(c),min((r["z"],r["y"],r["x"],r["source_group_index"],r["source_child_index"]) for r in c)))


def source_id(row):
    return f"G{row['source_group_index']:05d}/C{row['source_child_index']:02d}"


def summarize(rows,groups,metadata):
    byspecies=collections.defaultdict(list)
    for row in rows:byspecies[row["monster"].lower()].append(row)
    stats={name:{"slots":len(part),"intervals":histogram(r["spawntime_seconds"] for r in part),
                 "floors":histogram(r["z"] for r in part),"hostile":part[0]["hostile"],"attackable":part[0]["attackable"]}
           for name,part in sorted(byspecies.items())}
    dwarfs=[r for r in rows if r["monster"].lower() in DWARFS]
    clusters=[]
    for i,part in enumerate(components(dwarfs,64),1):
        ident=f"D{i:02d}"
        for row in part:row["dwarf_cluster"]=ident
        bounds=[min(r["x"] for r in part),max(r["x"] for r in part),min(r["y"] for r in part),max(r["y"] for r in part)]
        mx,my=statistics.mean(r["x"] for r in part),statistics.mean(r["y"] for r in part)
        town=min(metadata["towns"],key=lambda t:((t["x"]-mx)**2+(t["y"]-my)**2,t["id"])) if metadata["towns"] else None
        clusters.append({"id":ident,"slots":len(part),"bounds_xy":bounds,"floors":histogram(r["z"] for r in part),
                         "species":dict(sorted(collections.Counter(r["monster"].lower() for r in part).items())),
                         "intervals":histogram(r["spawntime_seconds"] for r in part),
                         "nearest_projected_town_temple":town["name"] if town else None,
                         "nearest_temple_xy_distance":round(math.hypot(town["x"]-mx,town["y"]-my),1) if town else None,
                         "source_slots":[source_id(r) for r in part]})
    logical=collections.defaultdict(list)
    for r in rows:logical[(r["monster"].lower(),r["x"],r["y"],r["z"])].append(r)
    duplicates=[{"monster":key[0],"position":list(key[1:]),"sources":[source_id(r) for r in part]} for key,part in sorted(logical.items()) if len(part)>1]
    duplicate_ids={s for d in duplicates for s in d["sources"]}
    candidates=[r for r in dwarfs if r["spawntime_seconds"]==600 and r["hostile"] and r["attackable"] and source_id(r) not in duplicate_ids]
    pilot=[r for r in candidates if r["source_group_index"]==8455]
    # A reviewed analytic cohort, never an active manifest. Fail if its inputs drift.
    if len(pilot)!=10 or collections.Counter(r["monster"].lower() for r in pilot)!=collections.Counter({"dwarf":5,"dwarf soldier":3,"dwarf guard":2}):
        raise AuditError("proposed group-8455 pilot changed; re-review recommendation")
    values=[r["spawntime_seconds"] for r in rows]
    return {"groups":len(groups),"empty_groups":sum(g["monster_slots"]==0 for g in groups),
            "nonempty_groups":sum(g["monster_slots"]>0 for g in groups),"slots":len(rows),"species":len(stats),
            "floor_distribution":histogram(r["z"] for r in rows),"interval_distribution":histogram(values),
            "minimum_seconds":min(values),"maximum_seconds":max(values),"median_seconds":statistics.median(values),
            "modes_seconds":statistics.multimode(values),"species_statistics":stats,
            "uniform_species":sum(len(s["intervals"])==1 for s in stats.values()),
            "variable_species":sum(len(s["intervals"])>1 for s in stats.values()),
            "nonhostile_slots":sum(not r["hostile"] for r in rows),"nonattackable_slots":sum(not r["attackable"] for r in rows),
            "duplicate_logical_keys":duplicates,"direction_defaults":sum(r["direction_defaulted"] for r in rows),
            "source_z_mismatches":sum(r["source_z"]!=r["z"] for r in rows),
            "dwarf_total_slots":len(dwarfs),"dwarf_clusters":clusters,
            "dwarf_cluster_sensitivity":{str(d):[len(c) for c in components(dwarfs,d)] for d in (32,64,96)},
            "dwarf_same_floor_components_32":len(components(dwarfs,32,True)),
            "dwarf_candidate_slots":len(candidates),"dwarf_candidate_source_slots":[source_id(r) for r in candidates],
            "dwarf_excluded_slots":[{"source":source_id(r),"monster":r["monster"],"position":[r["x"],r["y"],r["z"]],"seconds":r["spawntime_seconds"]} for r in dwarfs if r not in candidates],
            "proposed_pilot":{"activated":False,"source_group":8455,"slots":len(pilot),"source_slots":[source_id(r) for r in pilot],
                              "base_seconds":600,"interval_percent":50,"minimum_seconds":120,"population_floor":0,"suggested_seconds":300}}


def collect(root):
    contract=json.loads((root/"tests/audit/spawn_runtime_contract.json").read_text())
    for name,digest in contract["sha256"].items():
        if sha(root/name)!=digest:raise AuditError(f"{name}: runtime source changed; re-audit the architecture and contract before regeneration")
    for name,digest in contract.get("audit_data_sha256",{}).items():
        if sha(root/name)!=digest:raise AuditError(f"{name}: audited world/config snapshot changed; re-review the data-specific report and pilot before refreshing the contract")
    source,metadata,config=authority(root)
    notes=[];definitions=monsters(root,notes)
    rows,groups=extract(source,root,definitions,notes)
    manifest=parse_xml(root/"data/zedas/spawns.xml",notes)
    if manifest.tag!="easyspawns" or manifest.attrs.get("version")!="1":raise AuditError("invalid Easy manifest header")
    summary=summarize(rows,groups,metadata)
    input_paths={"config.lua.example","data/monster/monsters.xml",metadata["map_file"],str(source.relative_to(root)),"data/zedas/spawns.xml"}
    input_paths.update(d["file"] for d in definitions.values())
    for note in notes:
        file=Path(note["file"])
        if file.is_absolute():note["file"]=str(file.relative_to(root))
    return {"schema_version":1,"audited_runtime_commit":contract["audited_runtime_commit"],
            "scope":"Tracked example configuration and repository world definitions; live config/runtime not supplied.",
            "row_format":"zip(columns, row); source_group_index/source_child_index are one-based XML element positions",
            "sort_order":["z","y","x","monster (ASCII case-insensitive)","source_group_index","source_child_index"],
            "columns":COLUMNS,"direction_legend":{str(k):v for k,v in DIRECTIONS.items()},"source_file":str(source.relative_to(root)),
            "input_sha256":{p:sha(root/p) for p in sorted(input_paths)},"runtime_sha256":contract["sha256"],
            "example_configuration":config,"map_metadata":metadata,"monster_definitions":dict(sorted(definitions.items())),
            "easy_manifest_entries":len(manifest.children),"diagnostics":notes,
            "groups":groups,"summary":summary,"rows":[[r[c] for c in COLUMNS] for r in rows]}


def outputs(bundle):
    out=io.StringIO(newline="");writer=csv.writer(out,lineterminator="\n");writer.writerow(COLUMNS)
    for row in bundle["rows"]:writer.writerow([str(x).lower() if isinstance(x,bool) else x for x in row])
    # Column-oriented row arrays avoid repeating 21 keys 23,311 times. One row per
    # line remains grep/diff friendly; metadata describes all types and ordering.
    shell={k:v for k,v in bundle.items() if k!="rows"}
    prefix=json.dumps(shell,ensure_ascii=False,sort_keys=True,indent=2)
    encoded=prefix[:-2]+',\n  "rows": [\n'+',\n'.join('    '+json.dumps(row,ensure_ascii=False,separators=(',',':')) for row in bundle["rows"])+ '\n  ]\n}\n'
    from spawn_report import render
    return {"docs/SPAWN_MATRIX.csv":out.getvalue(),"docs/SPAWN_MATRIX.json":encoded,"docs/SPAWN_AUDIT.md":render(bundle)}


def validate_encoded(bundle,generated):
    parsed=json.loads(generated["docs/SPAWN_MATRIX.json"])
    if parsed!=bundle:raise AuditError("JSON roundtrip lost metadata or rows")
    csvrows=list(csv.DictReader(io.StringIO(generated["docs/SPAWN_MATRIX.csv"])))
    if len(csvrows)!=len(bundle["rows"]):raise AuditError("CSV row count mismatch")
    for actual,record in zip(csvrows,bundle["rows"]):
        expected={k:('' if v is None else str(v).lower() if isinstance(v,bool) else str(v)) for k,v in zip(COLUMNS,record)}
        if actual!=expected:raise AuditError("CSV/JSON row/type roundtrip mismatch")


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",type=Path,default=ROOT)
    parser.add_argument("--check",action="store_true",help="validate/rebuild in memory and reject stale checked-in artifacts")
    args=parser.parse_args()
    try:
        bundle=collect(args.root.resolve());generated=outputs(bundle);validate_encoded(bundle,generated)
        for name,text in generated.items():
            path=args.root/name
            if args.check:
                if not path.is_file() or path.read_bytes()!=text.encode("utf8"):raise AuditError(f"{name}: missing/stale; regenerate with python3 tests/audit/spawn_matrix.py")
            else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(text.encode("utf8"))
        s=bundle["summary"]
        print(f"Validated {s['groups']} groups, {s['slots']} distinct source slots, {s['species']} species; CSV/JSON bijection and deterministic artifacts {'checked' if args.check else 'written'}.")
        print(f"Surfaced {len(bundle['diagnostics'])} input diagnostics; see SPAWN_AUDIT.md/JSON. No gameplay files were changed.")
    except (AuditError,OSError,ValueError) as error:
        print(f"Spawn audit failed: {error}",file=sys.stderr);return 1
    return 0


if __name__=="__main__":sys.exit(main())
