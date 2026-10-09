"""Topology-preserving tree preparation shared by the baker and regression checks."""
import hashlib
import os
import pathlib
import subprocess

import numpy as np


def leaf_components(tri, vertex_count):
    """Label connected leaf surfaces, including leaves made from many quads."""
    labels = np.arange(vertex_count, dtype=np.int32)
    while True:
        before = labels.copy()
        roots = labels[tri]
        np.minimum.at(labels, tri.ravel(), np.repeat(roots.min(axis=1), 3))
        labels = labels[labels]
        if np.array_equal(before, labels):
            return labels[tri[:, 0]]


def select_leaves(tri, labels, budget, rng):
    """Retain entire leaves; compensate their projected area for reduced density."""
    groups, counts = np.unique(labels, return_counts=True)
    if len(tri) <= budget:
        return tri, labels, 1.0
    order = rng.permutation(len(groups))
    # Never cut a connected surface to meet a triangle budget.
    keep = order[np.cumsum(counts[order]) <= budget]
    if not len(keep):
        keep = order[:1]
    mask = np.isin(labels, groups[keep])
    selected = tri[mask]
    return selected, labels[mask], float(np.sqrt(len(tri) / len(selected)))


def expand_leaves(points, labels, scale):
    _, inverse = np.unique(labels, return_inverse=True)
    centers = np.zeros((inverse.max() + 1, 3), dtype=np.float64)
    counts = np.bincount(inverse)
    np.add.at(centers, inverse, points.reshape(-1, 3, 3).mean(axis=1))
    centers /= counts[:, None]
    centers = np.repeat(centers[inverse], 3, axis=0)
    return centers + (points - centers) * scale


def retain_branches(geometry, labels, budget):
    """Drop complete tiny twigs when disconnected source pieces limit collapse."""
    if len(geometry) // 3 <= budget * 1.2:
        return geometry
    faces = geometry.reshape(-1, 3, 8)
    groups, inverse, counts = np.unique(labels, return_inverse=True, return_counts=True)
    areas = np.zeros(len(groups), dtype=np.float64)
    np.add.at(areas, inverse, np.linalg.norm(np.cross(faces[:, 1, :3] - faces[:, 0, :3],
        faces[:, 2, :3] - faces[:, 0, :3]), axis=1))
    order = np.argsort(-areas, kind='stable')
    keep = order[np.cumsum(counts[order]) <= budget]
    if not len(keep):
        keep = order[:1]  # Never punch holes in a single large connected trunk.
    return faces[np.isin(labels, groups[keep])].reshape(-1, 8)


def decimate_wood(pos, normals, uv, tri, near, lod, root):
    """Cache deterministic Blender edge-collapse output for shared source geometry."""
    if len(tri) <= lod:
        packed = np.concatenate((pos, normals, uv), axis=1)[tri.ravel()]
        return packed, packed
    digest = hashlib.sha256((root / 'tools/decimate_tree_wood.py').read_bytes())
    digest.update((root / 'tools/tree_geometry.py').read_bytes())
    for array in (pos, uv, tri):
        digest.update(array.tobytes())
    digest.update(f'{near},{lod}'.encode())
    cache = root / 'build-tools/tree-wood-cache'
    cache.mkdir(parents=True, exist_ok=True)
    output = cache / (digest.hexdigest() + '.npz')
    if not output.exists():
        blender = pathlib.Path(os.environ.get('TREE_BLENDER',
            root / 'build-tools/blender-4.5.14-windows-x64/blender.exe'))
        if not blender.is_file():
            raise RuntimeError('Set TREE_BLENDER to Blender 4.5+ to bake connected tree wood')
        source = output.with_suffix('.input.npz')
        np.savez(source, pos=pos, uv=uv, tri=tri, near=near, lod=lod)
        subprocess.run([str(blender), '--background', '--factory-startup', '--threads', '4',
            '--python', str(root / 'tools/decimate_tree_wood.py'), '--', str(source), str(output)], check=True)
        source.unlink()
    with np.load(output) as result:
        near_geometry = np.concatenate((pos, normals, uv), axis=1)[tri.ravel()] if len(tri) <= near else result['near']
        return near_geometry, result['lod']
