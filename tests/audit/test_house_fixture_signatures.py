import importlib.util
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('house_fixture_signature',ROOT/'tools/audit/house_fixture_signature.py')
identity=importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)

class HouseSignatureTests(unittest.TestCase):
    def row(self):
        return dict(house_id=900001,name='Fixture lease',entry_x=100,entry_y=99,entry_z=6,
                    tiles=[[101,101,6],[100,101,6],[100,100,6]])
    def test_matches_native_fixture_reference(self):
        self.assertEqual(identity.signature(self.row()),'867f4b0e0173cecd7b30d4f868523383ee99fe83')
    def test_owner_is_not_layout_identity(self):
        row=self.row();before=identity.signature(row);row['owner']=123
        self.assertEqual(identity.signature(row),before)
    def test_name_entry_and_footprint_changes_require_review(self):
        before=identity.signature(self.row())
        for change in ({'name':'Different house'},{'entry_z':7},{'tiles':[[100,100,6]]}):
            row=self.row();row.update(change);self.assertNotEqual(identity.signature(row),before)
