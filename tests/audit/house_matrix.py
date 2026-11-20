#!/usr/bin/env python3
"""Read-only house identity inventory. Never infers encounter safety or live ownership."""
import argparse, collections, csv, hashlib, io, json, re, struct, sys
from pathlib import Path
import xml.etree.ElementTree as ET
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).parent))
from spawn_matrix import read_otbm

def collect(root):
    world=root/'data/world/map.otbm'
    metadata=read_otbm(world)
    path=world.parent/metadata['house_reference']
    houses={int(n.get('houseid')):dict(n.attrib) for n in ET.parse(path).getroot()}
    if len(houses)!=len(ET.parse(path).getroot()): raise ValueError('duplicate house IDs')
    raw=world.read_bytes(); special=re.compile(b'[\xfd-\xff]'); stack=[]; tiles=collections.defaultdict(list)
    at=4
    while at<len(raw):
        match=special.search(raw,at)
        if not match: raise ValueError('unterminated map')
        i=match.start()
        if stack: stack[-1][1].extend(raw[at:i])
        token=raw[i];at=i+1
        if token==253:
            stack[-1][1].append(raw[at]);at+=1
        elif token==254:
            kind=raw[at];at+=1
            if stack and stack[-1][0]==4:
                props=stack[-1][1]
                if len(props)<5: raise ValueError('bad tile area')
                area=struct.unpack_from('<HHB',props)
            else: area=stack[-1][2] if stack else None
            stack.append([kind,bytearray(),area])
        else:
            kind,props,area=stack.pop()
            if kind==14:
                if len(props)<6 or not area: raise ValueError('bad house tile')
                house=struct.unpack_from('<I',props,2)[0]
                tiles[house].append([area[0]+props[0],area[1]+props[1],area[2]])
    sql=(root/'nostalrius.sql').read_text()
    owners={}
    for m in re.finditer(r'INSERT INTO `houses`.*?VALUES\s*(.*?);',sql,re.S):
        for h,o in re.findall(r'\((\d+),\s*(\d+),',m[1]):
            if int(h) in owners: raise ValueError('duplicate sample house SQL rows')
            owners[int(h)]=int(o)
    rows=[]
    for hid in sorted(set(houses)|set(tiles)):
        record=houses.get(hid,{})
        rows.append(dict(house_id=hid,name=record.get('name',''),entry_x=record.get('entryx',''),entry_y=record.get('entryy',''),entry_z=record.get('entryz',''),
                         tile_count=len(tiles[hid]),sample_owner=owners.get(hid),live_owner='UNKNOWN',
                         classification='currently-owned (sample dump only)' if owners.get(hid,0)>0 else 'player-ownable house',
                         approved=False,reason='HouseTile excluded even if unowned; live ownership/encounter review required',
                         tiles=sorted(tiles[hid],key=lambda p:(p[2],p[1],p[0]))))
    npcs=[]
    for p in sorted((root/'data/npc').glob('*.npc')):
        text=p.read_bytes().decode('latin1');home=re.search(r'Home\s*=\s*\[(\d+),(\d+),(\d+)\]',text)
        name=re.search(r'Name\s*=\s*"([^"]+)"',text)
        if home and name: npcs.append(dict(name=name[1],position=list(map(int,home.groups())),source=str(p.relative_to(root)),classification='NPC/public reference; commerce/special role requires review',approved=False))
    return dict(map_file=str(world.relative_to(root)),house_file=str(path.relative_to(root)),map_sha256=hashlib.sha256(raw).hexdigest(),map_sha1=hashlib.sha1(raw).hexdigest(),
                house_file_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),houses=rows,npc_references=npcs,
                counts=dict(xml_houses=len(houses),map_house_ids=len(tiles),house_tiles=sum(map(len,tiles.values())),sample_owned=sum(o>0 for o in owners.values()),
                            npc_references=len(npcs),safe_candidates=0,production_entries=0),
                caveat='Ownership is sample dump data, never live. Non-HouseTile buildings have no authoritative region inventory; quest/special/uncertain areas remain unapproved.')

def outputs(bundle):
    out=io.StringIO(newline='');fields=['house_id','name','entry_x','entry_y','entry_z','tile_count','sample_owner','live_owner','classification','approved','reason']
    w=csv.DictWriter(out,fields,lineterminator='\n');w.writeheader()
    for r in bundle['houses']: w.writerow({k:r[k] for k in fields})
    return {'docs/HOUSE_REVIEW_QUEUE.csv':out.getvalue(),'docs/HOUSE_MATRIX.json':json.dumps(bundle,indent=2,sort_keys=True)+'\n'}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--check',action='store_true');args=p.parse_args()
    bundle=collect(ROOT)
    for name,text in outputs(bundle).items():
        path=ROOT/name
        if args.check:
            if path.read_text()!=text: raise ValueError('stale '+name)
        else:path.write_text(text)
    print(json.dumps(bundle['counts'],sort_keys=True))
if __name__=='__main__':main()
