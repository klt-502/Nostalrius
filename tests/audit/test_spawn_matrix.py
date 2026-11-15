"""Extraction regressions; fixtures exercise source semantics, not gameplay changes."""
import csv
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

sys.dont_write_bytecode=True
import spawn_matrix as audit


def otbm_node(kind, props=b"", children=()):
    escaped=bytearray()
    for value in props:
        if value in (253,254,255):escaped.append(253)
        escaped.append(value)
    return b"\xfe"+bytes([kind])+escaped+b"".join(children)+b"\xff"


def string(value):
    raw=value.encode("utf8")
    return struct.pack("<H",len(raw))+raw


def otbm(reference="spawns.xml", extra=b"", version=2):
    town=otbm_node(13,struct.pack("<I",7)+string("Mapped town")+struct.pack("<HHB",65535,65000,7))
    waypoint=otbm_node(16,string("Anchor")+struct.pack("<HHB",500,600,8))
    props=(b"\x0b"+string(reference) if reference is not None else b"")+extra
    metadata=otbm_node(2,props,[otbm_node(12,children=[town]),otbm_node(15,children=[waypoint])])
    return b"\0"*4+otbm_node(0,struct.pack("<IHHII",version,65000,65000,3,3),[metadata])


class SpawnAuditTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.source=self.root/"spawns.xml"
        self.definitions={"dwarf":{"hostile":True,"attackable":True}}
        self.base='<spawns>\n<spawn centerx="100" centery="200" centerz="8" radius="4">\n<monster name="dwarf" x="-3" y="2" z="8" spawntime="600"/>\n</spawn>\n</spawns>'
    def extract(self,text=None):
        self.source.write_text(text or self.base)
        notes=[]
        rows,groups=audit.extract(self.source,self.root,self.definitions,notes)
        return rows,groups,notes
    def test_signed_offsets_and_exact_source_trace(self):
        rows,groups,notes=self.extract()
        row=rows[0]
        self.assertEqual((row['x'],row['y'],row['z']),(97,202,8))
        self.assertEqual((row['source_group_index'],row['source_child_index'],row['source_line']),(1,1,3))
        self.assertEqual(row['spawntime_seconds'],600)
        self.assertEqual(groups[0]['center'],[100,200,8]);self.assertEqual(notes,[])
    def test_z_is_group_floor_not_a_monster_offset(self):
        rows,_,notes=self.extract(self.base.replace('z="8" spawntime','z="3" spawntime'))
        self.assertEqual(rows[0]['z'],8);self.assertEqual(rows[0]['source_z'],3)
        self.assertEqual(notes[0]['kind'],'ignored_monster_z')
    def test_native_optional_defaults_are_explicit(self):
        rows,groups,notes=self.extract(self.base.replace(' radius="4"','').replace(' z="8" spawntime',' spawntime'))
        self.assertEqual(rows[0]['direction'],0);self.assertTrue(rows[0]['direction_defaulted'])
        self.assertEqual(groups[0]['radius'],-1);self.assertTrue(groups[0]['radius_defaulted'])
        self.assertIsNone(rows[0]['source_z']);self.assertEqual(len(notes),1)
    def test_explicit_direction(self):
        rows,_,_=self.extract(self.base.replace('spawntime="600"','spawntime="600" direction="5"'))
        self.assertEqual(rows[0]['direction'],5);self.assertFalse(rows[0]['direction_defaulted'])
    def test_duplicates_preserved_and_empty_groups_counted(self):
        monster='<monster name="dwarf" x="-3" y="2" z="8" spawntime="600"/>'
        text=self.base.replace(monster,monster+monster).replace('</spawns>','<spawn centerx="1" centery="2" centerz="7"/></spawns>')
        rows,groups,_=self.extract(text)
        self.assertEqual(len(rows),2);self.assertEqual(len(groups),2)
        self.assertEqual([r['source_child_index'] for r in rows],[1,2]);self.assertEqual(groups[1]['monster_slots'],0)
    def test_npc_child_does_not_shift_source_identity(self):
        rows,_,_=self.extract(self.base.replace('<monster','<npc name="Existing NPC"/><monster'))
        self.assertEqual(rows[0]['source_child_index'],2)
    def test_missing_required_values_fail_with_location(self):
        for remove in (' name="dwarf"',' x="-3"',' y="2"',' spawntime="600"',' centerx="100"',' centery="200"',' centerz="8"'):
            with self.subTest(remove=remove),self.assertRaisesRegex(audit.AuditError,'spawns.xml:'):
                self.extract(self.base.replace(remove,''))
    def test_malformed_and_overflow_values_fail(self):
        for old,new in [('spawntime="600"','spawntime="1"'),('spawntime="600"','spawntime="600.5"'),
                        ('x="-3"','x="oops"'),('x="-3"','x="-101"'),('centerx="100"','centerx="65536"'),
                        ('radius="4"','radius="-2"'),('spawntime="600"','spawntime="600" direction="8"')]:
            with self.subTest(new=new),self.assertRaises(audit.AuditError):self.extract(self.base.replace(old,new))
    def test_unknown_monster_and_attributes_fail(self):
        for old,new in [('name="dwarf"','name="unregistered"'),('spawntime="600"','spawntime="600" hidden="yes"')]:
            with self.subTest(new=new),self.assertRaises(audit.AuditError):self.extract(self.base.replace(old,new))
    def test_malformed_xml_and_dtd_surface(self):
        for text in (self.base[:-8], '<!DOCTYPE spawns [<!ENTITY x "bad">]>'+self.base):
            with self.subTest(text=text[:25]),self.assertRaises(audit.AuditError):self.extract(text)
    def monster_fixture(self,flags):
        folder=self.root/'data/monster';folder.mkdir(parents=True,exist_ok=True)
        (folder/'monsters.xml').write_text('<?xml version="1.0" encoding="UTF - 8"?>\n<monsters><monster name="dwarf" file="dwarf.xml"/></monsters>')
        (folder/'dwarf.xml').write_text('<monster name="Dwarf"><flags>'+flags+'</flags></monster>')
    def test_legacy_declaration_and_flag_defaults(self):
        self.monster_fixture('<flag attackable="0"/>')
        notes=[];definitions=audit.monsters(self.root,notes)
        self.assertEqual(len(notes),1);self.assertEqual(notes[0]['kind'],'legacy_xml_encoding_declaration')
        self.assertTrue(definitions['dwarf']['hostile']);self.assertFalse(definitions['dwarf']['attackable'])
        self.assertEqual(definitions['dwarf']['hostile_source'],'MonsterType::info default')
    def test_only_first_flag_attribute_is_native_effective(self):
        self.monster_fixture('<flag hostile="0" attackable="0"/>')
        notes=[];definitions=audit.monsters(self.root,notes)
        self.assertFalse(definitions['dwarf']['hostile']);self.assertTrue(definitions['dwarf']['attackable'])
        self.assertTrue(any(n['kind']=='ignored_extra_flag_attributes' for n in notes))
    def test_invalid_flag_and_missing_monster_file_surface(self):
        self.monster_fixture('<flag hostile="maybe"/>')
        with self.assertRaisesRegex(audit.AuditError,'malformed boolean'):audit.monsters(self.root,[])
        (self.root/'data/monster/dwarf.xml').unlink()
        with self.assertRaises(audit.AuditError):audit.monsters(self.root,[])
    def test_duplicate_registry_name_surfaces(self):
        self.monster_fixture('')
        path=self.root/'data/monster/monsters.xml';path.write_text(path.read_text().replace('</monsters>','<monster name="DWARF" file="dwarf.xml"/></monsters>'))
        with self.assertRaisesRegex(audit.AuditError,'duplicate'):audit.monsters(self.root,[])
    def test_real_otbm_escaping_and_named_metadata(self):
        path=self.root/'map.otbm';path.write_bytes(otbm())
        result=audit.read_otbm(path)
        self.assertEqual((result['width'],result['height']),(65000,65000))
        self.assertEqual(result['spawn_reference'],'spawns.xml')
        self.assertEqual(result['towns'][0]['x'],65535);self.assertEqual(result['towns'][0]['y'],65000)
        self.assertEqual(result['waypoints'][0]['name'],'Anchor')
    def test_truncated_or_unsupported_otbm_fails(self):
        for raw in (otbm()[:-1],otbm(version=3),otbm(extra=b'\x63'),b'BAD!'+otbm()[4:]):
            path=self.root/'map.otbm';path.write_bytes(raw)
            with self.subTest(raw=raw[:12]),self.assertRaises(audit.AuditError):audit.read_otbm(path)
    def test_authority_reads_map_reference_and_actual_config(self):
        path=self.root/'data/world';path.mkdir(parents=True)
        (path/'map.otbm').write_bytes(otbm(reference='actual.xml'))
        (path/'actual.xml').write_text('<spawns/>')
        (self.root/'config.lua.example').write_text('mapName = "map"\nrateSpawn = 0\neasyModeEnabled = false\neasyModeRespawn = false\n')
        source,_,config=audit.authority(self.root)
        self.assertEqual(source,path/'actual.xml');self.assertEqual(config['rateSpawn'],0)
    def test_missing_map_reference_uses_literal_working_directory_fallback(self):
        path=self.root/'data/world';path.mkdir(parents=True)
        (path/'map.otbm').write_bytes(otbm(reference=None))
        (self.root/'map-spawn.xml').write_text('<spawns/>')
        (self.root/'config.lua.example').write_text('mapName = "map"\nrateSpawn = 0\neasyModeEnabled = false\neasyModeRespawn = false\n')
        source,_,_=audit.authority(self.root)
        self.assertEqual(source,self.root/'map-spawn.xml')
    def test_deterministic_components_and_floor_separation(self):
        rows=[dict(x=x,y=0,z=z,source_group_index=i,source_child_index=1) for i,(x,z) in enumerate(((0,7),(20,8),(40,7),(200,7)),1)]
        self.assertEqual([len(c) for c in audit.components(rows,32)],[3,1])
        self.assertEqual([len(c) for c in audit.components(rows,32,True)],[1,1,1,1])
        self.assertEqual(audit.components(rows,32),audit.components(rows,32))
    def test_csv_json_roundtrip_keeps_rows_and_boolean_types(self):
        rows,_,_=self.extract()
        record=[rows[0][c] for c in audit.COLUMNS]
        bundle={'columns':audit.COLUMNS,'rows':[record,record]}
        with patch('spawn_report.render',return_value='report'):
            generated=audit.outputs(bundle)
        audit.validate_encoded(bundle,generated)
        self.assertIs(json.loads(generated['docs/SPAWN_MATRIX.json'])['rows'][0][17],True)
        broken=dict(generated);broken['docs/SPAWN_MATRIX.csv']='\n'.join(broken['docs/SPAWN_MATRIX.csv'].splitlines()[:-1])
        with self.assertRaisesRegex(audit.AuditError,'row count'):audit.validate_encoded(bundle,broken)
    def test_runtime_contract_drift_requires_review(self):
        folder=self.root/'tests/audit';folder.mkdir(parents=True)
        (self.root/'runtime.cpp').write_text('changed')
        (folder/'spawn_runtime_contract.json').write_text(json.dumps({'audited_runtime_commit':'baseline','sha256':{'runtime.cpp':'wrong'}}))
        with self.assertRaisesRegex(audit.AuditError,'runtime source changed'):audit.collect(self.root)
    def test_audited_world_drift_requires_prose_and_pilot_review(self):
        folder=self.root/'tests/audit';folder.mkdir(parents=True)
        (self.root/'world.xml').write_text('changed')
        (folder/'spawn_runtime_contract.json').write_text(json.dumps({'audited_runtime_commit':'baseline','sha256':{},'audit_data_sha256':{'world.xml':'wrong'}}))
        with self.assertRaisesRegex(audit.AuditError,'audited world/config snapshot changed'):audit.collect(self.root)
    def test_check_rejects_stale_artifact_without_overwriting(self):
        rows,_,_=self.extract();bundle={'columns':audit.COLUMNS,'rows':[[rows[0][c] for c in audit.COLUMNS]],'summary':{'groups':1,'slots':1,'species':1},'diagnostics':[]}
        with patch('spawn_report.render',return_value='report'):
            generated=audit.outputs(bundle)
        for name,text in generated.items():
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)
        with patch.object(sys,'argv',['spawn_matrix.py','--root',str(self.root),'--check']),patch.object(audit,'collect',return_value=bundle),patch.object(audit,'outputs',return_value=generated),patch('sys.stdout',io.StringIO()):
            self.assertEqual(audit.main(),0)
            path=self.root/'docs/SPAWN_MATRIX.csv';path.write_text('stale')
            with patch('sys.stderr',io.StringIO()) as error:self.assertEqual(audit.main(),1)
            self.assertIn('missing/stale',error.getvalue());self.assertEqual(path.read_text(),'stale')
    def test_repository_source_bijection_independent_xml_reader(self):
        root=audit.ROOT;source=root/'data/world/spawns.xml'
        rows,groups=audit.extract(source,root,audit.monsters(root,[]),[])
        native=ET.parse(source).getroot()
        self.assertEqual(len(groups),len(native));seen=set()
        lines=source.read_text().splitlines()
        for row in rows:
            key=(row['source_group_index'],row['source_child_index']);self.assertNotIn(key,seen);seen.add(key)
            group=native[key[0]-1];monster=group[key[1]-1]
            self.assertEqual(monster.tag.lower(),'monster');self.assertEqual(row['monster'],monster.get('name'))
            self.assertEqual(row['x'],int(group.get('centerx'))+int(monster.get('x')))
            self.assertEqual(row['y'],int(group.get('centery'))+int(monster.get('y')))
            self.assertEqual(row['z'],int(group.get('centerz')))
            self.assertEqual(row['spawntime_seconds'],int(monster.get('spawntime')))
            self.assertIn('<monster ',lines[row['source_line']-1])
        self.assertEqual(len(rows),sum(c.tag.lower()=='monster' for g in native for c in g))
        self.assertEqual(len(rows),23311)


if __name__=='__main__':unittest.main()
