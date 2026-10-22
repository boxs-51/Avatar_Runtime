"""Authorized local-only fixture helper; never returns data to public artifacts."""
import io,json
from pathlib import Path
from moc3_local_replay import _private_manifest
from moc3_admission_fixtures import archive

def private_fixture(folder):
    from PIL import Image
    manifest=_private_manifest()
    if manifest is None:raise RuntimeError('AUTHORIZED_LOCAL_FIXTURE_UNAVAILABLE')
    files={manifest.name:manifest.read_bytes()};refs=json.loads(manifest.read_bytes())['FileReferences']
    for name in (refs['Moc'],refs['Physics']):files[name]=(manifest.parent/name).read_bytes()
    for name in refs['Textures']:
        with Image.open(manifest.parent/name) as image:
            image=image.convert('RGBA');image.thumbnail((512,512),Image.Resampling.LANCZOS);out=io.BytesIO();image.save(out,format='PNG');files[name]=out.getvalue()
    folder=Path(folder);folder.mkdir(parents=True,exist_ok=True)
    for name,data in files.items():
        target=folder/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    return archive(files),manifest.name,folder
