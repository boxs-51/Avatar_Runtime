"""Build a self-contained experimental app, optionally preloaded from native2D .arschar.
Usage: python run_studio.py --model character.arschar --out preview_app
Default launches the local demo app. No network or repository mutation.
"""
import argparse
import json
import shutil
import webbrowser
from pathlib import Path

from model_to_studio import load_model

HOME = Path(__file__).resolve().parent

def package(output, model=None):
    output=Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    for f in ('index.html','physics_core.js','studio_app.js'):
        shutil.copy2(HOME/f,output/f)
    html=(output/'index.html').read_text('utf-8')
    if model:
        doc=load_model(model)
        # Escaped assignment: user asset metadata cannot break script context.
        encoded=json.dumps(doc,ensure_ascii=True,separators=(',',':')).replace('<','\\u003c').replace('>','\\u003e')
        (output/'bootstrap.js').write_text('window.STUDIO_BOOTSTRAP='+encoded+';\n',encoding='utf-8')
        html=html.replace('<script src="./physics_core.js"></script>', '<script src="./bootstrap.js"></script><script src="./physics_core.js"></script>')
        (output/'native2d_scene_v03.json').write_text(json.dumps(doc,ensure_ascii=False,indent=2),encoding='utf-8')
    (output/'index.html').write_text(html,encoding='utf-8')
    return output/'index.html'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--model',type=Path,help='Local .arschar native2D bundle')
    ap.add_argument('--out',type=Path,default=Path('layer_physics_preview_app'))
    ap.add_argument('--no-open',action='store_true')
    args=ap.parse_args()
    app=package(args.out,args.model)
    print('Layer Physics Studio v0.3 app:',app)
    print('Preview supports native2D sprites + authoring; NOT MOC3/GLB or full native renderer equivalence')
    if not args.no_open: webbrowser.open(app.as_uri())
if __name__=='__main__': main()
