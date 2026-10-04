"""Cook CC0 Poly Haven grass into indexed DX11 foliage with shared 2K PBR maps.

Run fetch_grass.ps1 first. Keeps the source's UVs and curved blade geometry;
the two cheaper LODs sample the source atlas's photographed clump cutouts.
"""
import json
import math
import struct
from pathlib import Path
import numpy as np
from PIL import Image
from build_city_models import read_scene, accessor

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/models/source/polyhaven/grass_medium_01'
OUT = ROOT / 'assets/models/baked/nature'


def write_mesh(name, vertices, indices, texture, normal=True):
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
        f'{"grass-normal.jpg" if normal else "-"} grass-orm.jpg - - MASK 0.42\n')
    return {'name': name, 'vertices': len(vertices), 'triangles': len(indices)//3}


def blade_mesh(doc, binary, node):
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


def cards(count, tall=False):
    # Source atlas has pre-rendered clumps in its lower strip. Use their exact
    # cutouts rather than covering an entire atlas with one repeated billboard.
    uv = (0.20,0.754,0.485,0.908) if not tall else (0.58,0.748,0.805,0.866)
    vertices=[];indices=[]
    for card in range(count):
        angle=math.pi*card/count
        co,si=math.cos(angle),math.sin(angle)
        for height in (0,0.5,1):
            for side in (-1,1):
                x=side*0.62; bend=0.06*height*height
                vertices.append((x*co+bend,height,x*si,
                    -si*.65,.76,co*.65,
                    uv[0] if side<0 else uv[2],uv[3]+(uv[1]-uv[3])*height,
                    .72+.28*height,.72+.28*height,.72+.28*height,height*height))
        base=card*6
        for row in range(2):
            i=base+row*2;indices.extend((i,i+1,i+3,i,i+3,i+2))
    return vertices,indices


def main():
    OUT.mkdir(parents=True,exist_ok=True)
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
    doc,binary=read_scene(SOURCE/'grass_medium_01.gltf')
    nodes={node['name']:node for node in doc['nodes']}
    variants=[('lawn','small_a','green'),('meadow','small_b','green'),
              ('savanna','tall_a','dry'),('desert','tall_b','dry'),
              ('snow','tall_c','frost'),('coastal','small_b','dry')]
    meshes=[]
    for name,node,color in variants:
        verts,indices=blade_mesh(doc,binary,nodes[f'grass_medium_01_{node}_LOD0'])
        meshes.append(write_mesh(f'grass_{name}',verts,indices,f'grass-{color}'))
        for level,count in [(1,3),(2,2)]:
            verts,indices=cards(count,name in ('savanna','desert','snow'))
            meshes.append(write_mesh(f'grass_{name}-lod{level}',verts,indices,f'grass-{color}',False))
    (OUT/'GRASS_MANIFEST.json').write_text(json.dumps({
        'source':'https://polyhaven.com/a/grass_medium_01','license':'CC0-1.0',
        'texture_size':[2048,2048],'meshes':meshes},indent=2)+'\n')
    print(f'Cooked {len(meshes)} indexed foliage meshes with shared 2048x2048 maps')


if __name__=='__main__':main()
