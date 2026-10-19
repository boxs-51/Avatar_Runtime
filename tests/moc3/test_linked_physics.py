"""Public generated Physics3 oracles, independent of private model output."""
from copy import deepcopy
import json
import math
from pathlib import Path
import sys
import threading
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from moc3_physics import Physics, PhysicsError, STEP_NS

CATALOG={'drive':(-30.,0.,30.),'out':(-2.,0.,2.)}

def fixture():
    return {'Version':3,'Meta':{'PhysicsSettingCount':1,'TotalInputCount':1,'TotalOutputCount':1,
        'VertexCount':2,'EffectiveForces':{'Gravity':{'X':0,'Y':-1},'Wind':{'X':0,'Y':0}},'Fps':60},
        'PhysicsSettings':[{'Id':'generated','Input':[{'Source':{'Target':'Parameter','Id':'drive'},
            'Type':'Angle','Weight':100,'Reflect':False}],
        'Output':[{'Destination':{'Target':'Parameter','Id':'out'},'VertexIndex':1,
                   'Type':'X','Weight':100,'Scale':1,'Reflect':False}],
        'Vertices':[{'Position':{'X':0,'Y':i},'Mobility':.8,'Delay':1,'Acceleration':1,'Radius':i}
                    for i in range(2)],
        'Normalization':{key:{'Minimum':-30,'Default':0,'Maximum':30} for key in ('Position','Angle')}}]}

def engine(value=None):
    return Physics(json.dumps(value or fixture()).encode(), CATALOG)

