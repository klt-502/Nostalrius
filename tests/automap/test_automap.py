"""Small independently constructed binary fixtures; no proprietary client assets in CI."""
import json
from dataclasses import replace
from pathlib import Path
import struct
import sys
import tempfile
import subprocess
import unittest
import zlib
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/automap'))
from world import FormatError, Reader, read_dat, read_srv, tiles, render_tile, Tile
from generate import build,write_outputs,png


def u16(v): return struct.pack('<H',v)
def string(s):
    b=s.encode('utf-8');return u16(len(b))+b

def node(kind,props=b'',children=()):
    escaped=b''.join(bytes([253,b]) if b>=253 else bytes([b]) for b in props)
    return bytes([254,kind])+escaped+b''.join(children)+bytes([255])


def dat_record(attrs=b''):
    return attrs+b'\xff'+bytes([1]*7)+u16(1)


def fixture(root,duplicate=False):
    (root/'data/npc').mkdir(parents=True)
    (root/'data/npc/example.npc').write_bytes(b'Name = "Caf\xe9"\nHome = [32769,32002,7]\n')
    dat=root/'Tibia.dat'
    dat.write_bytes(struct.pack('<IHHHH',0x439d5a33,105,1,1,1)+b''.join([
        dat_record(b'\0'+u16(150)+b'\x0d\x1c'+u16(24)),
        dat_record(b'\x02\x0c\x0d\x1c'+u16(186)),
        dat_record(b'\x04'), dat_record(b'\x1c'+u16(129)),
        dat_record(b'\x01\x17\x1c'+u16(210)), dat_record(),
        dat_record(b'\x15'+u16(4)+u16(215)),dat_record(),dat_record()]))
    srv=root/'items.srv'
    srv.write_text('''# Synthetic direct-client identities
TypeID = 100
Name = "grass"
Flags = {Bank,Unmove}
Attributes = {Waypoints=150}
TypeID = 101
Name = "wall"
Flags = {Bottom,Unpass,Unmove}
TypeID = 102
Name = "bag"
Flags = {Container,Take}
TypeID = 103
Name = "ordinary loot"
Flags = {Take}
TypeID = 104
Name = "stairs"
Flags = {Clip,Unmove}
TypeID = 105
Name = "disguised ground"
Flags = {Bank,Unmove,Disguise}
Attributes = {DisguiseTarget=100,Waypoints=150}
''')
    tile=node(5,bytes([1,2,9])+u16(100),[node(6,u16(102),[node(6,u16(101))])])
    wall=node(5,bytes([2,1,9])+u16(100),[node(6,u16(101))])
    house=node(14,bytes([3,3])+struct.pack('<I',7)+bytes([9])+u16(100),[node(6,u16(103))])
    world=root/'map.otbm'
    world.write_bytes(b'OTBM'+node(0,struct.pack('<IHHII',2,65535,65535,3,23),[
        node(2,b'\x0b'+string('spawns.xml')+b'\x0d'+string('houses.xml'),[
            node(4,struct.pack('<HHB',32768,32000,7),[tile,wall,house]+([tile] if duplicate else [])),
            node(4,struct.pack('<HHB',32768,32000,8),[node(5,bytes([1,1,9])+u16(105))]),
            node(12,b'',[node(13,struct.pack('<I',1)+string('Fixture Town')+struct.pack('<HHB',32769,32002,7))]),
            node(15,b'',[node(16,string('Fixture waypoint')+struct.pack('<HHB',32770,32001,7))])])]))
    (root/'houses.xml').write_text('<houses><house name="Fixture House" houseid="7" entryx="32769" entryy="32002" entryz="7"/></houses>')
    (root/'spawns.xml').write_text('<spawns><spawn centerx="32768" centery="32000" centerz="7" radius="4"><monster name="Rat" x="1" y="2" spawntime="600"/><monster name="Rat" x="-1" y="-2" spawntime="700"/></spawn></spawns>')
    return world,srv,dat


