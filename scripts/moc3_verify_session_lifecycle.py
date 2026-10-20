"""Private R3/R4 direct integration verification. Assets/evidence stay under build/."""
import argparse,io,json,math,struct,zipfile
from pathlib import Path
from moc3_private_lease_fixture import private_fixture
from moc3_isolated_session import IsolatedSession,SessionError,STEP
from moc3_package_admission import admit_package
from moc3_local_physics import Bridge
from moc3_physics import Physics
from moc3_admission_fixtures import archive


def particles(checkpoint):
    inner=checkpoint[152:];count=struct.unpack_from('<I',inner,12)[0];remainder=struct.unpack_from('<Q',inner,32)[0];pos=72;values=[]
    for _ in range(count):
        n=struct.unpack_from('<I',inner,pos)[0];pos+=4;chain=[]
        for _ in range(n):
            x,y,vx,vy=struct.unpack_from('<4d',inner,pos);pos+=32;chain.append(((x,y),(vx,vy)))
        values.append(tuple(chain))
    assert pos==len(inner)
    return remainder,tuple(values)


def state_error(actual,expected):
    assert len(actual)==len(expected),'CHAIN_SHAPE'
    for chain,reference in zip(actual,expected):
        assert len(chain)==len(reference),'PARTICLE_SHAPE'
        for point,other in zip(chain,reference):
            assert len(point)==len(other)==2 and all(len(v)==2 for v in (*point,*other)),'VECTOR_SHAPE'
    errors=[abs(a-b) for c,d in zip(actual,expected) for p,q in zip(c,d) for v,w in zip(p,q) for a,b in zip(v,w)]
    assert errors and all(math.isfinite(v) for v in errors),'NONFINITE_STATE'
    return max(errors)


def compare(owner,reference,bridge,manual,enabled=True,reset=False):
    candidate=reference.prepare(owner.accepted.clock,manual,enabled=enabled,reset=reset)
    remainder,state=particles(owner.checkpoint)
    assert remainder==candidate.remainder
    error=state_error(state,candidate.state)
    assert error<1e-6,('PARTICLE_PARITY',error)
    expected=bridge.frame(dict(candidate.parameters))[0];actual=owner.host.readback()[0]
    assert len(expected)==len(actual)
    pixel_error=max(abs(a-b) for a,b in zip(expected,actual))
    assert pixel_error<=1,('PIXEL_PARITY',pixel_error)
    reference.commit(candidate)
    return error,pixel_error


def r2(package,entry,fixture,root,hardware):
    cases=[]
    for fault in ('malformed','target','cancel','cancel_ready','expiry','revoke'):
        owner=IsolatedSession(package,entry,root/fault,hardware)
        try:
            owner.update(control=3);old=owner.snapshot();meta=owner.host.state()
            try:owner.update(control=1,fault=fault,lease_ms=1000 if fault=='expiry' else 2000)
            except SessionError:pass
            else:raise AssertionError('FAULT_PUBLISHED')
            assert owner.snapshot()==old and not owner.host.state()['pending']
            if fault in ('cancel_ready','expiry','revoke'):assert owner.host.state()['ready_calls']>meta['ready_calls']
            if fault!='revoke':
                owner.update(control=1);assert owner.accepted.generation==2 and owner.accepted.epoch>1
            else:
                try:owner.update(control=1)
                except SessionError:pass
                else:raise AssertionError('REVOKED_AUTHORITY_RESUMED')
            cases.append({'case':fault,'result':'PASS'})
        finally:owner.close()
    (root/'evidence.json').write_text(json.dumps(cases))


def r3(package,entry,fixture,root,hardware):
    baseline=admit_package(fixture,entry);refdir=root/'reference';refdir.mkdir(parents=True,exist_ok=True)
    bridge=Bridge(baseline,refdir,hardware);owner=IsolatedSession(package,entry,root/'owner',hardware);trace=[]
    try:
        reference=Physics(baseline.linked_physics_bytes,bridge.catalog)
        drive=next(b.key for c in reference.chains for b in c.inputs if b.axis=='Angle' and b.weight>0 and bridge.catalog[b.key][2]>bridge.catalog[b.key][1])
        slot=list(bridge.catalog).index(drive);low,default,high=bridge.catalog[drive];amplitude=default+(high-default)*.5
        owner.update(control=0);compare(owner,reference,bridge,{},False)
        epochs={owner.epoch};frames=[]
        for i in range(17):
            manual={drive:amplitude} if i<3 else {};bindings=[(slot,.5)] if manual else []
            if i in (4,8,12):
                fault={4:'cancel_ready',8:'crash',12:'timeout'}[i];old=owner.snapshot()
                try:owner.update(bindings,control=1,fault=fault)
                except (SessionError,EOFError,OSError):pass
                else:raise AssertionError('FAULT_PUBLISHED')
                assert owner.snapshot()==old
            result=owner.update(bindings,control=1,fault='ack_loss' if i==6 else None,advance_ns=6*STEP+(i%2))
            if i==6:assert result['ack_lost']
            errors=compare(owner,reference,bridge,manual);epochs.add(owner.epoch);frames.append(owner.host.readback()[0])
            trace.append({'generation':owner.accepted.generation,'epoch':owner.epoch,'clock':owner.accepted.clock,'errors':errors})
        assert len(epochs)>=3 and owner.accepted.clock>1_000_000_000 and any(a!=frames[3] for a in frames[4:])
        owner.update(control=3);compare(owner,reference,bridge,{},True,True)
        # Closing command admission is permanent even while exact GPU completion drains.
        worker=owner._worker(owner.package,owner.entry,owner.package_revision,owner.checkpoint,owner.accepted.generation)
        from moc3_lease import Key
        import hashlib
        a=owner.accepted;same=a.epoch==worker.epoch
        k=Key(owner.session,a.package,worker.epoch,a.generation+1,a.clock+STEP,a.local_sequence+1 if same else 1,a.local_clock+STEP if same else STEP,a.generation+1,worker.nonce,worker.model,hashlib.sha256(owner.checkpoint).digest())
        assert owner.host.begin_lease(k,[],1)==['PREPARED']
        assert owner.host.stage_lease(worker.prepare(k,[],1))==['STAGED']
        assert owner.host.command('DELAY_ONCE')==['ARMED']
        assert owner.host.command('LEASE_CLOSE')==['BUSY']
        assert owner.host.poll()==['REJECTED']
        from dataclasses import replace
        assert owner.host.begin_lease(replace(k,epoch=k.epoch+1,local_sequence=1,local_clock=STEP),[],1)==['DENIED']
        assert owner.host.command('LEASE_CLOSE')==['CLOSED']
        bridge.finish()
    finally:bridge.close();owner.close()
    assert all(x['job_empty'] and x['profile_removed'] for x in owner.cleanups)
    (root/'evidence.json').write_text(json.dumps({'verdict':'PASS','trace':trace,'cleanup':owner.cleanups}))


