import unittest
import xml.etree.ElementTree as ET
import review_respawns as review

class ReviewTests(unittest.TestCase):
    def setUp(self):
        self.rows = review.audited_rows(review.ROOT)
    def test_candidates_are_not_classification(self):
        import csv, io
        rows = list(csv.DictReader(io.StringIO(review.candidates(self.rows))))
        self.assertEqual(len(rows), 954)
        self.assertEqual(sum(r['source_group_index'] == '8455' for r in rows), 10)
        self.assertTrue(all(r['review_status'] == 'UNVERIFIED_DO_NOT_ACTIVATE' for r in rows))
    def test_draft_requires_explicit_evidence(self):
        with self.assertRaises(ValueError): review.draft(self.rows, ['G08455/C01'], '')
        xml = ET.fromstring(review.draft(self.rows, ['G08455/C01'], 'SYNTHETIC TEST ONLY'))
        slot = xml.find('spawn')
        self.assertEqual(slot.get('baseSeconds'), '600')
        self.assertIsNone(slot.get('intervalPercent'))
        self.assertEqual(slot.get('minSeconds'), '120')
        for ids in (['G99999/C01'], ['G08455/C01'] * 2):
            with self.assertRaises(ValueError): review.draft(self.rows, ids, 'TEST ONLY')
    def test_production_is_empty(self):
        self.assertEqual(len(ET.parse(review.ROOT/'data/zedas/spawns.xml').getroot()), 0)
