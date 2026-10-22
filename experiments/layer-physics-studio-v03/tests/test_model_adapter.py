import io
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from model_to_studio import load_model
from run_studio import package


def sample_bundle(path,bad=None):
    im=Image.new('RGBA',(48,64),(125,140,240,205));bi=io.BytesIO();im.save(bi,format='PNG')
    manifest={'schema_version':'0.1.0','display_name':'Fixture 2D model','representations':{'2d':{'format':'ars2d-rig-v0.1','asset':'assets/rig.ars2d.json','atlas':'assets/atlas.png','layer_bindings':[{'layer_id':'base.body','part_ids':['face','hair']}]}},'visual_layers':[{'id':'base.body','z_index':0}]}
    rig={'schema_version':'0.1.0','coordinate_space':'canvas_px','canvas':{'width':48,'height':64},'parts':[{'id':p,'atlas_rect_px':[0,0,24,32],'vertices_px':[[0,0],[24,0],[24,32],[0,32]],'z_index':i} for i,p in enumerate(['face','hair'])]}
    with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as f:
        f.writestr('character.json',json.dumps(manifest))
        f.writestr('assets/rig.ars2d.json',json.dumps(rig))
        f.writestr('assets/atlas.png',bi.getvalue())
        if bad is not None:f.writestr(bad,b'bad')

class ModelImport(unittest.TestCase):
    def test_import_complete_visible_native2d_part_set(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'model.arschar';sample_bundle(p);d=load_model(p)
            self.assertEqual(len(d['layers']),3)
            self.assertEqual({l['name'] for l in d['layers'][1:]},{'face','hair'})
            self.assertTrue(all(l['imageData'].startswith('data:image/png;base64,') for l in d['layers'][1:]))
            self.assertEqual(d['version'],'0.3.0')
            html=package(Path(t)/'pack',p)
            self.assertIn('bootstrap.js',html.read_text('utf-8'))
            self.assertTrue((html.parent/'bootstrap.js').is_file())
    def test_reject_zip_traversal_and_duplicate_manifest(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'bad.arschar';sample_bundle(p,'../outside')
            with self.assertRaises(ValueError):load_model(p)
            q=Path(t)/'dup.arschar';sample_bundle(q)
            with zipfile.ZipFile(q,'a') as z:z.writestr('character.json','{}')
            with self.assertRaises(ValueError):load_model(q)
    def test_reject_3d_only(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'three.arschar'
            with zipfile.ZipFile(p,'w') as z:z.writestr('character.json',json.dumps({'schema_version':'0.1.0','representations':{'3d':{}}}))
            with self.assertRaises(ValueError):load_model(p)

if __name__=='__main__':unittest.main()
