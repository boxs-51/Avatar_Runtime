"""Installed private trusted-local consumer; no repository or Git lookup required."""
import sys
sys.dont_write_bytecode=True
if __name__=='__main__':
    # sys is built in. Resolve bootstrap imports only from the interpreter installation,
    # excluding the unchecked script directory (including stdlib-name shadow modules).
    _base=sys.base_prefix.replace('\\','/').rstrip('/').lower()+'/'
    sys.path[:]=[p for p in sys.path if p and p.replace('\\','/').lower().startswith(_base)]
import argparse
import hashlib
import json
from pathlib import Path,PurePosixPath

def require(value,reason):
    if not value:raise ValueError(reason)


def verify_smoke_poses(poses):
    require(poses[0]!=poses[1] and poses[0]==poses[2]==poses[11],'PRIVATE_NATIVE_RELEASE_DEFAULT')
    require(any(poses[i][0]!=poses[3][0] and poses[i][1]!=poses[3][1] for i in (4,5)),
            'PRIVATE_PHYSICS_HELD_INPUT_GEOMETRY_AND_PIXELS')


def verify_bundle(root):
    info=json.loads((root/'bundle.json').read_text(encoding='utf-8'))
    require(info['kind']=='PRIVATE_TRUSTED_LOCAL_CONSUMER' and info['public_activation'] is False,'PRIVATE_BUNDLE_KIND')
    for name,digest in info['files_sha256'].items():
        relative=PurePosixPath(name)
        require(not relative.is_absolute() and all(p not in ('..','.') for p in relative.parts) and
                '\\' not in name and ':' not in name,'PRIVATE_BUNDLE_PATH')
        path=root.joinpath(*relative.parts)
        require(path.resolve(strict=True).is_relative_to(root.resolve()),'PRIVATE_BUNDLE_ESCAPE')
        current=root
        for part in relative.parts:
            current=current/part;require(not current.is_symlink(),'PRIVATE_BUNDLE_SYMLINK')
        require(hashlib.sha256(path.read_bytes()).hexdigest()==digest,'PRIVATE_BUNDLE_HASH')
    actual={p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
    require(actual==set(info['files_sha256'])|{'bundle.json'},'PRIVATE_BUNDLE_EXTRA_FILE')
    return info


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--trusted-local',action='store_true',required=True)
    parser.add_argument('--manifest',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--hardware',action='store_true')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    result={'kind':'PRIVATE_INSTALLED_CONSUMER_SMOKE','public_activation':False}
    output=args.output.resolve()
    if output.is_relative_to(root):parser.error('evidence must be outside installed bundle')
    output.mkdir(parents=True,exist_ok=False)
    # Standard-library bootstrap only: do not execute bundled modules before checking their bytes.
    try:verify_bundle(root)
    except (OSError,ValueError,TypeError,KeyError) as error:
        result.update(verdict='FAIL',reason=type(error).__name__)
        (output/'evidence.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print('PRIVATE_INSTALLED_CONSUMER_FAIL')
        return 1
    sys.path.insert(0,str(root/'scripts'))
    from moc3_core_session import CoreSession
    from moc3_local_pixels import PixelError
    from moc3_package_admission import admit_package,AdmissionError
    from moc3_physics import PhysicsError
    session=None
    try:
        manifest=args.manifest.resolve(strict=True)
        require(not manifest.is_relative_to(root),'PRIVATE_MODEL_MUST_BE_EXTERNAL')
        package=admit_package(manifest.parent,manifest.name)
        require(package.linked_physics_bytes is not None,'PRIVATE_LINKED_PHYSICS_REQUIRED')
        session=CoreSession(package,output/'host',args.hardware,binary_dir=root/'bin')
        poses=[]
        for i in range(12):
            drive=.5 if i in (1,3,4,5) else None
            enabled=i>=3 and i!=11
            meta,rgba,ppm,geometry=session.update(drive,enabled=enabled,reset=i in (0,11))
            poses.append((hashlib.sha256(rgba).hexdigest(),geometry))
        verify_smoke_poses(poses)
        result.update(verdict='PASS',bundle_manifest_sha256=hashlib.sha256((root/'bundle.json').read_bytes()).hexdigest(),
                      final_frame=meta,memory=session.finish(),hardware_requested=args.hardware)
    except (PixelError,PhysicsError,AdmissionError,OSError,ValueError,TypeError,KeyError) as error:
        result.update(verdict='FAIL',reason=str(error) if isinstance(error,(PixelError,PhysicsError)) else type(error).__name__)
    finally:
        if session:session.close()
    (output/'evidence.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print('PRIVATE_INSTALLED_CONSUMER_'+result['verdict'])
    return 0 if result['verdict']=='PASS' else 1


if __name__=='__main__':raise SystemExit(main())
