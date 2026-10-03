"""Cook the attributed helicopter, separating its authored main rotor blades."""
import pathlib,json,math,struct
import convert_assets as g
from PIL import Image,ImageDraw
ROOT=pathlib.Path(__file__).resolve().parents[1]
SRC=ROOT/'assets/models/source/helicopter';OUT=ROOT/'assets/models/baked/vehicles';OUT.mkdir(parents=True,exist_ok=True)
d,b=g.read_glb(SRC/'helicopter.glb');parts={}
for ni,node in enumerate(d['nodes']):
 verts=[];m=g.local_matrix(node)
 for primitive in d['meshes'][node['mesh']]['primitives']:
  a=primitive['attributes'];pos=g.accessor(d,b,a['POSITION']);norm=g.accessor(d,b,a['NORMAL']);uv=g.accessor(d,b,a['TEXCOORD_0']) if 'TEXCOORD_0' in a else [(0,0)]*len(pos)
  color=d['materials'][primitive['material']]['pbrMetallicRoughness']['baseColorFactor']
  c=[1.055*x**(1/2.4)-.055 if x>.0031308 else 12.92*x for x in color[:3]]
  for idx in g.accessor(d,b,primitive['indices']):
   i=idx[0];p=g.transform(m,pos[i]);n=g.transform(m,norm[i],True)
   verts.append([*p,*n,*uv[i],*c,1])
 parts[ni]=verts
# Main rotor source blades are nodes 1 and 9, pivot (-932.3, 235.9, -172.9).
body=[v for i,vs in parts.items() if i not in (1,9) for v in vs]
lo=[min(v[i] for v in body) for i in range(3)];hi=[max(v[i] for v in body) for i in range(3)]
cx=(lo[0]+hi[0])/2;cz=(lo[2]+hi[2])/2;scale=.12
for name,verts,center in [('helicopter',body,(cx,lo[1],cz)),('helicopter-rotor',parts[1]+parts[9],(-932.3,233.83,-172.9))]:
 cooked=[]
 for v in verts:
  x,y,z=[(v[i]-center[i])*scale for i in range(3)]
  cooked.append([-x,y,-z,-v[3],v[4],-v[5],*v[6:]])
 with (OUT/(name+'.m3d')).open('wb') as f:
  f.write(struct.pack('<4sI',b'M3D1',len(cooked)))
  for v in cooked:f.write(struct.pack('<12f',*v))
 print(name,len(cooked)//3, 'triangles')
meta=json.loads((SRC/'manifest.json').read_text());meta['modifications']='Source vertex colors converted from linear to sRGB; rotated toward +Z, uniform scale; source main rotor nodes 1 and 9 separated for runtime rotation. Added procedural tail rotor overlay.'
(SRC/'manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
(ROOT/'assets/models/HELICOPTER.md').write_text('# Helicopter attribution\n\nHelicopter by **Poly by Google**, [source](https://poly.pizza/m/cTzINMr0WdS), [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/). Source GLB, SHA-256, license and modifications are retained in `source/helicopter/`. Cook offline with `tools/import_helicopter.py`. Main rotor blades are separate runtime geometry; tail rotor is an original procedural addition.\n')

