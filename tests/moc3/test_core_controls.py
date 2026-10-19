"""Generated controller protocol tests; actual pixels require the visible verifier."""
import concurrent.futures
import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from moc3_core_session import validate_control
from moc3_test_harness import Coordinator,ApiError


class CoreControlsTests(unittest.TestCase):
    def setUp(self):
        self.c=Coordinator()
        self.c.run={'run_id':'current','state':'RUNNING','suite_id':'CORE-LIVE'}
        self.value={'sequence':1,'drive':.5,'physics':True,'reset':False,'lease_ms':1000}

    def test_invalid_values_do_not_reserve_sequence(self):
        for key,value in (('sequence',True),('sequence',0),('drive',True),('drive',float('nan')),
                          ('drive',1.01),('drive',10**1000),('drive','x'),('physics',1),('reset',0),('lease_ms',99),('lease_ms',2001)):
            bad=dict(self.value);bad[key]=value
            with self.assertRaises(ApiError):self.c.control('current',bad)
            self.assertEqual(self.c.control_sequence,0)
            self.assertTrue(self.c.controls.empty())
        bad=dict(self.value);bad['extra']='x'
        with self.assertRaises(ValueError):validate_control(bad)

    def test_queue_and_replay_are_distinct(self):
        status,result=self.c.control('current',self.value)
        self.assertEqual((status,result['state']),(202,'QUEUED'))
        with self.assertRaisesRegex(ApiError,'CONTROL_REPLAY'):self.c.control('current',self.value)
        later=dict(self.value,sequence=2)
        with self.assertRaisesRegex(ApiError,'CONTROL_BUSY'):self.c.control('current',later)
        self.assertEqual(self.c.controls.get_nowait(),self.value)
        self.assertNotIn('accepted_control_sequence',self.c.run)

    def test_single_pending_across_competing_clients(self):
        def submit(sequence):
            try:return self.c.control('current',dict(self.value,sequence=sequence))[0]
            except ApiError as error:return error.code
        with concurrent.futures.ThreadPoolExecutor(8) as pool:results=list(pool.map(submit,range(1,9)))
        self.assertEqual(results.count(202),1)
        self.assertEqual(self.c.controls.qsize(),1)

    def test_stale_run_terminal_wrong_suite_and_cancel(self):
        with self.assertRaisesRegex(ApiError,'RUN_NOT_CURRENT'):self.c.control('old',self.value)
        for field,value in (('state','COMPLETE'),('suite_id','CORE-SOAK')):
            before=copy.deepcopy(self.c.run);self.c.run[field]=value
            with self.assertRaisesRegex(ApiError,'CONTROL_NOT_ACTIVE'):self.c.control('current',self.value)
            self.c.run=before
        self.c.stop_run.set()
        with self.assertRaisesRegex(ApiError,'CONTROL_NOT_ACTIVE'):self.c.control('current',self.value)

if __name__=='__main__':unittest.main()
