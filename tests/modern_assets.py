"""Validate original cooked geometry and self-contained editable GLB sources."""
import hashlib
import json
import math
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/"assets/models/baked/modern"
manifest=json.loads((folder/"manifest.json").read_text())
materials=json.loads((folder/'materials.json').read_text())
textures=json.loads((folder/'textures.json').read_text())
source_maps={entry['file']:entry['sha256'] for m in materials['materials'] for entry in m['maps'].values()}
assert materials['size']==2048 and len(materials['materials'])==18
assert len(textures['textures'])==54 and textures['coherence_channel']=='ORM alpha'
for material in materials['materials']:
    for channel,item in material['maps'].items():
        data=(folder/item['file']).read_bytes()
        assert data[:8]==b'\x89PNG\r\n\x1a\n'
        assert struct.unpack_from('>II',data,16)==(2048,2048)
        assert hashlib.sha256(data).hexdigest()==item['sha256']
for item in textures['textures']:
    data=(folder/item['file']).read_bytes();h=struct.unpack_from('<37I',data)
    assert len(data)==item['bytes'] and hashlib.sha256(data).hexdigest()==item['sha256']
    assert h[0]==0x20534444 and h[4]==h[3]==2048 and h[7]==12
    channel=item['file'].rsplit('-',1)[1].split('.')[0]
    assert h[32]=={'base':99,'normal':83,'orm':98}[channel]
    assert len(data)==148+sum(max(1,(2048//2**m+3)//4)**2*16 for m in range(12))
    source=(folder/item['file'].replace('.dds','.png')).read_bytes()
    assert hashlib.sha256(source).hexdigest()==item['source_sha256']
total=0
for item in manifest["assets"]:
    data=(ROOT/"assets/models/baked"/(item["name"]+".m3d")).read_bytes()
    magic,nv,ni=struct.unpack_from("<4sII",data)
    assert magic==b"M3D2" and ni%3==0 and len(data)==12+nv*48+ni*4
    assert hashlib.sha256(data).hexdigest()==item["sha256"]
    vertices=list(struct.iter_unpack("<12f",data[12:12+nv*48]))
    indices=struct.unpack_from("<"+"I"*ni,data,12+nv*48)
    assert max(indices)<nv
    for v in vertices:
        assert all(math.isfinite(x) for x in v)
        assert abs(sum(x*x for x in v[3:6])-1)<1e-5
    for offset in range(0,ni,3):
        a,b,c=(vertices[indices[offset+i]] for i in range(3))
        ab=[b[i]-a[i] for i in range(3)];ac=[c[i]-a[i] for i in range(3)]
        n=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]]
        assert sum(x*x for x in n)>1e-16,(item["name"],offset,"degenerate")
        assert sum(n[i]*(a[i+3]+b[i+3]+c[i+3]) for i in range(3))>0,(item["name"],offset,"winding")
    total+=ni//3

sources=list((ROOT/"assets/models/source/modern").glob("*.glb"))
assert len(sources)==12
for source in sources:
    data=source.read_bytes();magic,version,length=struct.unpack_from("<4sII",data)
    assert magic==b"glTF" and version==2 and length==len(data)
    size,tag=struct.unpack_from("<I4s",data,12);assert tag==b"JSON"
    doc=json.loads(data[20:20+size]);bin_size,tag=struct.unpack_from("<I4s",data,20+size)
    assert tag==b"BIN\0" and doc["buffers"][0]["byteLength"]<=bin_size
    for view in doc["bufferViews"]:
        assert view["byteOffset"]%4==0 and view["byteOffset"]+view["byteLength"]<=bin_size
    for image in doc["images"]:
        assert image["mimeType"]=="image/png" and "uri" not in image
        view=doc['bufferViews'][image['bufferView']]
        start=28+size+view['byteOffset']
        assert hashlib.sha256(data[start:start+view['byteLength']]).hexdigest()==source_maps[image['name']+'.png']
    for primitive in doc["meshes"][0]["primitives"]:
        assert {"POSITION","NORMAL","TEXCOORD_0"}<=primitive["attributes"].keys()
        assert 0<=primitive["material"]<len(doc["materials"])
    assert len(doc["materials"])==len(doc["meshes"][0]["primitives"])
    for material in doc["materials"]:
        assert material['occlusionTexture']['index']==material['pbrMetallicRoughness']['metallicRoughnessTexture']['index']
        extensions=material.get("extensions",{})
        assert all(name in doc.get("extensionsUsed",[]) for name in extensions)
        if material["name"] in ("blue_paint","pearl_paint"):
            coat=extensions["KHR_materials_clearcoat"]
            assert coat["clearcoatFactor"]==1 and 0<coat["clearcoatRoughnessFactor"]<.2
        if material["name"] in ("glass","car_glass"):
            assert extensions["KHR_materials_ior"]["ior"]==1.5
print(f"Validated {len(manifest['assets'])} meshes, {total} triangles, {len(sources)} embedded GLBs, 54 original 2K maps and 54 complete compressed DDS chains")
