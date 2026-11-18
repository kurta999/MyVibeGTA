"""Cook CC0 Poly Haven grass into indexed DX11 foliage with shared 2K PBR maps.

Run fetch_grass.ps1 first. The cheaper meshes keep complete blades from the
detail mesh, using the same UVs, normals, and PBR material at every distance.
Use --lod-only to update the LODs from the cooked detail meshes and maps.
"""
import argparse
import json
import struct
from pathlib import Path
import numpy as np
from tree_geometry import leaf_components

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/models/source/polyhaven/grass_medium_01'
OUT = ROOT / 'assets/models/baked/nature'


def write_mesh(name, vertices, indices, texture):
    vertices = np.asarray(vertices, dtype='<f4').reshape((-1, 12))
    indices = np.asarray(indices, dtype='<u4').reshape(-1)
    assert np.isfinite(vertices).all() and len(indices) % 3 == 0
    assert indices.max() < len(vertices)
    path = OUT / name
    with path.with_suffix('.m3d').open('wb') as stream:
        stream.write(struct.pack('<4sII', b'M3D2', len(vertices), len(indices)))
        stream.write(vertices.tobytes()); stream.write(indices.tobytes())
    path.with_suffix('.pbr').write_text(
        f'0 {len(indices)} 0.88 0 0 {texture}.png '
        'grass-normal.jpg grass-orm.jpg - - MASK 0.42\n')
    return {'name': name, 'vertices': len(vertices), 'triangles': len(indices)//3}


def blade_mesh(doc, binary, node):
    from build_city_models import accessor

    primitive = doc['meshes'][node['mesh']]['primitives'][0]
    attributes = primitive['attributes']
    positions = accessor(doc, binary, attributes['POSITION']).astype(np.float32).copy()
    positions -= np.array([(positions[:,0].min()+positions[:,0].max())/2,
                          positions[:,1].min(),
                          (positions[:,2].min()+positions[:,2].max())/2])
    positions /= positions[:,1].max()
    normals = accessor(doc, binary, attributes['NORMAL']).copy()
    uv = accessor(doc, binary, attributes['TEXCOORD_0']).copy()
    # Convert right-handed glTF to the runtime's left-handed coordinates.
    positions[:,2] *= -1; normals[:,2] *= -1
    indices = accessor(doc, binary, primitive['indices']).reshape((-1,3))[:,[0,2,1]]
    colors = np.ones((len(positions),4), np.float32)
    colors[:,:3] *= (0.72+positions[:,1,None]*0.28)
    colors[:,3] = positions[:,1]**2  # rooted wind weight; alpha is in the texture
    return np.concatenate((positions,normals,uv,colors),axis=1),indices.reshape(-1)


def read_baked_mesh(name):
    data = (OUT / f'{name}.m3d').read_bytes()
    magic, vertex_count, index_count = struct.unpack_from('<4sII', data)
    assert magic == b'M3D2'
    vertices = np.frombuffer(data, '<f4', vertex_count * 12, 12).reshape((-1, 12))
    indices = np.frombuffer(data, '<u4', index_count, 12 + vertex_count * 48)
    return vertices, indices


def blade_lods(vertices, indices):
    triangles = indices.reshape((-1, 3))
    labels = leaf_components(triangles, len(vertices))
    groups, inverse, counts = np.unique(labels, return_inverse=True, return_counts=True)
    positions = vertices[:, :3]
    corners = positions[triangles]
    areas = np.linalg.norm(np.cross(corners[:, 1] - corners[:, 0],
                                    corners[:, 2] - corners[:, 0]), axis=1)
    group_areas = np.bincount(inverse, weights=areas)
    order = np.argsort(-group_areas / counts, kind='stable')
    # Unreferenced extrema preserve the detail mesh's instance scale and origin.
    bounds = np.r_[positions.argmin(axis=0), positions.argmax(axis=0)]
    keep = []
    used = 0
    for fraction in (.11, .23):
        budget = int(len(triangles) * fraction)
        for index in order:
            if index not in keep and used + counts[index] <= budget:
                keep.append(index)
                used += counts[index]
        selected = triangles[np.isin(labels, groups[keep])]
        retained, remap = np.unique(np.r_[selected.ravel(), bounds], return_inverse=True)
        yield vertices[retained], remap[:selected.size]


def bake_textures():
    from PIL import Image

    tex=SOURCE/'textures'
    source_alpha=Image.open(tex/'grass_medium_01_alpha_2k.png')
    # Poly Haven supplies a 16-bit grayscale mask. Pillow's convert('L') clamps
    # values instead of scaling them, destroying fine strand/edge coverage.
    alpha=Image.fromarray((np.asarray(source_alpha,dtype=np.float32)/257).clip(0,255).astype(np.uint8))
    assert alpha.size==(2048,2048)
    colors={}
    for name,source in [('green','diff'),('dry','dry_diff')]:
        image=Image.open(tex/f'grass_medium_01_{source}_2k.jpg').convert('RGBA')
        image.putalpha(alpha);colors[name]=image
        image.save(OUT/f'grass-{name}.png',optimize=True)
    # Frost stays on the same stems, with brighter tips and muted dormant roots.
    frost=np.asarray(colors['dry']).copy();rgb=frost[:,:,:3].astype(np.float32)
    frost[:,:,:3]=np.clip(rgb*.36+np.array([155,170,177]),0,255).astype(np.uint8)
    Image.fromarray(frost).save(OUT/'grass-frost.png',optimize=True)
    for source,target in [('nor_dx','normal'),('arm','orm')]:
        (OUT/f'grass-{target}.jpg').write_bytes((tex/f'grass_medium_01_{source}_2k.jpg').read_bytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lod-only', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    if not args.lod_only:
        from build_city_models import read_scene

        bake_textures()
        doc, binary = read_scene(SOURCE/'grass_medium_01.gltf')
        nodes = {node['name']: node for node in doc['nodes']}
    variants=[('lawn','small_a','green'),('meadow','small_b','green'),
              ('savanna','tall_a','dry'),('desert','tall_b','dry'),
              ('snow','tall_c','frost'),('coastal','small_b','dry')]
    meshes=[]
    for name,node,color in variants:
        mesh = f'grass_{name}'
        if args.lod_only:
            verts, indices = read_baked_mesh(mesh)
            meshes.append({'name': mesh, 'vertices': len(verts), 'triangles': len(indices)//3})
        else:
            verts, indices = blade_mesh(doc, binary, nodes[f'grass_medium_01_{node}_LOD0'])
            meshes.append(write_mesh(mesh, verts, indices, f'grass-{color}'))
        far, middle = blade_lods(verts, indices)
        for level, (lod_verts, lod_indices) in enumerate((middle, far), 1):
            meshes.append(write_mesh(f'{mesh}-lod{level}', lod_verts, lod_indices, f'grass-{color}'))
    (OUT/'GRASS_MANIFEST.json').write_text(json.dumps({
        'source':'https://polyhaven.com/a/grass_medium_01','license':'CC0-1.0',
        'texture_size':[2048,2048],'meshes':meshes},indent=2)+'\n')
    print(f'Cooked {len(meshes)} indexed foliage meshes with shared 2048x2048 maps')


if __name__=='__main__':main()
