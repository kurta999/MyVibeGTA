"""Record cooked hashes, sizes, and exposed wood edges for the tree correction."""
import hashlib
import json
import pathlib
import struct

import numpy as np

from build_city_models import TREE_IDS, tree_source, read_scene

ROOT = pathlib.Path(__file__).resolve().parents[1]
records = []
for name in TREE_IDS:
    source = tree_source(name)[0]
    doc, _ = read_scene(source)
    materials = doc.get('materials', [{}])
    cols = 2 if len(materials) <= 4 else 4
    rows = (len(materials) + cols - 1) // cols
    wood = [index for index, material in enumerate(materials)
        if not any(part in material.get('name', '').lower()
            for part in ('leaf', 'leav', 'twig', 'foliage', 'flower', 'blossom'))]
    entry = {'model': name, 'source': str(source.relative_to(ROOT)),
        'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(), 'meshes': {}}
    if source.suffix == '.gltf':
        entry['buffers_sha256'] = {buffer['uri']: hashlib.sha256((source.parent / buffer['uri']).read_bytes()).hexdigest()
            for buffer in doc['buffers']}
    for suffix in ['', '-lod']:
        for stage, directory in [('before', ROOT / 'build-tools/trees-before'),
                                 ('after', ROOT / 'assets/models/baked/nature')]:
            path = directory / (name + suffix + '.m3d')
            if stage == 'before' and not path.is_file():
                continue
            raw = path.read_bytes()
            magic, count = struct.unpack_from('<4sI', raw)
            assert magic == b'M3D1' and count % 3 == 0 and len(raw) == 8 + count * 48
            geometry = np.frombuffer(raw, '<f4', offset=8).reshape(-1, 3, 12)
            assert np.isfinite(geometry).all()
            assert np.all((geometry[:, :, 6:8] >= 0) & (geometry[:, :, 6:8] <= 1))
            cells = np.floor(geometry[:, :, 6:8].mean(axis=1) * [cols, rows]).astype(int)
            material = cells[:, 1] * cols + cells[:, 0]
            points = geometry[np.isin(material, wood), :, :3]
            exposed = total = 0
            if len(points):
                _, ids = np.unique(np.round(points.reshape(-1, 3), 5), axis=0, return_inverse=True)
                faces = ids.reshape(-1, 3)
                edges = np.sort(np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1)
                counts = np.unique(edges, axis=0, return_counts=True)[1]
                exposed, total = int(np.count_nonzero(counts == 1)), len(counts)
            entry['meshes'][stage + (suffix or '-near')] = {
                'sha256': hashlib.sha256(raw).hexdigest(), 'bytes': len(raw),
                'triangles': count // 3, 'wood_boundary_edges': exposed,
                'wood_edges': total, 'wood_boundary_fraction': exposed / total if total else 0}
    records.append(entry)
destination = ROOT / 'evidence/trees-20261007/geometry-audit.json'
destination.write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
print(f'Validated {len(records)} trees and their near/distant assets; wrote {destination}')
