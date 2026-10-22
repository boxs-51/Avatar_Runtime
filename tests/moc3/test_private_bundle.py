"""Generated bundle integrity/path controls; contains no model inputs."""
import hashlib
import json
from pathlib import Path
import sys
import subprocess
import unittest
import uuid
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
from moc3_private_consumer import verify_bundle,verify_smoke_poses
from moc3_private_bundle import build_bundle
from moc3_local_pixels import PixelError


class PrivateBundleTests(unittest.TestCase):
    def fixture(self):
        folder=ROOT/'build/f4-bundle-generated-tests'/uuid.uuid4().hex[:24]
        folder.mkdir(parents=True)
        (folder/'data.txt').write_bytes(b'generated-code-placeholder')
        manifest={'kind':'PRIVATE_TRUSTED_LOCAL_CONSUMER','public_activation':False,
                  'files_sha256':{'data.txt':hashlib.sha256((folder/'data.txt').read_bytes()).hexdigest()}}
        self.write(folder,manifest)
        return folder,manifest

    def write(self,folder,manifest):
        (folder/'bundle.json').write_text(json.dumps(manifest),encoding='utf-8')

    def test_exact_inventory(self):
        folder,manifest=self.fixture();self.assertEqual(verify_bundle(folder),manifest)

    def test_tampered_missing_and_extra_file(self):
        for mode in ('tampered','missing','extra'):
            folder,_=self.fixture()
            if mode=='tampered':(folder/'data.txt').write_bytes(b'changed')
            elif mode=='missing':(folder/'data.txt').unlink()
            else:(folder/'extra.model3.json').write_text('{}',encoding='utf-8')
            with self.assertRaises((PixelError,ValueError,OSError)):verify_bundle(folder)

    def test_escape_and_activation_rejected(self):
        for name in ('../outside','C:/outside','/outside','dir\\outside'):
            folder,manifest=self.fixture();manifest['files_sha256']={name:'0'*64};self.write(folder,manifest)
            with self.assertRaises((PixelError,ValueError)):verify_bundle(folder)
        folder,manifest=self.fixture();manifest['public_activation']=True;self.write(folder,manifest)
        with self.assertRaises((PixelError,ValueError)):verify_bundle(folder)

    def test_builder_does_not_overwrite_or_write_outside_build(self):
        with self.assertRaises(ValueError):build_bundle(ROOT/'unapproved-package')
        with self.assertRaises(ValueError):build_bundle(ROOT/'build')

    def test_hash_failure_precedes_any_implementation_import(self):
        folder,manifest=self.fixture()
        (folder/'data.txt').unlink()
        scripts=folder/'scripts';scripts.mkdir()
        app=scripts/'moc3_private_consumer.py';app.write_bytes((ROOT/'scripts/moc3_private_consumer.py').read_bytes())
        module=scripts/'moc3_core_session.py';module.write_bytes(b'# original generated placeholder\n')
        manifest['files_sha256']={p.relative_to(folder).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in (app,module)}
        self.write(folder,manifest)
        marker=folder.parent/(folder.name+'-unexpected-import')
        module.write_text('from pathlib import Path\nPath('+repr(str(marker))+').write_text("executed")\n',encoding='utf-8')
        result=subprocess.run([sys.executable,'-E','-B',str(app),'--trusted-local',
             '--manifest',str(folder/'external.model3.json'),'--output',str(folder.parent/(folder.name+'-evidence'))],
             capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,1)
        self.assertEqual(result.stdout.strip(),'PRIVATE_INSTALLED_CONSUMER_FAIL')
        self.assertFalse(marker.exists())

    def test_unlisted_stdlib_shadow_does_not_execute_before_inventory(self):
        folder,manifest=self.fixture();(folder/'data.txt').unlink()
        scripts=folder/'scripts';scripts.mkdir()
        app=scripts/'moc3_private_consumer.py';app.write_bytes((ROOT/'scripts/moc3_private_consumer.py').read_bytes())
        manifest['files_sha256']={'scripts/moc3_private_consumer.py':hashlib.sha256(app.read_bytes()).hexdigest()}
        self.write(folder,manifest)
        marker=folder.parent/(folder.name+'-stdlib-shadow')
        (scripts/'argparse.py').write_text('from pathlib import Path\nPath('+repr(str(marker))+').write_text("executed")\n',encoding='utf-8')
        result=subprocess.run([sys.executable,'-E','-B',str(app),'--trusted-local',
             '--manifest',str(folder/'external.model3.json'),'--output',str(folder.parent/(folder.name+'-evidence'))],
             cwd='C:/Windows',capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,1)
        self.assertEqual(result.stdout.strip(),'PRIVATE_INSTALLED_CONSUMER_FAIL')
        self.assertFalse(marker.exists())

    def test_release_only_and_single_channel_changes_do_not_prove_physics(self):
        poses=[('rest','rest'),('driven','driven'),('rest','rest')]+[('held','held')]*3+[('release','release')]*5+[('rest','rest')]
        with self.assertRaisesRegex(ValueError,'HELD_INPUT'):verify_smoke_poses(poses)
        poses[4]=('new-pixels','held')
        with self.assertRaisesRegex(ValueError,'HELD_INPUT'):verify_smoke_poses(poses)
        poses[4]=('new-pixels','new-geometry');verify_smoke_poses(poses)

if __name__=='__main__':unittest.main()
