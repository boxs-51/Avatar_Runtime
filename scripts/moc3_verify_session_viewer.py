"""Authorized local R5 API/mapped-window proof; no assets leave build/."""
import base64,json,threading,time,urllib.request,urllib.error
from pathlib import Path
from moc3_private_lease_fixture import private_fixture
from moc3_session_viewer import Viewer
from moc3_package_admission import admit_package
from moc3_local_physics import Bridge
from moc3_physics import Physics
from moc3_isolated_session import STEP


def ppm_pixels(value):
    header=b'P6\n512 512\n255\n'
    assert type(value) is bytes and value.startswith(header) and len(value)==len(header)+512*512*3,'PPM_SHAPE'
    return value[len(header):]


def verify(hardware,root,package,entry,fixture):
    viewer=Viewer(package,entry,root,hardware);errors=[];receipt={};states=[]
    def agent():
        bridge=None
        def call(path,data,token=None,expected=200):
            raw=json.dumps(data).encode();request=urllib.request.Request(viewer.endpoint['url']+path,data=raw,headers={'Authorization':'Bearer '+(viewer.token if token is None else token),'Content-Type':'application/json'})
            try:
                with urllib.request.urlopen(request,timeout=20) as response:code=response.status;value=json.load(response)
            except urllib.error.HTTPError as e:code=e.code;value=json.load(e)
            assert code==expected,(path,code,expected)
            return value
        try:
            baseline=admit_package(fixture,entry);ref=root/'reference';ref.mkdir(parents=True,exist_ok=True);bridge=Bridge(baseline,ref,hardware)
            physics=Physics(baseline.linked_physics_bytes,bridge.catalog)
            drive=next(b.key for c in physics.chains for b in c.inputs if b.axis=='Angle' and b.weight>0 and bridge.catalog[b.key][2]>bridge.catalog[b.key][1]);slot=list(bridge.catalog).index(drive)
            status=call('/status',{});call('/status',{},token='wrong',expected=403)
            def request(status,sequence,bindings=(),control=1):return {'session':status['session'],'package':status['package'],'generation':status['generation'],'sequence':sequence,'bindings':list(bindings),'control':control,'advance_ns':6*STEP}
            call('/update',request(status,2),expected=409)
            invalid=request(status,1);invalid['unknown']=True;call('/update',invalid,expected=409)
            invalid=request(status,1);invalid['control']=True;call('/update',invalid,expected=409)
            assert call('/status',{})['generation']==0
            frames=[]
            for i in range(12):
                action=request(status,i+1,[(slot,.5)] if 1<=i<=3 else [],0 if i==0 else 1)
                status=call('/update',action);frame=call('/frame',{})
                assert frame['mapped'] and frame['generation']==frame['host_generation']==frame['presented_generation']==i+1
                assert status['sequence']==i+1
                ppm=base64.b64decode(frame['ppm_base64'],validate=True);assert ppm.startswith(b'P6\n512 512\n255\n');frames.append(ppm)
                manual={drive:bridge.catalog[drive][1]+(bridge.catalog[drive][2]-bridge.catalog[drive][1])*.5} if 1<=i<=3 else {}
                candidate=physics.prepare(frame['clock'],manual,enabled=i!=0)
                rgba=bridge.frame(dict(candidate.parameters))[0];rgb=ppm_pixels(ppm)
                expected=bytearray()
                for y in range(512):
                    for x in range(512):
                        p=(y*512+x)*4;bg=180 if ((x//16+y//16)%2) else 220
                        expected.extend(min(255,rgba[p+c]+bg*(255-rgba[p+3])//255) for c in range(3))
                assert len(expected)==len(rgb)==512*512*3
                assert max(abs(a-b) for a,b in zip(expected,rgb))<=1
                physics.commit(candidate);states.append({k:status[k] for k in ('generation','epoch','clock','presented_generation','mapped','sequence')})
                call('/update',action,expected=409)
                time.sleep(.08)
            assert len({s['epoch'] for s in states})>=2 and any(f!=frames[4] for f in frames[5:])
            bridge.finish();bridge.close();bridge=None
            close={k:status[k] for k in ('session','package','generation')};close['sequence']=13
            assert call('/revoke',close)['revoked']
            call('/update',request(status,14),expected=409)
            assert call('/frame',{})['ppm_base64']==base64.b64encode(frames[-1]).decode()
            close['sequence']=15
            assert call('/close',close)['closed']
            time.sleep(.3)
            try:call('/status',{})
            except (urllib.error.URLError,OSError):pass
            else:raise AssertionError('CLOSED_ENDPOINT_RESUMED')
            receipt.update(verdict='PASS',states=states)
        except BaseException as e:
            errors.append(e)
            # Queue owner-thread teardown even if the agent test fails.
            viewer.stop_requested.set()
        finally:
            if bridge:bridge.close()
    thread=threading.Thread(target=agent,daemon=True);thread.start();viewer.root.after(180000,viewer.stop);viewer.run();thread.join(timeout=25)
    if errors:raise errors[0]
    assert not thread.is_alive() and receipt.get('verdict')=='PASS' and viewer.presented==12
    assert all(x['job_empty'] and x['profile_removed'] for x in viewer.session.cleanups)
    receipt['cleanups']=viewer.session.cleanups;(root/'evidence.json').write_text(json.dumps(receipt))

if __name__=='__main__':
    root=Path('build/alpha58-session-viewer');package,entry,fixture=private_fixture(root/'fixture')
    for hardware in (False,True):verify(hardware,root/('hardware' if hardware else 'warp'),package,entry,fixture)
    print('R5_MAPPED_WINDOW_AGENT_API_NATIVE_PIXEL_REFERENCE_PASS')
