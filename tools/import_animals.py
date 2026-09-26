"""Bake attributed, textured animal GLBs and eight procedural gait poses.

Source meshes are static. This deliberately does not claim skeletal animation.
The smaller deer is a fawn-derived roe-deer art adaptation (see manifest).
"""
import io
import json
import math
import pathlib
import struct
from PIL import Image, ImageDraw
import convert_assets as gltf

ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/models/source/animals'
OUTPUT=ROOT/'assets/models/baked/animals'

def extract(path):
    d,b=gltf.read_glb(path)
    vertices=[]
    identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
    def visit(index,parent):
        node=d['nodes'][index];m=gltf.multiply(parent,gltf.local_matrix(node))
        if 'mesh' in node:
            for prim in d['meshes'][node['mesh']]['primitives']:
                attr=prim['attributes']
                p=gltf.accessor(d,b,attr['POSITION']);n=gltf.accessor(d,b,attr['NORMAL'])
                uv=gltf.accessor(d,b,attr['TEXCOORD_0'])
                indices=[i[0] for i in gltf.accessor(d,b,prim['indices'])]
                for i in indices:
                    vertices.append((*gltf.transform(m,p[i]),*gltf.transform(m,n[i],True),*uv[i],1,1,1,1))
        for child in node.get('children',[]): visit(child,m)
    for node in d['scenes'][d.get('scene',0)]['nodes']:visit(node,identity)
    img=d['images'][0];v=d['bufferViews'][img['bufferView']];start=v.get('byteOffset',0)
    texture=Image.open(io.BytesIO(b[start:start+v['byteLength']])).convert('RGB')
    return vertices,texture

def write(path,vertices):
    with path.open('wb') as f:
        f.write(struct.pack('<4sI',b'M3D1',len(vertices)))
        for v in vertices:f.write(struct.pack('<12f',*v))

def preview(items):
    sheet=Image.new('RGB',(1000,math.ceil(len(items)/3)*240),(220,228,218));draw=ImageDraw.Draw(sheet)
    for index,(name,vertices,tex) in enumerate(items):
        ox=(index%3)*333+165;oy=(index//3)*240+180
        lo=[min(v[i] for v in vertices) for i in range(3)]
        hi=[max(v[i] for v in vertices) for i in range(3)]
        scale=170/max(hi[i]-lo[i] for i in range(3));center=[(hi[i]+lo[i])/2 for i in range(3)]
        triangles=[]
        for j in range(0,len(vertices),3):
            tri=vertices[j:j+3];points=[]
            for v in tri:
                x,y,z=[(v[k]-center[k])*scale for k in range(3)]
                points.append((ox+x*0.75+z*0.66,oy-y-z*0.2+x*0.18))
            u=sum(v[6] for v in tri)/3;vv=sum(v[7] for v in tri)/3
            color=tex.getpixel((int(u*(tex.width-1))%tex.width,int(vv*(tex.height-1))%tex.height))
            light=0.75+0.25*max(0,sum(v[4] for v in tri)/3)
            triangles.append((sum(v[0]-v[2] for v in tri),points,tuple(int(c*light) for c in color)))
        for _,points,color in sorted(triangles):draw.polygon(points,fill=color)
        draw.text((ox-150,oy-165),name,fill=(10,30,20))
    sheet.save(ROOT/'screenshots/animal-source-sheet.png')

def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    items=[]
    manifest=json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8-sig'))
    for item in manifest:
        name=item['id'];vertices,texture=extract(SOURCE/(name+'.glb'))
        items.append((name,vertices,texture))
        # Google Poly quadrupeds use Z as the long axis; source orientation is
        # preserved here and per-species yaw corrections are recorded below.
        lo=[min(v[i] for v in vertices) for i in range(3)]
        hi=[max(v[i] for v in vertices) for i in range(3)]
        ext=[hi[i]-lo[i] for i in range(3)]
        oriented=[]
        # Normalize all meshes to one canonical box for stable gait/corpse poses.
        for v in vertices:
            q=list(v)
            for axis in range(3):q[axis]=(q[axis]-lo[axis])/ext[axis]-(0 if axis==1 else 0.5)
            oriented.append(q)
        texture.save(OUTPUT/(name+'.png'))
        write(OUTPUT/(name+'.m3d'),oriented)
        for frame in range(8):
            phase=frame*math.tau/8;pose=[]
            for v in oriented:
                q=v.copy();x,y,z=q[:3]
                weight=max(0,1-y/0.43)**1.4
                side=1 if x>=0 else -1;end=1 if z>=0 else -1
                swing=math.sin(phase+(0 if side*end>0 else math.pi))
                q[2]+=weight*swing*0.10
                q[1]+=weight*max(0,math.cos(phase+(0 if side*end>0 else math.pi)))*0.06
                pose.append(q)
            write(OUTPUT/(name+'-walk-'+str(frame)+'.m3d'),pose)
        # Death pose: roll onto the side and preserve dimensions via renderer.
        dead=[]
        for v in oriented:
            q=v.copy();q[0],q[1]=v[1]-0.5,-v[0]+0.5
            q[3],q[4]=v[4],-v[3];dead.append(q)
        write(OUTPUT/(name+'-dead.m3d'),dead)
        item['modifications']='Normalized scale; original texture; eight procedural gait deformations and side-lying death pose.'
        if name=='roe-deer':item['modifications']+=' Fawn source adapted in game proportions as a small roe-deer stand-in; not a taxonomically exact source model.'
        print(name,len(vertices)//3,'triangles',tuple(round(x,2) for x in ext))
    (SOURCE/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    preview(items)
    text='# Forest wildlife model attribution\n\nAll source GLBs and their embedded textures are by **Poly by Google**, licensed under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/). The full legal text is included in source/animals/CC-BY-3.0.txt.\n\n'
    for item in manifest:text+=f"- **{item['id']}**: [{item['title']}]({item['page']}). {item['modifications']}\n"
    (ROOT/'assets/models/ANIMALS.md').write_text(text)

if __name__=='__main__':main()