class AutomapTests(unittest.TestCase):
    def test_full_roundtrip_determinism_and_entities(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);world,srv,dat=fixture(root)
            chunks,entities,report=build(world,srv,dat,root)
            self.assertEqual(report['floors'],{7:3,8:1});self.assertEqual(report['chunks'],2)
            self.assertEqual(report['unresolved_mappings'],{})
            self.assertEqual(report['entities'],{'town':1,'waypoint':1,'house':1,'npc-home':1,'spawn':2})
            self.assertEqual(next(e['footprint'] for e in entities if e['kind']=='house'),[[32771,32003,7]])
            self.assertEqual(next(e['position'] for e in entities if e['kind']=='spawn' and e['seconds']==700),[32767,31998,7])
            write_outputs(chunks,entities,report,root/'Automap',root/'Viewer')
            again=build(world,srv,dat,root);write_outputs(*again,root/'Again')
            for p in (root/'Automap').iterdir(): self.assertEqual(p.read_bytes(),(root/'Again'/p.name).read_bytes())
            b=(root/'Automap/12812507.map').read_bytes()
            self.assertEqual(len(b),131076);self.assertEqual(b[-4:],b'\0'*4)
            self.assertEqual((b[1*256+2],b[65536+1*256+2]),(24,150)) # bag contents do not draw a wall
            self.assertEqual((b[2*256+1],b[65536+2*256+1]),(186,255)) # asymmetric layout catches transpose
            self.assertEqual((b[3*256+3],b[65536+3*256+3]),(24,150)) # movable house loot stripped
            self.assertEqual((b[0],b[65536]),(0,250)) # absent tile stays unexplored, not walkable
            self.assertIn('Fixture Town',(root/'Viewer/data.js').read_text())
            self.assertTrue((root/'Viewer/tiles/12812508.png').is_file())
            with self.assertRaises(FormatError): write_outputs(chunks,entities,report,root/'Automap')
            with self.assertRaises(FormatError): write_outputs(chunks,entities,report,root/'new',root/'new/sub')
            self.assertFalse((root/'new').exists())

    def test_cli_publication_and_fail_closed(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);world,srv,dat=fixture(root)
            base=[sys.executable,'-B',str(ROOT/'tools/automap/generate.py'),'--map',str(world),'--items',str(srv),'--client-dat',str(dat),'--root',str(root)]
            run=subprocess.run(base+['--output',str(root/'new')],capture_output=True,text=True)
            self.assertEqual(run.returncode,0,run.stderr)
            self.assertEqual(json.loads(run.stdout)['tiles'],4)
            before=(root/'new/12812507.map').read_bytes()
            run=subprocess.run(base+['--output',str(root/'new')],capture_output=True,text=True)
            self.assertNotEqual(run.returncode,0);self.assertIn('refusing existing',run.stderr)
            self.assertEqual((root/'new/12812507.map').read_bytes(),before)
            srv.write_text(srv.read_text().replace('TypeID = 100','TypeID = 65000'))
            run=subprocess.run(base+['--output',str(root/'bad')],capture_output=True,text=True)
            self.assertNotEqual(run.returncode,0);self.assertIn('unresolved',run.stderr)
            self.assertFalse((root/'bad').exists())

    def test_missing_mappings_and_duplicates_surface(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);world,srv,dat=fixture(root,True)
            with self.assertRaisesRegex(FormatError,'duplicate source tile'): build(world,srv,dat,root)
        _,_,missing=render_tile(Tile(1,1,7,0,0,(65000,)),{},{});self.assertIn('server:65000',missing)

    def test_client_layout_and_truncation(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);world,srv,dat=fixture(root)
            c,meta=read_dat(dat);self.assertEqual(meta['signature'],'439d5a33')
            self.assertEqual(c[100].color,24);self.assertEqual(c[100].speed,150)
            self.assertEqual(read_srv(srv)[105].client_id,100)
            for raw in (dat.read_bytes()[:-1],dat.read_bytes()+b'\0',dat.read_bytes()[:12]+b'\xfe'):
                bad=root/'bad.dat';bad.write_bytes(raw)
                with self.assertRaises(FormatError): read_dat(bad)
            bad=root/'bad.otbm'
            for raw in (b'OTBM\xfe',world.read_bytes()[:-1],world.read_bytes()+b'\xff'):
                bad.write_bytes(raw)
                with self.assertRaises(FormatError): list(tiles(bad,{}))

    def test_tile_layers_and_path_costs(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);_,srv,dat=fixture(root);s,c=read_srv(srv),read_dat(dat)[0]
            self.assertEqual(render_tile(Tile(1,1,7,0,0,(100,103)),s,c)[:2],(24,150)) # ordinary down item not map color
            self.assertEqual(render_tile(Tile(1,1,7,0,0,(100,104)),s,c)[:2],(210,255))
            self.assertEqual(render_tile(Tile(1,1,7,0,0,(105,)),s,c)[:2],(24,150))
            s[100]=replace(s[100],speed=10)
            self.assertEqual(render_tile(Tile(1,1,7,0,0,(100,)),s,c)[:2],(24,150)) # client DAT friction, not server typo
            self.assertEqual(render_tile(Tile(1,1,7,0,0,(102,)),s,c)[:2],(0,255))

    def test_png_transpose_and_crc(self):
        colors=bytearray(65536);colors[1*256+2]=186
        b=png(colors);self.assertEqual(b[:8],b'\x89PNG\r\n\x1a\n')
        r=Reader(b[8:]);compressed=b''
        while r.at<len(r.data):
            size=int.from_bytes(r.take(4),'big');kind=r.take(4);value=r.take(size);crc=int.from_bytes(r.take(4),'big')
            self.assertEqual(zlib.crc32(kind+value)&0xffffffff,crc)
            if kind==b'IDAT':compressed+=value
        raw=zlib.decompress(compressed);self.assertEqual(raw[2*257+1+1],186)
        self.assertEqual(raw[1*257+1+2],0)

if __name__=='__main__': unittest.main()