class LinkedPhysicsTests(unittest.TestCase):
    def test_numeric_first_step_and_constraint(self):
        e=engine();c=e.prepare(STEP_NS,{'drive':30})
        # Declared integrator: delay=.5, rotated unit gravity=(.5,sqrt(3)/2).
        dx=.5*(STEP_NS/1e9*30)**2
        dy=1+math.sqrt(3)/2*(STEP_NS/1e9*30)**2
        expected=dx/math.hypot(dx,dy)
        self.assertAlmostEqual(dict(c.parameters)['out'],expected,places=12)
        self.assertAlmostEqual(math.hypot(*c.state[0][1][0]),1,places=12)
        self.assertEqual(e.timestamp,0)  # prepare did not publish
        e.commit(c);self.assertEqual(e.timestamp,STEP_NS)

    def test_normalization_weight_reflection_and_clamp(self):
        value=fixture();binding=value['PhysicsSettings'][0]['Output'][0]
        binding.update(Reflect=True,Scale=10000,Weight=50)
        e=engine(value);c=e.prepare(STEP_NS,{'drive':30})
        self.assertEqual(dict(c.parameters)['out'],-2)
        self.assertEqual(e.normalized('drive',15,(-10,0,10)),5)

    def test_nonsaturating_weight_and_chain_source_isolation_order(self):
        value=fixture();first=value['PhysicsSettings'][0]
        first['Output'][0]['Weight']=50
        e=engine(value);candidate=e.prepare(STEP_NS,{'drive':30,'out':.4})
        raw=candidate.state[0][1][0][0]
        self.assertAlmostEqual(dict(candidate.parameters)['out'],.4*.5+raw*.5,places=12)
        # Chain 2 reads out from the original manual pose, then blends into
        # the destination value produced by chain 1 in declared file order.
        second=deepcopy(first);second['Id']='second'
        second['Input'][0]['Source']['Id']='out'
        value['PhysicsSettings'].append(second)
        value['Meta'].update(PhysicsSettingCount=2,TotalInputCount=2,TotalOutputCount=2,VertexCount=4)
        e=engine(value);c=e.prepare(STEP_NS,{'drive':30,'out':.4})
        isolated=deepcopy(value);isolated['PhysicsSettings']=[second]
        isolated['Meta'].update(PhysicsSettingCount=1,TotalInputCount=1,TotalOutputCount=1,VertexCount=2)
        ref=engine(isolated).prepare(STEP_NS,{'out':.4})
        self.assertEqual(c.state[1],ref.state[0])
        first_raw=c.state[0][1][0][0];second_raw=c.state[1][1][0][0]
        self.assertAlmostEqual(dict(c.parameters)['out'],(.4*.5+first_raw*.5)*.5+second_raw*.5,places=12)
        reversed_value=deepcopy(value);reversed_value['PhysicsSettings'].reverse()
        reversed_c=engine(reversed_value).prepare(STEP_NS,{'drive':30,'out':.4})
        self.assertNotEqual(dict(c.parameters)['out'],dict(reversed_c.parameters)['out'])

    def test_chunk_equivalence_and_fractional_clock(self):
        a,b=engine(),engine()
        for i in range(1,9):a.commit(a.prepare(i*STEP_NS,{'drive':20}))
        b.commit(b.prepare(8*STEP_NS,{'drive':20}))
        self.assertEqual(a.state,b.state)
        c=a.prepare(8*STEP_NS+1,{'drive':20});a.commit(c)
        self.assertEqual((c.steps,c.remainder),(0,1))
        zero=a.prepare(a.timestamp,{'drive':20})
        self.assertEqual((zero.steps,zero.state),(0,a.state))

    def test_release_disable_reset_reload_repeat(self):
        def replay(e):
            outputs=[]
            for i in range(1,61):
                c=e.prepare(i*STEP_NS,{'drive':20} if i<=10 else {})
                outputs.append(dict(c.parameters)['out']);e.commit(c)
            return outputs
        e=engine();first=replay(e)
        self.assertTrue(any(abs(x)>1e-5 for x in first[10:]))
        self.assertNotEqual(first[10],first[20])
        e.commit(e.prepare(0,reset=True))
        self.assertEqual(replay(e),first)
        self.assertEqual(replay(engine()),first)
        disabled=e.prepare(61*STEP_NS,{},enabled=False)
        self.assertEqual(dict(disabled.parameters)['out'],0)
        self.assertEqual(disabled.state,e.rest())

    def test_reject_without_publication_and_stale_candidate(self):
        e=engine();old=(e.state,e.timestamp,e.generation)
        for time,manual in ((-1,{}),(9*STEP_NS,{}),(1,{'unknown':0}),(1,{'drive':float('nan')}),(1,{'drive':31})):
            with self.assertRaises(PhysicsError):e.prepare(time,manual)
            self.assertEqual((e.state,e.timestamp,e.generation),old)
        a=e.prepare(STEP_NS,{'drive':20});b=engine().prepare(STEP_NS)
        with self.assertRaises(PhysicsError):e.commit(b)
        self.assertEqual(e.generation,0)
        # A failed downstream consumer simply discards prepared a.
        retry=e.prepare(STEP_NS,{'drive':20});self.assertEqual(a.state,retry.state)
        e.commit(retry)
        with self.assertRaises(PhysicsError):e.commit(a)
        with self.assertRaises(PhysicsError):e.prepare(0)

    def test_invalid_linked_descriptor_matrix(self):
        mutations=[lambda d:d.update(Version=4),
            lambda d:d['Meta'].update(VertexCount=3),
            lambda d:d['PhysicsSettings'][0]['Output'][0].update(VertexIndex=0),
            lambda d:d['PhysicsSettings'][0]['Input'][0]['Source'].update(Id='missing'),
            lambda d:d['PhysicsSettings'][0]['Vertices'][1].update(Radius=0),
            lambda d:d['PhysicsSettings'][0]['Vertices'][1].update(Mobility=2),
            lambda d:d['PhysicsSettings'][0]['Output'][0].update(Weight=-1),
            lambda d:d['PhysicsSettings'][0]['Normalization']['Angle'].update(Maximum=-1)]
        for mutation in mutations:
            value=fixture();mutation(value)
            with self.assertRaises(PhysicsError):engine(value)
        with self.assertRaises(PhysicsError):Physics(b'{"Version":3,"Version":3}',CATALOG)
        with self.assertRaises(PhysicsError):Physics(b'x'*(1024*1024+1),CATALOG)

    def test_constant_and_endpoint_normalization_and_huge_scalar(self):
        value=fixture();value['PhysicsSettings'][0]['Normalization']['Position']={'Minimum':0,'Default':0,'Maximum':0}
        value['PhysicsSettings'][0]['Normalization']['Angle']={'Minimum':0,'Default':0,'Maximum':30}
        e=engine(value)
        self.assertEqual(e.normalized('drive',20,(0,0,0)),0)
        self.assertEqual(e.normalized('drive',15,(0,0,30)),15)
        with self.assertRaises(PhysicsError):e.prepare(STEP_NS,{'drive':10**10000})

    def test_owner_thread_and_bounded_soak(self):
        e=engine();errors=[]
        def wrong():
            try:e.prepare(STEP_NS)
            except PhysicsError as error:errors.append(str(error))
        t=threading.Thread(target=wrong);t.start();t.join()
        self.assertEqual(errors,['WRONG_THREAD'])
        for i in range(1,10001):
            c=e.prepare(i*STEP_NS,{'drive':20 if i%100<50 else 0});e.commit(c)
        self.assertEqual(sum(len(s) for s in e.state),2)
        self.assertTrue(all(math.isfinite(v) for s in e.state for p,velocity in s for v in (*p,*velocity)))

if __name__=='__main__':unittest.main()