def variant(package,entry,folder):
    from PIL import Image
    with zipfile.ZipFile(io.BytesIO(package)) as z:files={n:z.read(n) for n in z.namelist()}
    refs=json.loads(files[entry])['FileReferences']
    for name in refs['Textures']:
        image=Image.open(io.BytesIO(files[name])).convert('RGBA');r,g,b,a=image.split();image=Image.merge('RGBA',(b,r,g,a));out=io.BytesIO();image.save(out,format='PNG');files[name]=out.getvalue()
    folder.mkdir(parents=True,exist_ok=True)
    for name,data in files.items():target=folder/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    return archive(files)


def r4(package,entry,fixture,root,hardware):
    second=variant(package,entry,root/'second');owner=IsolatedSession(package,entry,root/'owner',hardware)
    try:
        owner.update(control=0)
        # Capture an actual old-package candidate for delayed-output replay.
        captured=[];prepare=owner.worker.prepare
        def capture(*args,**kwargs):
            packet=prepare(*args,**kwargs);captured.append(packet);return packet
        owner.worker.prepare=capture;owner.update(control=0)
        old=owner.snapshot();old_key=owner.accepted;old_package=owner.package
        for fault in ('target','cancel_ready'):
            try:owner.update(control=3,package=second,entry=entry,fault=fault)
            except SessionError:pass
            else:raise AssertionError('FAILED_REPLACEMENT_PUBLISHED')
            assert owner.snapshot()==old and owner.package==old_package and owner.package_revision==1
        try:owner.replace(b'invalid ZIP',entry)
        except (ValueError,RuntimeError,EOFError):pass
        else:raise AssertionError('BAD_PACKAGE_ACCEPTED')
        assert owner.snapshot()==old and owner.package_revision==1
        owner.update(control=0);assert owner.accepted.package==1
        before=owner.snapshot();owner.replace(second,entry)
        assert owner.accepted.package==2 and owner.accepted.epoch>old_key.epoch and owner.accepted.clock==0
        assert owner.snapshot()[2]!=before[2] and owner.accepted.generation==4
        from moc3_lease import Key,EMPTY
        stale=Key(owner.session,1,owner.epoch+1,5,0,1,0,5,1,old_key.model,EMPTY)
        state=owner.snapshot();calls=owner.host.state()['device_calls']
        assert owner.host.begin_lease(stale,[],3)==['DENIED']
        assert owner.snapshot()==state and owner.host.state()['device_calls']==calls
        # A cross-package restore is rejected by transport before any child starts.
        from moc3_lease import Lease
        try:Lease(package,entry,owner.session,1,owner.epoch+1,owner.checkpoint,3)
        except ValueError:pass
        else:raise AssertionError('CROSS_PACKAGE_RESTORE')
        # Delayed old candidate cannot reach GPU under a current pending key.
        import hashlib
        a=owner.accepted;w=owner.worker
        next_key=Key(owner.session,2,w.epoch,a.generation+1,STEP,a.local_sequence+1,STEP,a.generation+1,w.nonce,w.model,hashlib.sha256(owner.checkpoint).digest())
        assert owner.host.begin_lease(next_key,[],1)==['PREPARED']
        calls=owner.host.state()['creations'];assert owner.host.stage_lease(captured[-1])==['REJECTED']
        assert owner.snapshot()==state and owner.host.state()['creations']==calls
        owner._reap()
        refdir=root/'reference';refdir.mkdir(parents=True,exist_ok=True);baseline=admit_package(root/'second',entry);bridge=Bridge(baseline,refdir,hardware)
        try:compare(owner,Physics(baseline.linked_physics_bytes,bridge.catalog),bridge,{},True,True);bridge.finish()
        finally:bridge.close()
    finally:owner.close()
    assert all(x['job_empty'] and x['profile_removed'] for x in owner.cleanups)
    (root/'evidence.json').write_text(json.dumps({'verdict':'PASS','cleanup':owner.cleanups}))


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--stage',choices=('r2','r3','r4','all'),default='all');args=parser.parse_args()
    root=Path('build/alpha58-session-lifecycle');package,entry,fixture=private_fixture(root/'fixture')
    for stage in ('r2','r3','r4') if args.stage=='all' else (args.stage,):
        for hardware in (False,True):
            globals()[stage](package,entry,fixture,root/(stage+('-hardware' if hardware else '-warp')),hardware)
        print(stage.upper()+'_ISOLATED_SESSION_INTEGRATION_PASS',flush=True)
if __name__=='__main__':main()
