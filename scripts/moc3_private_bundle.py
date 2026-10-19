"""Allowlisted local-only consumer bundle; never copies model/build trees."""
import hashlib
import json
from pathlib import Path
import shutil
import uuid

ROOT=Path(__file__).resolve().parents[1]
MODULES=('moc3_private_consumer','moc3_core_session','moc3_local_integration','moc3_local_physics',
         'moc3_local_pixels','moc3_local_replay','moc3_session','moc3_package_admission',
         'moc3_physics','moc3_memory','moc3_process_tree','moc3_frame_packet')


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def build_bundle(destination):
    destination=destination.resolve()
    if not destination.is_relative_to((ROOT/'build').resolve()):raise ValueError('private build output required')
    if destination.exists():raise ValueError('new destination required')
    source=ROOT/'docs/audits/M2_MOC3_F4_LOCAL_SOURCE.json'
    native=json.loads(source.read_text(encoding='utf-8'))
    binary=ROOT/'build/m2-d-private/bin/Release/ars_moc3_product_bridge.exe'
    if sha(binary)!=native['binary_sha256'][binary.name]:raise ValueError('unverified native binary')
    for name,digest in native['source_sha256'].items():
        if Path(name).suffix in ('.cpp','.hpp','.h') and sha(ROOT/name)!=digest:
            raise ValueError('native source changed after tested build')
    files={f'scripts/{name}.py':ROOT/f'scripts/{name}.py' for name in MODULES}
    files['bin/ars_moc3_product_bridge.exe']=binary
    purism=ROOT/'build/moc3-i0/purismcore/LICENSE'
    if sha(purism)!=native['dependency']['compiled_scope_source_sha256']['LICENSE']:raise ValueError('dependency notice changed')
    files['notices/PurismCore-LICENSE.txt']=purism
    components=[{'name':'PurismCore','revision':native['dependency']['revision'],'license':'MIT'}]
    lock=json.loads((ROOT/'docs/M1_C_DEPENDENCY_LOCK.json').read_text(encoding='utf-8'))
    for dep in lock['dependencies']:
        path=ROOT/'build/m2-d-private/asset-deps'/f"{dep['name']}-{dep['commit']}"/f"{dep['name']}-{dep['commit']}"/dep['license_path']
        # Upstream libpng source directory uses the repository basename, as the existing CMake lock does.
        if dep['name']=='libpng':path=path.parent.parent/f"libpng-{dep['commit']}"/dep['license_path']
        if sha(path)!=dep['license_sha256']:raise ValueError('dependency notice mismatch')
        files[f"notices/{dep['name']}-LICENSE.txt"]=path
        components.append({'name':dep['name'],'revision':dep['commit'],'license':dep['license_expression']})
    destination.mkdir(parents=True,exist_ok=False)
    for name,path in files.items():
        target=destination/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
    notice=destination/'PRIVATE-NOTICE.txt'
    notice.write_text('Private local integration bundle. Not a shipping SDK or redistribution permission.\n'
        'No root project LICENSE has been supplied. Keep this bundle local.\n'
        'Models, textures and derived evidence must be supplied/stored externally.\n'
        'Requires Windows x64, Python 3.11+ and the compatible Microsoft C++ runtime already installed.\n'
        'Windows/D3D11/DXGI/compiler/bcrypt libraries and Python are external platform prerequisites.\n'
        'PurismCore and conservative asset dependency notices are retained under notices/.\n'
        'Public ABI, capability advertisement, arbitrary-file isolation and universal fidelity are not implemented.\n',encoding='utf-8')
    inventory={name:sha(destination/name) for name in files}
    inventory['PRIVATE-NOTICE.txt']=sha(notice)
    manifest={'kind':'PRIVATE_TRUSTED_LOCAL_CONSUMER','public_activation':False,
              'files_sha256':dict(sorted(inventory.items())),'components':components,
              'public_sdk_verdict':'HOLD','runtime':'Windows x64 / compatible MSVC runtime / Python 3.11+'}
    (destination/'bundle.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    return destination


if __name__=='__main__':
    folder=build_bundle(ROOT/'build/f4-private-bundles'/uuid.uuid4().hex[:24])
    print('PRIVATE_BUNDLE_PREPARED '+folder.name)
