"""Negative oracles for partial public-security evidence; not runtime proof."""
import importlib.util
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('a42',ROOT/'scripts/verify_public_security_cases.py')
a42=importlib.util.module_from_spec(spec);spec.loader.exec_module(a42)

class SecurityEvidence(unittest.TestCase):
    output='TRACE stale handle rejected\nSECURITY_CASE HDL-001 PASS seed=42 live=0\n'
    def test_exact_case_and_trace(self):a42.inspect_case(self.output,'HDL-001')
    def test_missing_duplicate_foreign_wrong_seed_or_live_rejected(self):
        for text in ('',self.output*2,self.output.replace('HDL-001','HDL-002'),self.output.replace('seed=42','seed=43'),
                     self.output.replace('live=0','live=1'),self.output.rstrip(),self.output.split('\n',1)[1],
                     'SECURITY_CASE HDL-002 PASS seed=42 live=0\n'+self.output):
            with self.subTest(text=text),self.assertRaises(RuntimeError):a42.inspect_case(text,'HDL-001')
    def test_unknown_case_rejected(self):
        with self.assertRaises(RuntimeError):a42.inspect_case(self.output,'RACE-999')
    def test_all87_preserved_with_platform_and_sanitizer_holds(self):
        data=json.loads((ROOT/a42.INVENTORY).read_text())
        rows=a42.inventory_rows(data)
        self.assertEqual(len(rows),87)
        for original,row in zip(data['cases'],rows):
            for key,value in original.items():self.assertEqual(row[key],value)
            self.assertEqual(row['local_windows']['result'],'NOT_RUN')
            self.assertEqual(set(row['required_platform_results'].values()),{'NOT_RUN'})
            self.assertEqual(row['sanitizers'],'NOT_RUN');self.assertEqual(row['qualification'],'HOLD')
    def test_missing_duplicate_or_unknown_inventory_rejected(self):
        for mode in ('missing','duplicate','unknown'):
            data=json.loads((ROOT/a42.INVENTORY).read_text())
            if mode=='missing':data['cases'].pop()
            elif mode=='duplicate':data['cases'][-1]=data['cases'][0]
            else:data['cases'][-1]['id']='RACE-999'
            with self.subTest(mode=mode),self.assertRaises(RuntimeError):a42.inventory_rows(data)
