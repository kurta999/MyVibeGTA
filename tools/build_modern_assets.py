"""Author original coastal city assets, editable GLBs and DX11 M3D2 meshes.

No downloaded models or textures are used. NumPy and Pillow author the 2K maps.
Coordinates are metres, Y up, +Z vehicle/weapon front. PBR sections and authored
LOD reductions are retained in the game; GLBs preserve editable material groups.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib
from build_modern_materials import build as build_materials

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/models/baked/modern"
SOURCE = ROOT / "assets/models/source/modern"
MATERIALS = {
    # sRGB colour, perceptual roughness, metalness, emission, texture pattern.
    "concrete": ((215, 210, 194), .83, 0, 0, "stone"),
    "stone": ((119, 130, 135), .72, 0, 0, "stone"),
    "glass": ((78, 113, 128), .12, 0, 0, "smooth"),
    "car_glass": ((90, 124, 139), .10, 0, 0, "smooth"),
    "metal": ((95, 107, 114), .31, 1, 0, "brushed"),
    "silver": ((190, 199, 205), .23, 1, 0, "brushed"),
    "bronze": ((150, 115, 71), .32, 1, 0, "brushed"),
    "blue_paint": ((37, 113, 143), .23, .45, 0, "smooth"),
    "pearl_paint": ((209, 215, 214), .26, .30, 0, "smooth"),
    "rubber": ((27, 29, 31), .94, 0, 0, "rubber"),
    "polymer": ((42, 47, 50), .76, 0, 0, "rubber"),
    "gunmetal": ((75, 84, 91), .33, .92, 0, "brushed"),
    "wood": ((149, 107, 68), .70, 0, 0, "wood"),
    "white_lens": ((221, 238, 242), .19, 0, 0, "lens"),
    "red_lens": ((175, 25, 30), .20, 0, 0, "lens"),
    "amber": ((234, 163, 48), .35, 0, 0, "lens"),
    "hydrant": ((181, 58, 42), .38, .35, 0, "smooth"),
    "foliage": ((75, 112, 57), .88, 0, 0, "stone"),
}
SURFACES = {
    "blue_paint": (1.0, .09, 0),
    "pearl_paint": (1.0, .12, 0),
    "hydrant": (.6, .18, 0),
    "glass": (0, .1, 1.5),
    "car_glass": (0, .1, 1.5),
}


def png(path, pixels, size=256):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    rows = b"".join(b"\0" + pixels[y*size*4:(y+1)*size*4] for y in range(size))
    path.write_bytes(b"\x89PNG\r\n\x1a\n" +
                    chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)) +
                    chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def textures():
    build_materials(MATERIALS, OUT)


def sub(a, b): return tuple(x-y for x, y in zip(a, b))
def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def dot(a, b): return sum(x*y for x, y in zip(a, b))
def unit(v):
    length = math.sqrt(dot(v, v))
    if length < 1e-10: raise ValueError("Degenerate authored surface")
    return tuple(x/length for x in v)


class Mesh:
    def __init__(self): self.sections = {}

    def face(self, points, material, normals=None):
        n = unit(cross(sub(points[1], points[0]), sub(points[2], points[0])))
        # Choose planar UV axes by dominant normal; source UVs tile correctly.
        axis = max(range(3), key=lambda i: abs(n[i]))
        axes = (2, 1) if axis == 0 else (0, 2) if axis == 1 else (0, 1)
        verts = self.sections.setdefault(material, [])
        for i in range(1, len(points)-1):
            for k in (0, i, i+1):
                p = points[k]
                v = (*p, *(normals[k] if normals else n), p[axes[0]]*.6,
                     p[axes[1]]*.6, 1, 1, 1, .24 if material == "car_glass" else 1)
                verts.append(v)

    def box(self, lo, hi, mat):
        x, y, z = lo; X, Y, Z = hi
        if min(X-x, Y-y, Z-z) <= 0: raise ValueError("Invalid box")
        for corners in (
            ((x,y,z),(x,Y,z),(X,Y,z),(X,y,z)),
            ((X,y,Z),(X,Y,Z),(x,Y,Z),(x,y,Z)),
            ((x,y,Z),(x,Y,Z),(x,Y,z),(x,y,z)),
            ((X,y,z),(X,Y,z),(X,Y,Z),(X,y,Z)),
            ((x,Y,z),(x,Y,Z),(X,Y,Z),(X,Y,z)),
            ((x,y,Z),(x,y,z),(X,y,z),(X,y,Z)),
        ): self.face(corners, mat)

    def beam(self, a, b, radius, mat, sides=12, radius_end=None):
        axis = unit(sub(b, a))
        ref = (0,1,0) if abs(axis[1]) < .9 else (1,0,0)
        u = unit(cross(axis, ref)); v = cross(axis, u)
        radius_end = radius if radius_end is None else radius_end
        ring_a, ring_b, ns = [], [], []
        for j in range(sides):
            angle = j*math.tau/sides
            normal = tuple(u[i]*math.cos(angle)+v[i]*math.sin(angle) for i in range(3))
            ns.append(normal)
            ring_a.append(tuple(a[i]+normal[i]*radius for i in range(3)))
            ring_b.append(tuple(b[i]+normal[i]*radius_end for i in range(3)))
        for j in range(sides):
            k = (j+1)%sides
            self.face((ring_a[j],ring_a[k],ring_b[k],ring_b[j]),mat,
                      (ns[j],ns[k],ns[k],ns[j]))
        self.face(tuple(reversed(ring_a)),mat)
        self.face(tuple(ring_b),mat)

    def torus(self, center, major, minor, mat, sides=32, tube=8, axis=0):
        def point(i, j):
            a, b = i*math.tau/sides, j*math.tau/tube
            p = (minor*math.sin(b),(major+minor*math.cos(b))*math.cos(a),
                 (major+minor*math.cos(b))*math.sin(a))
            n = (math.sin(b),math.cos(b)*math.cos(a),math.cos(b)*math.sin(a))
            if axis == 1: p=(p[1],p[0],p[2]); n=(n[1],n[0],n[2])
            if axis == 2: p=(p[2],p[1],p[0]); n=(n[2],n[1],n[0])
            return tuple(center[k]+p[k] for k in range(3)), n
        for i in range(sides):
            for j in range(tube):
                samples = [point(a,b) for a,b in ((i,j),(i+1,j),(i+1,j+1),(i,j+1))]
                # Reversing one basis permutation reverses winding.
                if axis in (1,2): samples.reverse()
                self.face(tuple(p for p,n in samples),mat,tuple(n for p,n in samples))

    def bevel_box(self, lo, hi, mat, radius):
        center=tuple((lo[i]+hi[i])/2 for i in range(3))
        half=tuple((hi[i]-lo[i])/2 for i in range(3))
        r=min(radius,min(half)*.45)
        def surface(points,normal):
            if dot(cross(sub(points[1],points[0]),sub(points[2],points[0])),normal)<0:
                points=list(reversed(points))
            self.face(tuple(tuple(p[i]+center[i] for i in range(3)) for p in points),mat)
        for axis in range(3):
            other=[i for i in range(3) if i!=axis]
            for sign in (-1,1):
                points=[]
                for a,b in ((-1,-1),(1,-1),(1,1),(-1,1)):
                    p=[0,0,0];p[axis]=sign*half[axis]
                    p[other[0]]=a*(half[other[0]]-r);p[other[1]]=b*(half[other[1]]-r)
                    points.append(p)
                normal=[0,0,0];normal[axis]=sign;surface(points,normal)
        for a,b,c in ((0,1,2),(0,2,1),(1,2,0)):
            for sa in (-1,1):
                for sb in (-1,1):
                    points=[]
                    for face,end in ((a,-1),(a,1),(b,1),(b,-1)):
                        p=[0,0,0];p[a]=sa*(half[a]-(0 if face==a else r))
                        p[b]=sb*(half[b]-(0 if face==b else r));p[c]=end*(half[c]-r)
                        points.append(p)
                    normal=[0,0,0];normal[a]=sa;normal[b]=sb;surface(points,normal)
        for sx in (-1,1):
            for sy in (-1,1):
                for sz in (-1,1):
                    signs=(sx,sy,sz)
                    points=[tuple(signs[i]*(half[i]-(0 if i==axis else r)) for i in range(3))
                            for axis in range(3)]
                    surface(points,signs)

    def loft(self, rings, mat, smooth=False):
        # z, half-width, bottom, shoulder, hood/roof height: octagonal contour.
        loops=[]
        for z,w,b,s,t in rings:
            loops.append(((-w*.83,b,z),(w*.83,b,z),(w,s,z),(w*.85,t,z),
                          (-w*.85,t,z),(-w,s,z)))
        if smooth:
            # Rounded cross-section corners and Catmull-Rom longitudinal curves.
            def rounded(loop):
                result=[]
                for i,p in enumerate(loop):
                    prev,next=loop[(i-1)%6],loop[(i+1)%6]
                    a=tuple(p[k]+(prev[k]-p[k])*.16 for k in range(3))
                    b=tuple(p[k]+(next[k]-p[k])*.16 for k in range(3))
                    for j in range(4):
                        t=j/3
                        result.append(tuple((1-t)**2*a[k]+2*t*(1-t)*p[k]+t*t*b[k] for k in range(3)))
                return result
            expanded=[]
            for i in range(len(loops)-1):
                p0,p1,p2,p3=(loops[max(0,i-1)],loops[i],loops[i+1],loops[min(len(loops)-1,i+2)])
                for j in range(3):
                    t=j/3
                    ring=[tuple(.5*((2*p1[c][k])+(-p0[c][k]+p2[c][k])*t+
                        (2*p0[c][k]-5*p1[c][k]+4*p2[c][k]-p3[c][k])*t*t+
                        (-p0[c][k]+3*p1[c][k]-3*p2[c][k]+p3[c][k])*t*t*t) for k in range(3)) for c in range(6)]
                    expanded.append(rounded(ring))
            expanded.append(rounded(loops[-1]));loops=expanded
        count=len(loops[0])
        for row,(a,b) in enumerate(zip(loops,loops[1:])):
            for i in range(count):
                points=(a[i],a[(i+1)%count],b[(i+1)%count],b[i])
                normals=None
                if smooth:
                    normals=[]
                    for rr,col in ((row,i),(row,(i+1)%count),(row+1,(i+1)%count),(row+1,i)):
                        around=sub(loops[rr][(col+1)%count],loops[rr][(col-1)%count])
                        along=sub(loops[min(len(loops)-1,rr+1)][col],loops[max(0,rr-1)][col])
                        normals.append(unit(cross(around,along)))
                self.face(points,mat,normals)
        self.face(tuple(reversed(loops[0])),mat)
        self.face(loops[-1],mat)

    def split_glass(self):
        glass=Mesh()
        if "car_glass" in self.sections: glass.sections["car_glass"]=self.sections.pop("car_glass")
        return glass

    def packed(self):
        vertices, indices, ranges, lookup = [], [], [], {}
        for mat, corners in self.sections.items():
            start=len(indices)
            for vertex in corners:
                value=struct.pack("<12f",*vertex)
                if value not in lookup:
                    lookup[value]=len(vertices);vertices.append(value)
                indices.append(lookup[value])
            ranges.append((mat,start,len(indices)-start))
        if not indices: raise ValueError("Empty authored mesh")
        return vertices,indices,ranges

    def save(self, name):
        vertices,indices,ranges=self.packed()
        data=struct.pack("<4sII",b"M3D2",len(vertices),len(indices))+b"".join(vertices)
        data+=struct.pack("<"+"I"*len(indices),*indices)
        target=OUT/f"{name}.m3d";target.write_bytes(data)
        lines=["# Original procedural project asset: indexed PBR material sections"]
        for mat,start,count in ranges:
            colour,rough,metal,emission,_=MATERIALS[mat]
            lines.append(f"{start} {count} {rough} {metal} {emission} {mat}-base.dds "
                         f"{mat}-normal.dds {mat}-orm.dds {mat}-orm.dds - OPAQUE 0.5 SURFACE1 "
                         + " ".join(str(v) for v in SURFACES.get(mat,(0,.1,0))))
        (OUT/f"{name}.pbr").write_text("\n".join(lines)+"\n",encoding="utf-8")
        positions=[struct.unpack("<12f",v)[:3] for v in vertices]
        bounds=[[min(p[i] for p in positions) for i in range(3)],
                [max(p[i] for p in positions) for i in range(3)]]
        return {"name":f"modern/{name}","triangles":len(indices)//3,
                "vertices":len(vertices),"materials":[r[0] for r in ranges],
                "bounds":bounds,"sha256":hashlib.sha256(data).hexdigest()}

    def glb(self, name):
        binary=bytearray();views=[];accessors=[];images=[];materials=[];primitives=[]
        def view(data,target=None):
            while len(binary)%4: binary.append(0)
            result={"buffer":0,"byteOffset":len(binary),"byteLength":len(data)}
            if target: result["target"]=target
            views.append(result);binary.extend(data);return len(views)-1
        def accessor(data,component,kind,count,bounds=None):
            result={"bufferView":view(data,34963 if kind=="SCALAR" else 34962),
                    "componentType":component,"count":count,"type":kind}
            if bounds: result.update(min=bounds[0],max=bounds[1])
            accessors.append(result);return len(accessors)-1
        for mat,corners in self.sections.items():
            vertices=[];indices=[];lookup={}
            for v in corners:
                if v not in lookup: lookup[v]=len(vertices);vertices.append(v)
                indices.append(lookup[v])
            def attribute(start,end,kind):
                values=[v[start:end] for v in vertices]
                bounds=([min(v[i] for v in values) for i in range(end-start)],
                        [max(v[i] for v in values) for i in range(end-start)]) if start==0 else None
                packed=b"".join(struct.pack("<"+"f"*(end-start),*v) for v in values)
                return accessor(packed,5126,kind,len(values),bounds)
            attrs={"POSITION":attribute(0,3,"VEC3"),"NORMAL":attribute(3,6,"VEC3"),
                   "TEXCOORD_0":attribute(6,8,"VEC2")}
            index=accessor(struct.pack("<"+"I"*len(indices),*indices),5125,"SCALAR",len(indices))
            image_ids=[]
            for channel in ("base","normal","orm"):
                images.append({"name":f"{mat}-{channel}","bufferView":view((OUT/f"{mat}-{channel}.png").read_bytes()),
                               "mimeType":"image/png"});image_ids.append(len(images)-1)
            _,rough,metal,emission,_=MATERIALS[mat]
            material={"name":mat,"pbrMetallicRoughness":{"baseColorTexture":{"index":image_ids[0]},
                      "metallicRoughnessTexture":{"index":image_ids[2]},"roughnessFactor":rough,"metallicFactor":metal},
                      "normalTexture":{"index":image_ids[1]},"occlusionTexture":{"index":image_ids[2]}}
            if mat=="car_glass":
                material["alphaMode"]="BLEND";material["pbrMetallicRoughness"]["baseColorFactor"]=[1,1,1,.24]
            coat,coat_roughness,ior=SURFACES.get(mat,(0,.1,0))
            if coat:
                material.setdefault("extensions",{})["KHR_materials_clearcoat"]={
                    "clearcoatFactor":coat,"clearcoatRoughnessFactor":coat_roughness}
            if ior:
                material.setdefault("extensions",{})["KHR_materials_ior"]={"ior":ior}
            materials.append(material)
            primitives.append({"attributes":attrs,"indices":index,"material":len(materials)-1})
        doc={"asset":{"version":"2.0","generator":"MiniCity3D original procedural authoring"},
             "scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"name":name,"mesh":0}],
             "meshes":[{"name":name,"primitives":primitives}],"bufferViews":views,"accessors":accessors,
             "buffers":[{"byteLength":len(binary)}],"images":images,"materials":materials,
             "samplers":[{"wrapS":10497,"wrapT":10497,"magFilter":9729,"minFilter":9987}],
             "textures":[{"source":i,"sampler":0} for i in range(len(images))]}
        extensions=sorted({name for mat in materials for name in mat.get("extensions",{})})
        if extensions: doc["extensionsUsed"]=extensions
        encoded=json.dumps(doc,separators=(",",":")).encode();encoded+=b" "*((-len(encoded))%4)
        binary+=b"\0"*((-len(binary))%4)
        total=12+8+len(encoded)+8+len(binary)
        (SOURCE/f"{name}.glb").write_bytes(struct.pack("<4sII",b"glTF",2,total)+
            struct.pack("<I4s",len(encoded),b"JSON")+encoded+struct.pack("<I4s",len(binary),b"BIN\0")+binary)


def building(style, level):
    m=Mesh(); width,depth,height=(22,18,32) if style==0 else (24,18,24)
    floors=7 if style==0 else 5
    # Footprint and roof extents stay identical at every LOD, including balconies.
    m.box((-width/2,0,-depth/2-.16),(width/2,.30,depth/2),"stone")
    m.box((-width/2+.7,.3,-depth/2+.9),(width/2-.7,height-.5,depth/2-.9),"concrete")
    m.box((-width/2,height-.5,-depth/2),(width/2,height,depth/2),"concrete")
    m.box((-3,height,-2),(0,height+1.2,1),"metal")
    if level<2:
        for i in range(15):
            m.box((-2.85+i*.18,height+.2,-2.015),(-2.79+i*.18,height+1.0,-2.005),"polymer")
    m.box((-width/2+1,.31,-depth/2+.16),(width/2-1,3.7,-depth/2+.27),"glass")
    m.box((-width/2,3.7,-depth/2),(width/2,4.1,-depth/2+1.8),"bronze" if style==0 else "metal")
    for side in (-1,1):
        z=side*(depth/2-.86)
        for floor in range(floors):
            y=4.3+floor*(height-5)/floors
            fh=(height-5)/floors-.6
            columns=6 if level<2 else 3 if level==2 else 1
            for col in range(columns):
                cell=(width-2)/columns;x=-width/2+1+col*cell
                m.box((x+.08,y,min(z,z+side*.05)),(x+cell-.08,y+fh,max(z,z+side*.05)),"glass")
                if level<2:
                    for post in (x,x+cell-.07):
                        m.box((post,y-.05,z-.055),(post+.065,y+fh+.07,z+.055),"metal")
                    m.box((x,y-.06,z-.07),(x+cell,y+.025,z+.07),"silver")
                    if style==0 and level==0:
                        for slat in range(2):
                            offset=x+.20+slat*.16
                            m.box((offset,y,z-.21),(offset+.06,y+fh,z+.21),"bronze")
            if style==1 and level<3:
                # Projecting walkways and real railings, with recessed panes behind.
                zz=side*(depth/2-1.1)
                m.box((-width/2+.6,y-.28,min(zz,side*depth/2)),
                      (width/2-.6,y-.1,max(zz,side*depth/2)),"concrete")
                rail=side*(depth/2-.16)
                m.beam((-width/2+.65,y+.92,rail),(width/2-.65,y+.92,rail),.035,"metal",8)
                for col in range(12 if level==0 else 6):
                    x=-width/2+.65+col*(width-1.3)/(11 if level==0 else 5)
                    m.beam((x,y,rail),(x,y+.92,rail),.025,"metal",6)
    for side in (-1,1):
        x=side*(width/2-.66)
        for floor in range(floors if level<3 else 1):
            y=4.3+floor*(height-5)/floors
            top=y+(height-5)/floors-.6 if level<3 else height-1
            for col in range(3 if level<2 else 1):
                cell=(depth-3)/(3 if level<2 else 1);z=-depth/2+1.5+col*cell
                m.box((x-.03,y,z+.12),(x+.03,top,z+cell-.12),"glass")
    if level<2:
        for side in (-1,1):
            m.box((side*(width/2-1)-.13,.3,-depth/2+.15),
                  (side*(width/2-1)+.13,height-.5,-depth/2+.52),"bronze")
        m.box((-2,.3,-depth/2-.01),(2,3.3,-depth/2+.15),"metal")
        m.box((-1.83,.38,-depth/2-.07),(1.83,3.18,-depth/2-.015),"glass")
        for x in (-.07,1.2):m.beam((x,1.05,-depth/2-.12),(x,1.65,-depth/2-.12),.035,"silver",8)
        # Recessed mechanical equipment stays inside roof silhouette.
        for x in (-6,4):
            m.box((x,height-.7,-2),(x+2,height-.51,1),"metal")
        if level==0:
            for i in range(18):
                m.box((-5.9+i*.09,height-.505,-1.9),(-5.87+i*.09,height-.501,.9),"silver")
    return m


def weapon(rifle, level=0):
    m=Mesh();detail=level<2
    if rifle:
        m.bevel_box((-.043,.15,-.24),(.043,.245,.21),"gunmetal",.006)
        m.bevel_box((-.032,.156,.21),(.032,.217,.45),"gunmetal",.004)
        m.beam((0,.194,.43),(0,.194,.66),.018,"gunmetal",20)
        m.beam((0,.194,.64),(0,.194,.69),.025,"metal",16)
        m.bevel_box((-.035,.16,-.45),(.035,.22,-.24),"polymer",.005)
        m.box((-.045,.07,-.50),(.045,.235,-.44),"rubber")
        m.loft([(-.15,.030,.05,.09,.155),(-.07,.028,-.015,.045,.15)],"polymer")
        m.box((-.025,.025,.02),(.025,.15,.115),"gunmetal")
        m.box((-.019,.045,-.15),(.019,.15,-.10),"polymer")
        for a,b in (((-.03,.12,-.07),(.03,.12,-.07)),((-.03,.07,-.07),(.03,.07,-.07))):
            m.beam(a,b,.006,"metal",8)
        if detail:
            for i in range(16):
                z=-.20+i*.037
                m.box((-.027,.245,z),(.027,.257,z+.016),"metal")
            for side in (-1,1):
                for i in range(7):
                    z=.215+i*.029
                    m.box((side*.034-.004,.17,z),(side*.034+.004,.203,z+.015),"polymer")
            m.box((-.028,.253,-.09),(.028,.283,.025),"polymer")
            m.beam((0,.288,-.035),(0,.288,.030),.029,"gunmetal",16)
            m.beam((0,.288,.031),(0,.288,.032),.024,"glass",16)
    else:
        m.bevel_box((-.028,.065,-.095),(.028,.122,.14),"gunmetal",.004)
        m.bevel_box((-.023,.044,-.08),(.023,.074,.105),"polymer",.003)
        m.loft([(-.10,.023,-.052,-.016,.065),(-.035,.023,-.041,.004,.061)],"polymer")
        m.beam((0,.090,.113),(0,.090,.145),.010,"metal",20)
        m.beam((0,.090,.145),(0,.090,.146),.006,"rubber",16)
        # Open trigger guard assembled around the trigger.
        m.beam((-.022,.048,-.019),(-.022,.015,-.005),.005,"polymer",8)
        m.beam((-.022,.015,-.005),(-.022,.017,.05),.005,"polymer",8)
        m.beam((-.022,.017,.05),(-.022,.05,.064),.005,"polymer",8)
        m.beam((0,.049,.015),(0,.026,.023),.003,"metal",8)
        m.box((-.022,-.055,-.10),(.022,-.048,-.035),"metal")
        for z in (-.07,.118):m.box((-.009,.122,z),(.009,.130,z+.01),"metal")
        if detail:
            for side in (-1,1):
                for i in range(7):
                    z=-.081+i*.006
                    m.box((side*.029-.002,.074,z),(side*.029+.002,.115,z+.002),"metal")
                for i in range(6):
                    y=-.035+i*.012
                    m.beam((side*.024,y,-.087),(side*.024,y,-.049),.0015,"rubber",6)
    return m


def prop(kind, level=0):
    m=Mesh();sides=20 if level==0 else 10
    if kind in ("street-lamp","twin-lamp"):
        m.box((-.18,0,-.18),(.18,.08,.18),"metal")
        m.beam((0,.08,0),(0,4.55,0),.085,"metal",sides,.045)
        arms=(-1,1) if kind=="twin-lamp" else (-1,)
        for sign in arms:
            m.beam((0,4.38,0),(sign*.7,4.62,0),.045,"metal",12)
            m.beam((sign*.7,4.62,0),(sign*1.25,4.62,0),.042,"metal",12)
            x=sign*1.31
            m.box((x-.30,4.48,-.23),(x+.30,4.69,.23),"metal")
            m.box((x-.24,4.465,-.18),(x+.24,4.485,.18),"white_lens")
            if level==0:
                for i in range(9):
                    m.box((x-.26+i*.061,4.69,-.20),(x-.24+i*.061,4.72,.20),"metal")
        if level==0:
            m.box((-.088,.32,-.056),(-.076,.64,.056),"polymer")
            for x in (-.13,.13):
                for z in (-.13,.13):m.beam((x,.075,z),(x,.097,z),.012,"silver",8)
    elif kind=="bench":
        for x in (-.70,.70):
            m.box((x-.06,0,-.28),(x+.06,.42,.30),"metal")
            m.beam((x,.38,.24),(x,.82,.36),.033,"metal",12)
            m.beam((x,.46,-.28),(x,.62,-.28),.023,"metal",10)
            m.beam((x,.62,-.28),(x,.62,.22),.023,"metal",10)
        for i in range(6):
            z=-.27+i*.096
            m.box((-.90,.42,z),(.90,.46,z+.078),"wood")
        for i in range(4):
            y=.5+i*.085
            m.box((-.90,y,.29),(.90,y+.064,.33),"wood")
    elif kind=="bin":
        m.beam((0,.05,0),(0,.80,0),.26,"metal",sides)
        m.beam((0,.80,0),(0,.95,0),.285,"metal",sides,.22)
        m.torus((0,.80,0),.27,.025,"silver",sides,6,1)
        m.box((-.13,.80,-.267),(.13,.91,-.255),"rubber")
        if level==0:
            for j in range(20):
                a=j*math.tau/20;x=.264*math.cos(a);z=.264*math.sin(a)
                m.beam((x,.10,z),(x,.74,z),.008,"silver",6)
    elif kind=="bollard":
        m.beam((0,0,0),(0,.80,0),.075,"metal",sides)
        m.beam((0,.71,0),(0,.75,0),.078,"amber",sides)
        m.beam((0,.80,0),(0,.815,0),.076,"silver",sides,.052)
        m.box((-.12,0,-.12),(.12,.045,.12),"metal")
    elif kind=="bike-rack":
        for x in (-.58,0,.58):
            # Rounded inverted U with capped floor anchors.
            points=[(x-.22,0,0),(x-.22,.65,0)]
            for i in range(9):
                a=math.pi-i*math.pi/8
                points.append((x+math.cos(a)*.22,.65+math.sin(a)*.22,0))
            points.append((x+.22,0,0))
            for a,b in zip(points,points[1:]):
                if dot(sub(a,b),sub(a,b))>1e-10:m.beam(a,b,.028,"silver",10)
    elif kind=="planter":
        m.box((-.60,0,-.28),(.60,.55,.28),"stone")
        m.box((-.54,.55,-.22),(.54,.565,.22),"rubber")
        for i in range(13):
            x=-.49+i*.081;h=.23+(i*7%5)*.025
            m.beam((x,.56,0),(x+.06,.56+h,.035),.008,"foliage",6,.002)
            for side in (-1,1):
                m.face(((x,.59,0),(x+side*.09,.64,.035),
                        (x+side*.14,.70,.07),(x+side*.06,.655,.065)),"foliage")
    elif kind=="hydrant":
        m.beam((0,.02,0),(0,.61,0),.115,"hydrant",sides)
        m.beam((0,.61,0),(0,.74,0),.13,"hydrant",sides,.04)
        m.beam((-.25,.43,0),(.25,.43,0),.075,"hydrant",16)
        for x in (-.255,.255):m.beam((x-.012,.43,0),(x+.012,.43,0),.085,"silver",10)
        m.beam((0,.39,-.18),(0,.39,-.09),.080,"silver",12)
        m.box((-.17,0,-.17),(.17,.035,.17),"metal")
    else: raise ValueError(kind)
    return m


def generate():
    OUT.mkdir(parents=True,exist_ok=True);SOURCE.mkdir(parents=True,exist_ok=True)
    textures();records=[]
    families=(("coastal-office",lambda level:building(0,level)),
              ("terrace-apartments",lambda level:building(1,level)))
    for name,build in families:
        meshes=[build(level) for level in range(4)]
        meshes[0].glb(name)
        for level,m in enumerate(meshes):
            suffix="" if level==0 else f"-lod{level}"
            records.append(m.save(name+suffix))
        (OUT/f"{name}.lod").write_text("MCLOD1\n"+"\n".join(
            f"modern/{name}{'' if i==0 else '-lod'+str(i)} {threshold}"
            for i,threshold in enumerate((500,240,80,0)))+"\n",encoding="utf-8")
    for name,build in (("compact-pistol",lambda:weapon(False)),("carbine",lambda:weapon(True))):
        m=build();m.glb(name);records.append(m.save(name))
    for name in ("street-lamp","twin-lamp","bench","bin","bollard","bike-rack","planter","hydrant"):
        m=prop(name);m.glb(name);records.append(m.save(name))
    report={"schema":1,"authoring":"Original project procedural geometry and textures; no external source assets",
            "generator":"tools/build_modern_assets.py","units":"metres; Y up; +Z front",
            "runtime":"indexed M3D2 with per-section PBR; static geometry",
            "limitations":["Glass uses blended raster approximation",
                            "No interior-parallax or refractive transmission shader is claimed", "No new gameplay collision shapes for decorative street props"],
            "assets":records}
    (OUT/"manifest.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"Authored {len(records)} cooked meshes and 12 editable GLBs")
    for item in records:
        print(f"{item['name']}: {item['triangles']} triangles, {len(item['materials'])} materials")


if __name__=="__main__":
    argparse.ArgumentParser(description=__doc__).parse_args()
    generate()
