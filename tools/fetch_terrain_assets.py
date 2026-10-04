"""Fetch pinned CC0 Poly Haven rocks and terrain maps; bake with --bake.

Source files and SHA-256 provenance are retained. The runtime needs no network.
"""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/models/source/terrain'
BAKED = ROOT / 'assets/models/baked/nature'
MODELS = ('namaqualand_boulder_02', 'coastal_cliff_02')

def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'MiniCity3D terrain importer'}), timeout=180) as response:
        return response.read()

def fetch():
    SOURCE.mkdir(parents=True, exist_ok=True)
    manifest_path = SOURCE / 'manifest.json'
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else []
    if not manifest:
        for asset in (*MODELS, 'rocky_terrain'):
            listing = json.loads(get('https://api.polyhaven.com/files/' + asset))
            files = []
            if asset in MODELS:
                main = listing['gltf']['1k']['gltf']
                files.append((asset + '.gltf', main))
                files.extend(main['include'].items())
            else:
                for kind in ('Diffuse', 'nor_dx', 'Rough'):
                    entry = listing[kind]['1k']['jpg']
                    files.append((entry['url'].rsplit('/', 1)[-1], entry))
            manifest.append(dict(id=asset, page='https://polyhaven.com/a/' + asset,
                license='CC0-1.0', license_url='https://polyhaven.com/license',
                files=[dict(path=name, url=item['url'], md5=item['md5']) for name, item in files]))
    for asset in manifest:
        for entry in asset['files']:
            path = SOURCE / asset['id'] / entry['path']
            path.parent.mkdir(parents=True, exist_ok=True)
            data = path.read_bytes() if path.exists() else get(entry['url'])
            if hashlib.md5(data).hexdigest() != entry['md5']:
                raise ValueError('Source checksum mismatch: ' + str(path))
            sha = hashlib.sha256(data).hexdigest()
            if entry.get('sha256', sha) != sha:
                raise ValueError('Pinned source changed: ' + str(path))
            if not path.exists(): path.write_bytes(data)
            entry['sha256'] = sha
        print('Verified', asset['id'])
    manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')

def bake():
    import numpy as np
    from PIL import Image
    from build_city_models import read_scene, accessor, node_matrix
    import struct
    import shutil
    BAKED.mkdir(parents=True, exist_ok=True)
    materials = ROOT / 'assets/materials/terrain'
    materials.mkdir(parents=True, exist_ok=True)
    for path in (SOURCE / 'rocky_terrain').glob('*.jpg'):
        shutil.copyfile(path, materials / path.name)
    shutil.copyfile(ROOT / 'assets/models/source/CC0-1.0.txt', materials / 'CC0-1.0.txt')
    for asset in MODELS:
        path = SOURCE / asset / (asset + '.gltf')
        doc, binary = read_scene(path)
        parts = []
        def visit(index, parent):
            node = doc['nodes'][index]
            matrix = parent @ node_matrix(node)
            if 'mesh' in node:
                for primitive in doc['meshes'][node['mesh']]['primitives']:
                    attrs = primitive['attributes']
                    pos = accessor(doc, binary, attrs['POSITION']) @ matrix[:3, :3].T + matrix[:3, 3]
                    uv = accessor(doc, binary, attrs['TEXCOORD_0']).copy()
                    normals = accessor(doc, binary, attrs['NORMAL']) @ np.linalg.inv(matrix[:3, :3])
                    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-8)
                    indices = accessor(doc, binary, primitive['indices']).reshape(-1).astype(np.uint32)
                    parts.append((pos, normals, uv, indices))
            for child in node.get('children', []): visit(child, matrix)
        for node in doc['scenes'][doc.get('scene', 0)]['nodes']: visit(node, np.eye(4))
        if len(parts) != 1: raise ValueError('Expected single scanned rock mesh')
        pos, normals, uv, indices = parts[0]
        uv[:, 1] = 1 - uv[:, 1]
        # Weld nearby vertices by position AND UV to preserve the scan texture
        # and seams; collapsed triangles are removed, never randomly dropped.
        extent = np.ptp(pos, axis=0).max()
        for suffix, resolution in (('', 100), ('-lod', 32)):
            keys = np.column_stack((np.round(pos / (extent / resolution)), np.round(uv * resolution))).astype(np.int64)
            _, first, inverse = np.unique(keys, axis=0, return_index=True, return_inverse=True)
            tri = inverse[indices].reshape(-1, 3)
            tri = tri[(tri[:, 0] != tri[:, 1]) & (tri[:, 0] != tri[:, 2]) & (tri[:, 1] != tri[:, 2])]
            selected = np.unique(tri)
            remap = np.zeros(len(first), dtype=np.uint32)
            remap[selected] = np.arange(len(selected))
            chosen = first[selected]
            packed = np.column_stack((pos[chosen], normals[chosen], uv[chosen], np.ones((len(chosen), 4)))).astype('<f4')
            triangles = remap[tri].astype('<u4')
            target = BAKED / ('rock_' + asset + suffix)
            target.with_suffix('.m3d').write_bytes(b'M3D2' + struct.pack('<II', len(packed), triangles.size) + packed.tobytes() + triangles.tobytes())
            diffuse = SOURCE / asset / 'textures' / (asset + '_diff_1k.jpg')
            Image.open(diffuse).save(target.with_suffix('.png'))
            print(target.name, len(packed), len(tri), 'triangles')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--fetch', action='store_true')
    parser.add_argument('--bake', action='store_true')
    args = parser.parse_args()
    if args.fetch: fetch()
    if args.bake: bake()
