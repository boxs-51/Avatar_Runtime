"""Development-only native2D .arschar -> Layer Physics Studio authoring scene.
NOT a substitute for Avatar_Runtime's isolated model validator or production renderer.
Only statically previews model parts as RGBA sprites. 3D/MOC3 unsupported here.
"""
import argparse
import base64
import io
import json
import re
import sys
import zipfile
from pathlib import Path

try:
    from PIL import Image
except ImportError as exc:
    raise SystemExit('Pillow required for native2D adapter: python -m pip install pillow') from exc

ROOT = Path(__file__).parent
BUNDLE_LIMIT = 32 * 1024 * 1024
EXPANDED_LIMIT = 64 * 1024 * 1024
IMAGE_LIMIT = 24 * 1024 * 1024
SAFE_NAME = re.compile(r'^(?:[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)*/)*[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)*$')

def fail(msg):
    raise ValueError(msg)

def load_model(path):
    path = Path(path)
    if not path.is_file() or path.stat().st_size > BUNDLE_LIMIT:
        fail('Bundle missing or exceeds 32 MiB')
    with zipfile.ZipFile(path) as z:
        entries = z.infolist()
        if len(entries) > 128 or not entries or any(e.is_dir() or e.flag_bits & 1 or e.file_size > EXPANDED_LIMIT or not SAFE_NAME.fullmatch(e.filename) or (e.external_attr >> 16) & 0o170000 == 0o120000 for e in entries):
            fail('Untrusted archive entry')
        names = {e.filename for e in entries}
        if len(names) != len(entries) or 'character.json' not in names or sum(e.file_size for e in entries) > EXPANDED_LIMIT:
            fail('Missing manifest, duplicate name or expanded budget')
        def read_json(name):
            if name not in names or z.getinfo(name).file_size > 1024 * 1024:
                fail('JSON path/budget rejected: ' + name)
            return json.loads(z.read(name))
        m = read_json('character.json')
        if m.get('schema_version') != '0.1.0' or '2d' not in m.get('representations', {}):
            fail('Native 2D representation required; GLB-only/MOC3 not supported by this editor')
        rep = m['representations']['2d']
        if rep.get('format') != 'ars2d-rig-v0.1':
            fail('Unsupported 2D format')
        atlas_name, rig_name = rep.get('atlas'), rep.get('asset')
        if not isinstance(atlas_name, str) or not SAFE_NAME.fullmatch(atlas_name) or atlas_name not in names or not isinstance(rig_name, str) or not SAFE_NAME.fullmatch(rig_name):
            fail('Unsafe asset paths')
        rig = read_json(rig_name)
        if rig.get('schema_version') != '0.1.0' or rig.get('coordinate_space') != 'canvas_px':
            fail('Unexpected rig version/coordinate system')
        atlas_bytes = z.read(atlas_name)
        if len(atlas_bytes) > 16 * 1024 * 1024:
            fail('Atlas budget exceeded')
        im = Image.open(io.BytesIO(atlas_bytes))
        im.load()
        if im.width * im.height > 2048 * 2048 or im.width < 1 or im.height < 1:
            fail('Atlas pixel budget')
        atlas = im.convert('RGBA')
        parts = rig.get('parts')
        layers = m.get('visual_layers')
        bindings = rep.get('layer_bindings')
        if not isinstance(parts, list) or not isinstance(layers, list) or not isinstance(bindings, list) or len(parts) > 63 or not parts:
            fail('Unsupported part/layer counts')
        indices = {p.get('id'): p for p in parts if isinstance(p, dict)}
        if len(indices) != len(parts):
            fail('Duplicate or bad part')
        bind = {}
        for x in bindings:
            for pid in x.get('part_ids', []):
                if pid in bind:
                    fail('Part owned more than once')
                bind[pid] = x.get('layer_id')
        if set(bind) != set(indices):
            fail('Incomplete layer bindings')
        vis = {l['id']: l for l in layers}
        base = json.loads((ROOT / 'example_authoring_v03.json').read_text('utf-8'))
        base['layers'] = [dict(base['layers'][0], id='model', name=m.get('display_name', 'Imported model')[:100], imageData=None)]
        seen = set(['model'])
        for i, part in enumerate(sorted(parts, key=lambda p: (vis.get(bind[p['id']], {}).get('z_index', 0), p.get('z_index', 0)))):
            rect = part.get('atlas_rect_px', [])
            verts = part.get('vertices_px', [])
            if len(rect) != 4 or not all(type(v) is int for v in rect):
                fail('Bad atlas rect')
            x, y, w, h = rect
            if x < 0 or y < 0 or w <= 0 or h <= 0 or x+w > atlas.width or y+h > atlas.height:
                fail('Atlas rect outside texture')
            if not isinstance(verts, list) or not verts or not all(isinstance(v, list) and len(v) == 2 and all(type(c) in (int,float) and abs(c) < 100000 for c in v) for v in verts):
                fail('Invalid vertices')
            pimg = atlas.crop((x, y, x+w, y+h))
            buff=io.BytesIO();pimg.save(buff, format='PNG')
            image = 'data:image/png;base64,' + base64.b64encode(buff.getvalue()).decode('ascii')
            pid=part['id'];safeid='part'+str(i)
            if safeid in seen:fail('ID collision')
            seen.add(safeid)
            minx=min(v[0] for v in verts);miny=min(v[1] for v in verts)
            entry=dict(base['layers'][0]);entry.update(id=safeid,name=pid[:100],parentId='model',kind='static',
                anchorX=max(-800,min(800,round(minx-rig.get('canvas',{}).get('width',1024)/2))),
                anchorY=max(-800,min(800,round(miny-rig.get('canvas',{}).get('height',1024)/2))),
                imageData=image,imageOffsetX=0,imageOffsetY=0,colliderRadiusPx=0,collisionEnabled=False)
            base['layers'].append(entry)
        if len(json.dumps(base)) > IMAGE_LIMIT:
            fail('Embedded preview exceeds authoring budget')
        base['source']={'type':'native2d-preview','file':path.name,'parts':len(parts),'mode':'static sprites only','warning':'Not validated or rendered by Avatar_Runtime'}
        return base

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('bundle', type=Path)
    a.add_argument('--out', type=Path, default=Path('native2d_scene_v03.json'))
    args=a.parse_args()
    try:
        scene=load_model(args.bundle)
        args.out.write_text(json.dumps(scene, ensure_ascii=False, indent=2), encoding='utf-8')
    except (ValueError, OSError, zipfile.BadZipFile, json.JSONDecodeError) as exc:
        print(f'REJECTED: {exc}', file=sys.stderr)
        raise SystemExit(2)
    print(f'Wrote {args.out} - {len(scene["layers"])-1} visible model parts; open index.html then Import JSON')
if __name__=='__main__': main()
