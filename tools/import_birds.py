"""Bake CC-BY textured bird bodies with procedural flight wings and flap poses.
Run offline with Python + Pillow. Source models, URLs and hashes are retained.
"""
import json
import math
import pathlib
import import_animals as animals

ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/models/source/birds'
OUTPUT=ROOT/'assets/models/baked/birds'

def main():
    OUTPUT.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8-sig'))
    colors={'crow':(35,35,33),'dove':(215,215,205),'parrot':(35,85,165),'sparrow':(105,78,53)}
    for item in manifest:
        name=item['id'];verts,tex=animals.extract(SOURCE/(name+'.glb'))
        lo=[min(v[i] for v in verts) for i in range(3)];hi=[max(v[i] for v in verts) for i in range(3)]
        extent=[hi[i]-lo[i] for i in range(3)];body=[]
        for v in verts:
            q=list(v)
            x,y,z=[(v[i]-lo[i])/extent[i]-0.5 for i in range(3)]
            if name!='seagull':
                # Tuck the perched source forward; separate flight wings extend
                # the silhouette without stretching its head, legs, or body.
                y,z=y*0.70-z*0.71,y*0.71+z*0.70
                q[:3]=[x*0.25,y*0.55,z*0.85]
            else:q[:3]=[x,y*0.6,z]
            body.append(q)
        if name!='seagull':
            target=colors[name];pixels=list(tex.getdata());best=min(range(len(pixels)),key=lambda i:sum((pixels[i][c]-target[c])**2 for c in range(3)))
            uv=((best%tex.width+0.5)/tex.width,(best//tex.width+0.5)/tex.height)
            outline=[(.10,.16),(.27,.10),(.5,-.16),(.46,-.27),(.38,-.20),(.31,-.28),(.24,-.20),(.11,-.16)]
            for side in (-1,1):
                center=(.18*side,0,-.07)
                for i in range(len(outline)):
                    a=outline[i];b=outline[(i+1)%len(outline)]
                    tri=[center,(a[0]*side,0,a[1]),(b[0]*side,0,b[1])]
                    for points,normal in [(tri,(0,1,0)),(tri[::-1],(0,-1,0))]:
                        for p in points:body.append([*p,*normal,*uv,1,1,1,1])
        tex.save(OUTPUT/(name+'.png'))
        for frame in range(8):
            pose=[];bend=math.sin(frame*math.tau/8)*0.65
            for v in body:
                q=v.copy();wing=max(0,abs(v[0])-.10)
                q[1]+=wing*bend;q[0]*=1-abs(bend)*wing*.3
                pose.append(q)
            animals.write(OUTPUT/(name+'-flap-'+str(frame)+'.m3d'),pose)
        dead=[]
        for v in body:
            q=v.copy();q[0],q[1]=v[1],-v[0];q[3],q[4]=v[4],-v[3];dead.append(q)
        animals.write(OUTPUT/(name+'-dead.m3d'),dead)
        item['modifications']='Normalized source textured body; procedural eight-pose wing flapping and rolled death pose.'
        if name!='seagull':item['modifications']+=' Perched body tilted forward with added feather-shaped flight wings sampling the original texture.'
        print(name,len(body)//3,'triangles')
    (SOURCE/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    text='# Bird model attribution\n\nAll five original textured GLBs are by **Poly by Google**, under [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/). The full license is in `source/birds/CC-BY-3.0.txt`; source hashes are in `source/birds/manifest.json`.\n\n'
    for item in manifest:text+=f"- **{item['id']}**: [{item['title']}]({item['page']}). {item['modifications']}\n"
    (ROOT/'assets/models/BIRDS.md').write_text(text)

if __name__=='__main__':main()
