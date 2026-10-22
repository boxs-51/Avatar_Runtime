"""Opt-in external-viewer diagnostics; never F2/F3 acceptance evidence."""
from pathlib import Path
import json
import os
import subprocess
import sys
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]
VIEWER = ROOT / 'build/moc3-render/viewer/Release/viewer.exe'

def profile():
    name = os.environ.get('MOC3_VIEWER_PROFILE')
    if not name:
        return {}
    path = Path(name).resolve()
    if not any(path.is_relative_to((ROOT / base).resolve()) for base in ('build', 'local-assets')):
        raise ValueError('Diagnostic profile must remain in ignored workspace storage')
    value = json.loads(path.read_text(encoding='utf-8'))
    model = (ROOT / value['model']).resolve()
    if not model.is_relative_to((ROOT / 'local-assets').resolve()) or not model.is_file():
        raise ValueError('Invalid local model profile')
    value['model'] = str(model)
    return value

def ready(config, numpy, image):
    if not config or numpy is None or image is None or not VIEWER.is_file():
        raise unittest.SkipTest('Opt-in private viewer/dependencies/profile unavailable')

def output_root(label):
    return ROOT / 'build/viewer-diagnostics' / (label + '-' + uuid.uuid4().hex)

def render(output, model, params, flags=()):
    output = Path(output).resolve()
    if not output.is_relative_to((ROOT / 'build/viewer-diagnostics').resolve()):
        raise ValueError('Invalid diagnostic output root')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)
    command = [str(VIEWER), '--shot', str(output.relative_to(ROOT)), *flags]
    for key, value in dict(params).items():
        command.extend(('--setparam', key, str(value)))
    command.append(os.path.relpath(model, ROOT))
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=30)
    if result.returncode != 0 or not output.is_file():
        raise RuntimeError('Diagnostic renderer failed; no fresh evidence')

if __name__ == '__main__':
    config = profile()
    if not config or not VIEWER.is_file():
        raise SystemExit('Set MOC3_VIEWER_PROFILE to an ignored local profile first')
    raise SystemExit(subprocess.call([str(VIEWER), os.path.relpath(config['model'], ROOT)], cwd=ROOT))
