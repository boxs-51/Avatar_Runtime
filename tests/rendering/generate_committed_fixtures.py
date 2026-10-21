"""Generated CC0 controls for #39; keeps the historical corpus byte-for-byte."""
import argparse
import copy
import io
import json
import math
from pathlib import Path
import struct
import zipfile
import zlib


def encoded(value):
    return json.dumps(value, separators=(',', ':'), allow_nan=False).encode()


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def glb(document, binary):
    document = copy.deepcopy(document)
    document['buffers'][0]['byteLength'] = len(binary)
    text = encoded(document)
    text += b' ' * (-len(text) % 4)
    binary += b'\0' * (-len(binary) % 4)
    return (struct.pack('<III', 0x46546c67, 2, 28 + len(text) + len(binary)) +
            struct.pack('<II', len(text), 0x4e4f534a) + text +
            struct.pack('<II', len(binary), 0x004e4942) + binary)


def generate(corpus, output):
    output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(corpus / 'positive-dual-full.arschar') as archive:
        manifest = json.loads(archive.read('character.json'))
        rig = json.loads(archive.read('assets/character2d.ars2d.json'))
    payload = (corpus / 'positive-m2-3d-pose.glb').read_bytes()
    text_length = struct.unpack_from('<I', payload, 12)[0]
    document = json.loads(payload[20:20 + text_length])
    binary_offset = 20 + text_length
    binary = payload[binary_offset + 8:]
    document['meshes'][0]['weights'] = [0.25]
    document['nodes'][1]['name'] = 'Head'
    document['nodes'][1]['rotation'] = [0, 0, math.sin(0.15), math.cos(0.15)]
    document['nodes'][2]['name'] = 'Jaw'
    document['nodes'].append({'name': 'Camera', 'camera': 0, 'translation': [0, 0, 5]})
    document['scenes'][0]['nodes'].append(3)
    document['cameras'] = [{'type': 'orthographic', 'orthographic':
                          {'xmag': 2, 'ymag': 2, 'znear': 0.01, 'zfar': 100}}]
    for semantic in manifest['rig_semantics']:
        if semantic['id'] == 'mouth.open':
            semantic['default'] = 0.25
        if semantic['id'] == 'head.yaw_rad':
            semantic['default'] = 0.2
    extra = [
        ({'id': 'head.pitch', 'type': 'float', 'units': 'radians', 'default': -0.1}, 'bone:Head.pitch'),
        ({'id': 'head.roll', 'type': 'float', 'units': 'radians', 'default': 0.15}, 'bone:Head.roll'),
        ({'id': 'head.position', 'type': 'vec3', 'units': 'meters', 'default': [0.4, 0.1, 0]}, 'bone:Head.translation'),
        ({'id': 'head.scale', 'type': 'vec3', 'units': 'unitless', 'default': [1, 1, 1]}, 'bone:Head.scale'),
        ({'id': 'camera.position', 'type': 'vec3', 'units': 'meters', 'default': [0, 0, 0]}, 'bone:Camera.translation'),
    ]
    for semantic, target in extra:
        manifest['rig_semantics'].append(semantic)
        manifest['representations']['3d']['mappings'].append({'semantic': semantic['id'], 'target': target})
    rig['canvas'] = {'width': 64, 'height': 64}
    for parameter in rig['parameters']:
        if parameter['id'] == 'MouthOpen':
            parameter['default'] = 0.25
    for i, part in enumerate(rig['parts']):
        part['atlas_rect_px'] = [i, 0, 1, 1]
        part['vertices_px'] = [[8, 8], [40, 8], [40, 40], [8, 40]]
        part['uv'] = [[0, 0], [1, 0], [1, 1], [0, 1]]
        part['triangles'] = [[0, 1, 2], [0, 2, 3]]
        for deform in part.get('deform_targets', []):
            deform['vertex_deltas_px'] = [[16, 0]] * 4
    raw = b'\0' + bytes([255, 0, 0, 255, 0, 255, 0, 255])
    atlas = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 1, 8, 6, 0, 0, 0)) +
             chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

    def save(name, metadata, source_document=None, source_binary=None):
        data = io.BytesIO()
        with zipfile.ZipFile(data, 'w') as archive:
            for path, content in [('character.json', encoded(metadata)),
                                  ('assets/character2d.ars2d.json', encoded(rig)),
                                  ('assets/atlas.png', atlas), ('assets/triangle.glb', glb(
                                      document if source_document is None else source_document,
                                      binary if source_binary is None else source_binary))]:
                info = zipfile.ZipInfo(path, (1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                archive.writestr(info, content)
        (output / name).write_bytes(data.getvalue())

    save('committed-euler.arschar', manifest)
    quaternion = copy.deepcopy(manifest)
    euler_ids = {'head.yaw_rad', 'head.pitch', 'head.roll'}
    quaternion['rig_semantics'] = [s for s in quaternion['rig_semantics'] if s['id'] not in euler_ids]
    quaternion['representations']['3d']['mappings'] = [m for m in quaternion['representations']['3d']['mappings']
                                                       if m['semantic'] not in euler_ids]
    quaternion['rig_semantics'].append({'id': 'head.rotation', 'type': 'quat', 'units': 'unitless',
                                       'default': [math.sin(0.1), 0, 0, math.cos(0.1)]})
    quaternion['representations']['3d']['mappings'].append({'semantic': 'head.rotation', 'target': 'bone:Head.rotation'})
    save('committed-quaternion.arschar', quaternion)
    hierarchy = copy.deepcopy(manifest)
    hierarchy['visual_layers'][1]['parent'] = 'base.body'
    save('committed-hierarchy.arschar', hierarchy)
    # Independent fractional-LOD controls: one red texel in a4x4 black image.
    # Point samples at uv(.125,.125) yield linear red1,.25,.0625 at mip0/1/2.
    # Full-screen geometry and exact UV slope nominate a known fractional LOD.
    mip_manifest = copy.deepcopy(manifest)
    mip_manifest['default_mode'] = '3d'
    mip_manifest['representations']['3d']['mappings'] = [
        m for m in mip_manifest['representations']['3d']['mappings']
        if m['semantic'] in ('mouth.open', 'camera.position')]
    pixels = b''.join(b'\0' + b''.join(bytes([255 if x == 0 and y == 0 else 0, 0, 0, 255])
                                      for x in range(4)) for y in range(4))
    image = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 4, 4, 8, 6, 0, 0, 0)) +
             chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))
    for lod in (0.25, 0.75, 1.25, 1.75):
        for mode in (9728, 9984, 9986):
            d = {'asset': {'version': '2.0'}, 'buffers': [{'byteLength': 0}],
                 'bufferViews': [], 'accessors': [], 'nodes': [{'mesh': 0}, {'name': 'Camera', 'camera': 0, 'translation': [0, 0, 5]}],
                 'scenes': [{'nodes': [0, 1]}], 'scene': 0, 'cameras': document['cameras'],
                 'textures': [{'source': 0, 'sampler': 0}],
                 'samplers': [{'magFilter': 9728, 'minFilter': mode, 'wrapS': 10497, 'wrapT': 10497}],
                 'materials': [{'doubleSided': True, 'pbrMetallicRoughness':
                               {'baseColorTexture': {'index': 0}, 'metallicFactor': 0, 'roughnessFactor': 1}}]}
            blob = bytearray()

            def view(content):
                blob.extend(b'\0' * (-len(blob) % 4))
                d['bufferViews'].append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(content)})
                blob.extend(content)
                return len(d['bufferViews']) - 1

            def accessor(values, shape, minimum=None, maximum=None):
                flat = [v for row in values for v in row]
                a = {'bufferView': view(struct.pack('<' + 'f' * len(flat), *flat)),
                     'componentType': 5126, 'count': len(values), 'type': shape}
                if minimum is not None:
                    a.update(min=minimum, max=maximum)
                d['accessors'].append(a)
                return len(d['accessors']) - 1

            positions = accessor([[-2, -2, 0], [2, -2, 0], [2, 2, 0], [-2, 2, 0]], 'VEC3', [-2, -2, 0], [2, 2, 0])
            slope = 16 * 2 ** lod
            start = 0.125 - 32.5 / 64 * slope
            uv = accessor([[start, start + slope], [start + slope, start + slope],
                           [start + slope, start], [start, start]], 'VEC2')
            morph = accessor([[0, 0, 0]] * 4, 'VEC3', [0, 0, 0], [0, 0, 0])
            indices = len(d['accessors'])
            d['accessors'].append({'bufferView': view(struct.pack('<6H', 0, 1, 2, 0, 2, 3)),
                                   'componentType': 5123, 'count': 6, 'type': 'SCALAR'})
            d['meshes'] = [{'primitives': [{'attributes': {'POSITION': positions, 'TEXCOORD_0': uv},
                                          'indices': indices, 'targets': [{'POSITION': morph}], 'material': 0}],
                            'weights': [0.25], 'extras': {'targetNames': ['MouthOpen']}}]
            d['images'] = [{'bufferView': view(image), 'mimeType': 'image/png'}]
            save(f'committed-lod-{int(lod*100)}-{mode}.arschar', mip_manifest, d, bytes(blob))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('corpus', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    generate(args.corpus, args.output)
