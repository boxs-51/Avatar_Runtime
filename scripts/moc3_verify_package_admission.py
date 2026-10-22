"""Real OS / GPU qualification for the static public admission slice.

Private originals and deliberately resized test derivatives stay under ignored build/.
No public fidelity or original full-resolution admission claim follows from a derivative.
"""
import argparse
from dataclasses import replace
import hashlib
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from moc3_public_package import admit_zip,validate_packet,render_packet,_run_envelope,ISOLATED_BIN,HOST_BIN
from moc3_admission_fixtures import archive,png,basic_package,drawable_packet,hostile_packets
from moc3_local_replay import _private_manifest
from moc3_admission_provenance import capture


def check(value,message):
    if not value:raise RuntimeError(message)


def run(evidence,notify=lambda _case,_frame=None:None):
    closure_before=capture()
    report={'kind':'PUB01_PUB03_STATIC_ADMISSION','public_activation':False,'physics':'VALIDATED_NOT_SIMULATED',
            'cases':[],'source_sha256':{},'binary_sha256':{}}
    def passed(name,**detail):
        report['cases'].append({'case':name,'result':'PASS',**detail});notify(name,None)
        (evidence/'evidence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    def reject_package(name,files,expected,entry='m.model3.json'):
        result=admit_zip(archive(files),entry)
        check(result.status==expected and not result.packet and result.profile_removed and result.job_empty,name)
        # Even rejection bytes accidentally delivered to the renderer must stop before device creation.
        out=evidence/'rejections'/name;gpu=render_packet(result.status.encode(),result.nonce,out)
        proof=json.loads((out/'admission.json').read_text())
        check(gpu.returncode==3 and proof['device_creation_calls']==0 and proof['gpu_resource_creations']==0,name+' before GPU')
        passed(name,status=result.status,profile_removed=True,job_empty=True,gpu_device_calls=0)
    try:
        cpu=subprocess.run([str(ISOLATED_BIN/'ars_moc3_packet_tests.exe')],capture_output=True,timeout=20)
        check(cpu.returncode==0 and b'PACKET_TEST_PASS' in cpu.stdout,'native packet corpus');passed('native_packet_corpus')
        physics_test=subprocess.run([str(ISOLATED_BIN/'ars_moc3_physics_admission_tests.exe')],capture_output=True,timeout=20)
        check(physics_test.returncode==0 and b'PHYSICS_ADMISSION_PASS' in physics_test.stdout,'native physics corpus');passed('native_physics_corpus')
        cases=hostile_packets()
        for name,data in cases.items():
            guard=validate_packet(data,42);out=evidence/'wire'/name;gpu=render_packet(data,42,out)
            proof=json.loads((out/'admission.json').read_text())
            check(guard.returncode==3 and gpu.returncode==3 and proof['device_creation_calls']==0 and proof['gpu_resource_creations']==0,'wire '+name)
            passed('wire_'+name,gpu_device_calls=0)
        # Explicit cancellation/stale request uses a different expected host nonce.
        out=evidence/'wire/cancelled';gpu=render_packet(drawable_packet(),99,out)
        check(gpu.returncode==3 and json.loads((out/'admission.json').read_text())['device_creation_calls']==0,'cancelled')
        passed('cancelled_request_before_gpu',gpu_device_calls=0)
        for hardware in (False,True):
            label='hardware' if hardware else 'warp';out=evidence/'generated'/label
            result=render_packet(drawable_packet(),42,out,hardware=hardware)
            check(result.returncode==0,'generated '+label);rgba=(out/'frame.rgba').read_bytes()
            center=(256*512+256)*4;check(rgba[center:center+4]==b'\xff\x00\x00\xff' and rgba[:4]==bytes(4),'generated numeric pixel '+label)
            passed('generated_numeric_pixel_'+label)
            previous=(out/'frame.rgba').read_bytes();old_proof=(out/'admission.json').read_bytes()
            (out/'render-proof.json').write_bytes(old_proof)
            rejected=render_packet(b'bad packet',42,out,hardware=hardware)
            proof=json.loads((out/'admission.json').read_text())
            check(rejected.returncode==3 and proof['device_creation_calls']==0 and (out/'frame.rgba').read_bytes()==previous,'retained advertised image '+label)
            passed('rejected_replacement_preserves_previous_image_'+label,gpu_device_calls=0)
        for fault in ('--query-timeout','--query-loss'):
            out=evidence/fault[2:]
            result=subprocess.run([str(HOST_BIN/'ars_moc3_packet_render_host.exe'),str(out),'42','--warp',fault],input=drawable_packet(),capture_output=True,timeout=10)
            check(result.returncode==5 and not (out/'frame.rgba').exists(),'query fault '+fault)
            check(json.loads((out/'admission.json').read_text())['status']=='QUERY_QUARANTINED_PROCESS_EXIT','query quarantine '+fault)
            passed('injected_'+fault[2:]+'_process_quarantine')
        reject_package('generated_valid_png_bad_moc',basic_package(),'PACKAGE_MOC_REJECTED')
        for name,bad in (('bad_signature',b'bad'),('bad_crc',png()[:-1]+b'x'),('trailing_png',png()+b'x'),('truncated_png',png()[:-10])):
            reject_package(name,basic_package(bad),'PACKAGE_PNG_REJECTED')
        too_wide=bytearray(png());struct.pack_into('>I',too_wide,16,8193)
        import zlib
        struct.pack_into('>I',too_wide,29,zlib.crc32(too_wide[12:29]))
        reject_package('png_edge_budget',basic_package(bytes(too_wide)),'PACKAGE_BUDGET_REJECTED')
        files=basic_package();files['../escape.txt']=b'x';reject_package('zip_traversal',files,'PACKAGE_ZIP_REJECTED')
        files=basic_package();files['T.PNG']=b'x';reject_package('zip_alias',files,'PACKAGE_ZIP_REJECTED')
        files=basic_package();files['padding.bin']=bytes(2*1024*1024);reject_package('zip_ratio',files,'PACKAGE_BUDGET_REJECTED')
        files=basic_package();files['m.model3.json']=b'{"Version":3,"Version":3,"FileReferences":{}}';reject_package('manifest_duplicate',files,'PACKAGE_MANIFEST_REJECTED')
        files=basic_package();manifest=json.loads(files['m.model3.json']);manifest['FileReferences']['Textures']=['../t.png'];files['m.model3.json']=json.dumps(manifest).encode();reject_package('reference_traversal',files,'PACKAGE_PNG_REJECTED')
        for mode,status in (('--crash','WORKER_REJECTED'),('--hang','TIMEOUT_REJECTED'),('--flood','OUTPUT_REJECTED')):
            result=_run_envelope(b'',42,mode);check(result.status==status and not result.packet and result.profile_removed and result.job_empty,'worker '+mode)
            out=evidence/'worker'/mode[2:];gpu=render_packet(result.status.encode(),42,out)
            check(gpu.returncode==3 and json.loads((out/'admission.json').read_text())['device_creation_calls']==0,'worker failure GPU')
            passed('worker_'+mode[2:],profile_removed=True,job_empty=True,gpu_device_calls=0)
        # Explicitly authorized local fixture custody. Parsing here constructs TEST INPUT only;
        # this is never called by the public broker or a product upload endpoint.
        from PIL import Image
        manifest_path=_private_manifest();check(manifest_path is not None,'private fixture')
        original_manifest=manifest_path.read_bytes();manifest=json.loads(original_manifest);refs=manifest['FileReferences']
        files={manifest_path.name:original_manifest}
        for ref in [refs['Moc'],*refs['Textures'],refs['Physics']]:files[ref]=(manifest_path.parent/ref).read_bytes()
        original=admit_zip(archive(files),manifest_path.name)
        check(original.status=='PACKAGE_BUDGET_REJECTED' and original.profile_removed and original.job_empty,'original atlas profile budget')
        passed('private_original_fixed_profile_rejection',status=original.status)
        derivative=evidence/'private-derivative';derivative.mkdir()
        for ref in refs['Textures']:
            with Image.open(io.BytesIO(files[ref])) as image:
                image=image.convert('RGBA');image.thumbnail((2048,2048),Image.Resampling.LANCZOS);buf=io.BytesIO();image.save(buf,format='PNG');files[ref]=buf.getvalue()
        for name,data in files.items():
            path=derivative/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        admitted=admit_zip(archive(files),manifest_path.name)
        check(admitted.status=='PACKET_CUSTODY' and admitted.profile_removed and admitted.job_empty,'private derivative isolation')
        check(validate_packet(admitted.packet,admitted.nonce).returncode==0,'private independent guard')
        (evidence/'packet.bin').write_bytes(admitted.packet);(evidence/'request.json').write_text(json.dumps({'nonce':admitted.nonce}),encoding='utf-8')
        passed('private_derivative_isolated_and_independently_validated')
        from moc3_package_admission import admit_package
        from moc3_local_physics import Bridge
        trusted=admit_package(derivative,manifest_path.name)
        for hardware in (False,True):
            label='hardware' if hardware else 'warp';out=evidence/'private-render'/label
            result=render_packet(admitted.packet,admitted.nonce,out,hardware=hardware)
            check(result.returncode==0,'private derivative render '+label)
            proof=json.loads((out/'admission.json').read_text());check(proof['physics']=='VALIDATED_NOT_SIMULATED','physics label')
            baseline=evidence/'private-reference'/label;baseline.mkdir(parents=True)
            bridge=Bridge(trusted,baseline,hardware)
            try:reference,_=bridge.frame({});bridge.finish()
            finally:bridge.close()
            actual=(out/'frame.rgba').read_bytes();check(actual==reference and any(actual[3::4]),'exact same-backend defaultpose parity '+label)
            passed('private_derivative_exact_defaultpose_pixels_'+label)
            notify('private_derivative_exact_defaultpose_pixels_'+label,{'backend':label,'ppm':(out/'frame.ppm').read_bytes(),'rgba':actual})
        # Physics mutations reach the actual catalog validator after real PNG/MOC decode.
        physics=json.loads(files[refs['Physics']])
        mutations={}
        bad=json.loads(json.dumps(physics));bad['Meta']['TotalInputCount']+=1;mutations['meta_count']=bad
        bad=json.loads(json.dumps(physics));bad['PhysicsSettings'][0]['Input'][0]['Source']['Id']='GeneratedMissingParameter';mutations['unknown_parameter']=bad
        bad=json.loads(json.dumps(physics));bad['PhysicsSettings'][0]['Output'][0]['VertexIndex']=0;mutations['vertex_binding']=bad
        bad=json.loads(json.dumps(physics));bad['PhysicsSettings'][0]['Vertices'][1]['Mobility']=1.1;mutations['particle_range']=bad
        bad=json.loads(json.dumps(physics));bad['PhysicsSettings'][0]['Input'].append(bad['PhysicsSettings'][0]['Input'][0]);mutations['duplicate_binding']=bad
        bad=json.loads(json.dumps(physics));bad['PhysicsSettings'][0]['Output'][0]['Reflect']=1;mutations['typed_boolean']=bad
        for name,bad in mutations.items():
            mutated=dict(files);mutated[refs['Physics']]=json.dumps(bad).encode();reject_package('physics_'+name,mutated,'PACKAGE_PHYSICS_REJECTED',manifest_path.name)
        bad=dict(files);bad[refs['Physics']]=b'{"Version":3,"Version":3}';reject_package('physics_duplicate_key',bad,'PACKAGE_PHYSICS_REJECTED',manifest_path.name)
        # Valid packet stays immutable after a rejected replacement. C++ corpus checks pointer identity.
        passed('transaction_retention_native_corpus')
        report['verdict']='PASS'
    except Exception as error:
        report.update(verdict='FAIL',reason=type(error).__name__)
        (evidence/'failure.txt').write_text(str(error),encoding='utf-8')
    finally:
        closure_after=capture()
        report['closure_before']=closure_before;report['closure_after']=closure_after
        report['source_sha256']=closure_before['sources'];report['binary_sha256']=closure_before['binaries']
        report['closure_unchanged']=closure_before==closure_after
        if not report['closure_unchanged']:report.update(verdict='FAIL',reason='SOURCE_OR_BINARY_CHANGED_DURING_RUN')
        (evidence/'evidence.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    return report


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--window',action='store_true');args=parser.parse_args()
    evidence=ROOT/'build/moc3-package-evidence'/uuid.uuid4().hex[:24];evidence.mkdir(parents=True)
    if args.window:
        from moc3_admission_observer import observe
        report=observe(lambda notify:run(evidence,notify),evidence)
    else:report=run(evidence)
    print('PUBLIC_PACKAGE_ADMISSION_'+report['verdict']);print('EVIDENCE_RUN '+evidence.name)
    return 0 if report['verdict']=='PASS' else 1

if __name__=='__main__':raise SystemExit(main())
