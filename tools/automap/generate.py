#!/usr/bin/env python3
"""Generate isolated classic 7.72 Automap chunks and an optional offline GM browser."""
import argparse
import collections
import hashlib
from functools import lru_cache
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET
import zlib
sys.dont_write_bytecode = True
from world import FormatError, read_dat, read_srv, tiles, render_tile

ROOT = Path(__file__).resolve().parents[2]
PALETTE = bytes(v for n in range(256) for v in ((n//36*51,n//6%6*51,n%6*51) if n < 216 else (0,0,0)))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def png(colors):
    # .map is column-major; PNG scanlines are row-major.
    rows = b''.join(b'\0'+bytes(colors[x*256+y] for x in range(256)) for y in range(256))
    def part(kind, data):
        return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    return (b'\x89PNG\r\n\x1a\n'+part(b'IHDR',struct.pack('>IIBBBBB',256,256,8,3,0,0,0))+
            part(b'PLTE',PALETTE)+part(b'IDAT',zlib.compress(rows,9))+part(b'IEND',b''))


def source_ref(path, root):
    try: return str(path.relative_to(root))
    except ValueError: return path.name


def index_entities(root, world, meta, footprints):
    entities, sources = [], {}
    def add(kind, name, position, source, **extra):
        if len(position) != 3 or not all(isinstance(p,int) for p in position) or not (0<=position[0]<=65535 and 0<=position[1]<=65535 and 0<=position[2]<=15):
            raise FormatError(f'invalid {kind} position: {position}')
        entities.append(dict(kind=kind,name=name,position=position,source=source,**extra))
    for kind, key in (('town','towns'),('waypoint','waypoints')):
        for entry in meta[key]: add(kind,entry['name'],entry['position'],source_ref(world,root),identity=entry['id'])
    for kind, key in (('house','house_file'),('spawn','spawn_file')):
        if key not in meta: raise FormatError(f'missing authoritative {key} reference')
        path = (world.parent/meta[key]).resolve()
        sources[source_ref(path,root)] = sha(path)
        doc = ET.parse(path).getroot()
        if kind == 'house':
            seen = set()
            for entry in doc:
                hid = int(entry.attrib['houseid'])
                if hid in seen: raise FormatError(f'duplicate house ID {hid}')
                seen.add(hid)
                add('house',entry.attrib['name'],[int(entry.attrib['entry'+a]) for a in ('x','y','z')],
                    source_ref(path,root),identity=hid,footprint=sorted(footprints.get(hid,[]),key=lambda p:(p[2],p[1],p[0])))
            if set(footprints)-seen: raise FormatError('HouseTiles missing house XML identity')
        else:
            for group_id, group in enumerate(doc,1):
                center = [int(group.attrib['center'+a]) for a in ('x','y','z')]
                for child_id, entry in enumerate(group,1):
                    if entry.tag != 'monster': continue
                    add('spawn',entry.attrib['name'],[center[0]+int(entry.attrib['x']),center[1]+int(entry.attrib['y']),center[2]],
                        source_ref(path,root),group=group_id,slot=child_id,seconds=int(entry.attrib['spawntime']))
    for path in sorted((root/'data/npc').glob('*.npc')):
        text = path.read_bytes().decode('latin1')
        home = re.search(r'(?m)^Home\s*=\s*\[\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\]',text)
        name = re.search(r'(?m)^Name\s*=\s*"([^"]+)"',text)
        if home and name:
            sources[source_ref(path,root)] = sha(path)
            add('npc-home',name[1],list(map(int,home.groups())),source_ref(path,root))
    entities.sort(key=lambda e:(e['kind'],e['name'].casefold(),e['position'][2],e['position'][1],e['position'][0],str(e.get('identity','')),e.get('group',0),e.get('slot',0)))
    return entities,sources


def build(world, srv, dat, root):
    server, (client, dat_meta) = read_srv(srv), read_dat(dat)
    chunks, meta, floors, footprints = {}, {}, collections.Counter(), collections.defaultdict(list)
    unresolved, used_ids = collections.Counter(), collections.Counter()
    colorless = 0
    black_ground = collections.Counter()
    @lru_cache(maxsize=65536)
    def cell(items, ownable):
        from world import Tile
        return render_tile(Tile(0,0,0,int(ownable),0,items),server,client)
    samples = {}
    for tile in tiles(world,meta):
        key = (tile.z,tile.x//256,tile.y//256)
        color,cost,missing = cell(tile.items,bool(tile.house_id))
        if key not in chunks: chunks[key] = (bytearray(65536),bytearray([250])*65536,bytearray(65536))
        colors,costs,seen = chunks[key]
        index = (tile.x%256)*256+tile.y%256
        if seen[index]: raise FormatError(f'duplicate source tile at {tile.x},{tile.y},{tile.z}')
        seen[index] = 1; colors[index],costs[index] = color,cost
        floors[tile.z] += 1
        colorless += color == 0
        if color == 0 and tile.items: black_ground[tile.items[0]] += 1
        unresolved.update(missing); used_ids.update(tile.items)
        if tile.house_id: footprints[tile.house_id].append([tile.x,tile.y,tile.z])
        # Deterministic small source-backed samples, not labels or encounter approvals.
        if tile.z not in samples: samples[tile.z] = dict(position=[tile.x,tile.y,tile.z],items=list(tile.items),color=color,cost=cost)
    entities, sources = index_entities(root,world,meta,footprints)
    bounds = {}
    for z,bx,by in sorted(chunks):
        p = bounds.setdefault(z,[65535,65535,0,0])
        p[0],p[1],p[2],p[3] = min(p[0],bx*256),min(p[1],by*256),max(p[2],bx*256+255),max(p[3],by*256+255)
    checks = []
    for e in entities:
        if e['kind'] == 'town' or (e['kind'] == 'npc-home' and e['name'] in ('Melchior','Malor','Yaman')):
            x,y,z = e['position']; chunk = chunks.get((z,x//256,y//256)); index=(x%256)*256+y%256
            checks.append(dict(name=e['name'],kind=e['kind'],position=e['position'],
                               source_tile_present=bool(chunk and chunk[2][index]),
                               color=chunk[0][index] if chunk else None,cost=chunk[1][index] if chunk else None))
    source_hashes = {source_ref(world,root):sha(world),source_ref(srv,root):sha(srv),'operator:Tibia.dat':sha(dat),**sources}
    report = dict(format='Tibia 7.72 classic .map; column-major color+friction+zero marker count',
                  mapping='items.srv IDs, native field normalization and single-hop DisguiseTarget',
                  dat=dat_meta,ground_friction_differences=[dict(server_id=i,client_id=t.client_id,name=t.name,server=t.speed,client=client[t.client_id].speed) for i,t in sorted(server.items()) if 'Bank' in t.flags and t.client_id in client and t.speed!=client[t.client_id].speed],sources=source_hashes,tiles=sum(floors.values()),floors=dict(sorted(floors.items())),
                  chunks=len(chunks),chunks_by_floor=dict(sorted(collections.Counter(k[0] for k in chunks).items())),
                  colorless_tiles=colorless,colorless_first_items=[dict(id=i,name=server[i].name if i in server else "UNRESOLVED",tiles=n) for i,n in black_ground.most_common(12)],
                  validation_coordinates=checks,unresolved_mappings=dict(sorted(unresolved.items())),
                  used_server_ids=len(used_ids),bounds=bounds,samples=samples,
                  entities=dict(sorted(collections.Counter(e['kind'] for e in entities).items())),
                  limitations=['Static source geometry, not live doors/creatures/house overlays/decay or access permissions.',
                               'Floor-change/path-blocking cells conservatively cost 255; no cross-floor route graph.',
                               'DAT ground friction clamped to 1..249; 250 unknown, 255 blocked. No map markers generated.',
                               '7.72 native-client import/display/pathfinding still requires operator acceptance.'])
    return chunks,entities,report


def check_destination(path):
    path = path.resolve()
    if path.exists(): raise FormatError(f'refusing existing destination (including existing Automap): {path}')
    return path


def write_outputs(chunks, entities, report, output, viewer=None):
    output = check_destination(output)
    if viewer:
        viewer = check_destination(viewer)
        if output == viewer or output in viewer.parents or viewer in output.parents:
            raise FormatError('Automap and viewer output directories must be separate, not nested')
    # Parse/validate everything before writing; publish new directories via same-parent rename.
    for dest, is_viewer in ((output,False),(viewer,True)):
        if dest is None: continue
        dest.parent.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='.zedas-map-',dir=dest.parent) as work:
            tmp = Path(work)/'bundle'; tmp.mkdir()
            for (z,bx,by),(colors,costs,seen) in sorted(chunks.items()):
                name = f'{bx:03}{by:03}{z:02}'
                if is_viewer:
                    (tmp/'tiles').mkdir(exist_ok=True)
                    (tmp/'tiles'/f'{name}.png').write_bytes(png(colors))
                else: (tmp/f'{name}.map').write_bytes(colors+costs+struct.pack('<I',0))
            (tmp/'REPORT.json').write_text(json.dumps(report,sort_keys=True,indent=2)+'\n',encoding='utf-8')
            if is_viewer:
                for name in ('index.html','viewer.js','viewer.css','icon.svg'):
                    shutil.copyfile(Path(__file__).parent/'viewer'/name,tmp/name)
                data = dict(report=report,entities=entities,chunks=[[bx,by,z] for z,bx,by in sorted(chunks)])
                (tmp/'data.js').write_text('window.ZEDAS_MAP='+json.dumps(data,sort_keys=True,separators=(',',':'))+';\n',encoding='utf-8')
            if dest.exists(): raise FormatError(f'destination appeared during generation: {dest}')
            tmp.rename(dest)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map',type=Path,default=ROOT/'data/world/map.otbm')
    parser.add_argument('--items',type=Path,default=ROOT/'data/items/items.srv',help='actual items.srv; this branch does not use items.otb')
    parser.add_argument('--client-dat',type=Path,required=True,help='operator supplied unencrypted 7.72 Tibia.dat')
    parser.add_argument('--output',type=Path,default=Path('generated/Automap'))
    parser.add_argument('--viewer-output',type=Path)
    parser.add_argument('--root',type=Path,default=ROOT,help='repository root for NPC references')
    parser.add_argument('--audit-only',action='store_true',help='read and validate; write nothing')
    args = parser.parse_args(argv)
    try:
        if not args.audit_only:
            check_destination(args.output)
            if args.viewer_output: check_destination(args.viewer_output)
        chunks,entities,report = build(args.map.resolve(),args.items.resolve(),args.client_dat.resolve(),args.root.resolve())
        if report['unresolved_mappings']:
            print(json.dumps(report['unresolved_mappings'],sort_keys=True))
            raise FormatError('unresolved item mappings; no output published')
        if args.audit_only: print(json.dumps(report,sort_keys=True,indent=2))
        else:
            write_outputs(chunks,entities,report,args.output,args.viewer_output)
            summary = {key:report[key] for key in ('tiles','chunks','floors','entities','unresolved_mappings')}
            summary['report'] = str(args.output/'REPORT.json')
            print(json.dumps(summary,sort_keys=True,indent=2))
    except (OSError,ValueError,KeyError,ET.ParseError,UnicodeError) as error:
        parser.exit(1,f'Error: {error}\n')


if __name__ == '__main__': main()
