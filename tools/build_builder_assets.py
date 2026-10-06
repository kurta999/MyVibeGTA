"""Reproducible original builder meshes, textures, icons and catalog (stdlib only).

Textures and geometric recipes are authored here; no Minecraft assets are copied.
The icon renderer uses the exported mesh, UV texture and normals.
"""
from pathlib import Path
import hashlib
import json
import math
import random
import struct
import zlib
from builder_rock_materials import RECIPES as ROCK_RECIPES, texture as rock_texture, recipe_manifest

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'assets/models/baked/builder'
SOURCE = ROOT / 'assets/models/source/builder'
ROCKS = [
    ('chalk', 40, (202, 202, 187)), ('mudstone', 50, (103, 78, 62)),
    ('shale', 60, (64, 70, 80)), ('tuff', 70, (127, 117, 92)),
    ('pumice', 80, (179, 172, 155)), ('sandstone', 90, (177, 131, 79)),
    ('limestone', 100, (192, 184, 146)), ('travertine', 110, (185, 164, 130)),
    ('dolostone', 120, (160, 156, 135)), ('conglomerate', 130, (132, 113, 97)),
    ('slate', 140, (61, 73, 86)), ('marble', 150, (218, 213, 201)),
    ('schist', 165, (129, 140, 145)), ('gneiss', 180, (137, 133, 130)),
    ('andesite', 195, (115, 119, 122)), ('granite', 210, (156, 127, 114)),
    ('diorite', 225, (176, 180, 176)), ('gabbro', 240, (63, 77, 72)),
    ('basalt', 255, (48, 53, 61)), ('quartzite', 280, (174, 171, 164))]
EXTRAS = [
    ('log', 100, (116, 77, 42)), ('plank', 80, (172, 124, 68)),
    ('leaves', 15, (66, 128, 49)), ('soil', 30, (111, 73, 46)),
    ('sand', 25, (217, 195, 141)), ('gravel', 40, (127, 128, 120)),
    ('snow', 20, (223, 233, 240)), ('brick', 160, (170, 76, 52)),
    ('concrete', 220, (141, 143, 137)), ('glass', 25, (138, 193, 210)),
    ('diamond-ore', 320, (73, 171, 191)), ('iron-ore', 200, (145, 104, 74)),
    ('gold-ore', 190, (197, 163, 57)), ('iron-ingot', 1, (156, 169, 177)),
    ('gold-ingot', 1, (209, 167, 49)), ('diamond', 1, (73, 217, 226)),
    ('stick', 1, (132, 88, 45)), ('chest', 80, (142, 91, 44)),
    ('torch', 10, (172, 126, 65)), ('crafting-bench', 100, (128, 83, 43)),
    ('coal', 1, (36, 38, 41)), ('furnace', 220, (104, 108, 112)),
    ('coal-ore', 170, (31, 34, 37)),
    ('tilled-soil', 30, (91, 56, 34)), ('surface-deposit', 400, (145, 113, 76))]
TIERS = [('wood', 1, 60, 2, (151, 103, 57)), ('stone', 2, 132, 4, (125, 131, 134)),
         ('iron', 3, 250, 6, (178, 188, 197)), ('gold', 2, 32, 9, (228, 183, 54)),
         ('diamond', 4, 1562, 8, (75, 211, 217))]

def png(path, width, height, data):
    def chunk(name, payload):
        return struct.pack('>I', len(payload)) + name + payload + struct.pack('>I', zlib.crc32(name + payload))
    rows = b''.join(b'\x00' + bytes(data[y*width*4:(y+1)*width*4]) for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) +
                    chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))

def texture(name, color, tool=False):
    if name in ROCK_RECIPES and not tool:
        return rock_texture(name, color)
    rng = random.Random(name)
    size = 256
    data = bytearray(size*size*4)
    for y in range(size):
        for x in range(size):
            noise = rng.uniform(-16, 16)
            wave = math.sin(x*.13 + math.sin(y*.038)*4)
            shade = noise
            base = color
            if tool and x < 128:
                base = (151, 53, 39) if name == 'shears' else (133, 87, 44)
                shade += 15*wave
            elif name == 'brush':
                base = (178, 184, 190) if y < 64 else (219, 190, 128)
                shade += 14*math.sin(x*1.4) if y >= 64 else 5*wave
            elif name in ('log', 'stick', 'plank', 'chest', 'crafting-bench', 'torch'):
                shade += 18*wave
                if name != 'log' and (y % 64 < 3 or (x+(y//64%2)*64) % 128 < 3):
                    shade -= 45
            elif name == 'brick':
                if y % 48 < 5 or (x+(y//48%2)*48) % 96 < 5:
                    base = (153, 146, 129)
            elif name == 'gravel':
                shade += 30*math.sin(x*.5)*math.cos(y*.5)
            elif name.endswith('-ore'):
                fleck = math.sin((x//12)*11+(y//12)*27) > .6
                if not fleck:
                    base = (83, 86, 89)
            elif name == 'leaves':
                shade += 30*math.sin(x*.3)*math.sin(y*.22)
            elif name == 'tilled-soil':
                shade += 32*math.cos(x*math.pi/16)
            elif name == 'surface-deposit':
                shade += 24*math.sin(x*.5)*math.cos(y*.4)
            elif name == 'glass':
                shade += 45 if abs(x-y) < 8 else 0
            offset = (y*size+x)*4
            data[offset:offset+4] = bytes([max(0,min(255,int(c+shade))) for c in base]+[255])
    return data

def quad(vertices, points, normal, wood=False):
    uv = [(0.03 if wood else .53, .95), (.47 if wood else .97, .95),
          (.47 if wood else .97, .05), (.03 if wood else .53, .05)]
    for index in (0,1,2,0,2,3):
        vertices.append((*points[index], *normal, *uv[index], 1,1,1,1))

def box(vertices, lo, hi, wood=False):
    a,b,c = lo
    d,e,f = hi
    quad(vertices, [(a,b,c),(d,b,c),(d,e,c),(a,e,c)], (0,0,-1), wood)
    quad(vertices, [(d,b,f),(a,b,f),(a,e,f),(d,e,f)], (0,0,1), wood)
    quad(vertices, [(a,b,f),(a,b,c),(a,e,c),(a,e,f)], (-1,0,0), wood)
    quad(vertices, [(d,b,c),(d,b,f),(d,e,f),(d,e,c)], (1,0,0), wood)
    quad(vertices, [(a,e,c),(d,e,c),(d,e,f),(a,e,f)], (0,1,0), wood)
    quad(vertices, [(a,b,f),(d,b,f),(d,b,c),(a,b,c)], (0,-1,0), wood)

def facet(vertices, points):
    a,b,c=points
    u=[b[i]-a[i] for i in range(3)]
    v=[c[i]-a[i] for i in range(3)]
    normal=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
    length=math.sqrt(sum(x*x for x in normal))
    normal=tuple(x/length for x in normal)
    for p in points:
        vertices.append((*p,*normal,.53+(p[0]+.5)*.44,.95-p[1]*.9,1,1,1,1))

def ring_model(vertices, outline, levels):
    # Outward wound, flat shaded bevels; levels are (height, outline scale).
    rings=[[(x*scale,y,z*scale) for x,z in outline] for y,scale in levels]
    for lo,hi in zip(rings,rings[1:]):
        for i in range(len(outline)):
            j=(i+1)%len(outline)
            facet(vertices,(lo[i],hi[i],hi[j]))
            facet(vertices,(lo[i],hi[j],lo[j]))
    for ring,top in ((rings[0],False),(rings[-1],True)):
        center=(0,ring[0][1],0)
        for i in range(len(outline)):
            j=(i+1)%len(outline)
            facet(vertices,(center,ring[j],ring[i]) if top else (center,ring[i],ring[j]))

def resource_model(name):
    vertices=[]
    if name in ('iron-ingot','gold-ingot'):
        outline=[(.5,-.17),(.5,.17),(.43,.24),(-.43,.24),(-.5,.17),(-.5,-.17),(-.43,-.24),(.43,-.24)]
        ring_model(vertices,outline,[(0,.9),(.04,1),(.2,.85),(.24,.73)])
    elif name=='diamond':
        outline=[(.5*math.cos(i*math.pi/4),.5*math.sin(i*math.pi/4)) for i in range(8)]
        ring_model(vertices,outline,[(0,.04),(.38,.94),(.43,1),(.72,.53)])
    elif name=='stick':
        outline=[(.045*math.cos(i*math.pi/3),.045*math.sin(i*math.pi/3)) for i in range(6)]
        ring_model(vertices,outline,[(0,.8),(.03,1),(.85,1),(.9,.8)])
    return vertices

def extrusion(vertices, contour, depth):
    # Clockwise X/Y contour, bevel rings on the head perimeter.
    cx = sum(p[0] for p in contour)/len(contour)
    cy = sum(p[1] for p in contour)/len(contour)
    rings = [(-depth, .90), (-depth*.72, 1), (depth*.72, 1), (depth, .90)]
    for n in range(len(contour)):
        a,b = contour[n],contour[(n+1)%len(contour)]
        length = math.hypot(b[0]-a[0], b[1]-a[1])
        normal = ((b[1]-a[1])/length, (a[0]-b[0])/length, 0)
        for (z0,s0),(z1,s1) in zip(rings,rings[1:]):
            points = [(cx+(a[0]-cx)*s0,cy+(a[1]-cy)*s0,z0),
                      (cx+(b[0]-cx)*s0,cy+(b[1]-cy)*s0,z0),
                      (cx+(b[0]-cx)*s1,cy+(b[1]-cy)*s1,z1),
                      (cx+(a[0]-cx)*s1,cy+(a[1]-cy)*s1,z1)]
            quad(vertices, points, normal)
    for side in (-1,1):
        ring = [(cx+(x-cx)*.9,cy+(y-cy)*.9,side*depth) for x,y in contour]
        if side == -1:
            ring.reverse()
        for i in range(1,len(ring)-1):
            for p in (ring[0],ring[i],ring[i+1]):
                vertices.append((*p,0,0,side,.53+(p[0]+.5)*.44,.95-p[1]*.9,1,1,1,1))

def shears_parts():
    halves=[]
    for sign in (-1,1):
        vertices=[]
        blade=[(sign*x,y) for x,y in ((0,.52),(-.035,.64),(-.025,1.08),(.02,1.12),(.055,1.07),(.07,.62),(.03,.50))]
        if sign == -1:
            blade.reverse()
        extrusion(vertices,blade,.025)
        # A real ring with a visible central opening, front/back and inner walls.
        cx=-sign*.17
        for n in range(20):
            a=n*math.tau/20
            b=(n+1)*math.tau/20
            outer=[(cx+.145*math.cos(t),.215+.18*math.sin(t)) for t in (a,b)]
            inner=[(cx+.094*math.cos(t),.215+.125*math.sin(t)) for t in (a,b)]
            for z,normal in ((-.036,(0,0,-1)),(.036,(0,0,1))):
                points=[(*outer[0],z),(*outer[1],z),(*inner[1],z),(*inner[0],z)]
                if z < 0: points.reverse()
                quad(vertices,points,normal,True)
            quad(vertices,[(*outer[0],-.036),(*outer[1],-.036),(*outer[1],.036),(*outer[0],.036)],(math.cos((a+b)/2),math.sin((a+b)/2),0),True)
            quad(vertices,[(*inner[1],-.036),(*inner[0],-.036),(*inner[0],.036),(*inner[1],.036)],(-math.cos((a+b)/2),-math.sin((a+b)/2),0),True)
        box(vertices,(min(0,cx)-.024,.35,-.03),(max(0,cx)+.024,.53,.03))
        # Offset crossed blades in depth while leaving their common pivot intact.
        vertices=[(v[0],v[1],v[2]+sign*.028,*v[3:]) for v in vertices]
        halves.append(vertices)
    pivot=[]
    extrusion(pivot,[(.067*math.cos(n*math.tau/12),.52+.067*math.sin(n*math.tau/12)) for n in range(12)],.068)
    return halves+[pivot]

def shear_rotation(vertices,angle):
    co,si=math.cos(angle),math.sin(angle)
    return [(co*v[0]-si*(v[1]-.52),.52+si*v[0]+co*(v[1]-.52),v[2],co*v[3]-si*v[4],si*v[3]+co*v[4],*v[5:]) for v in vertices]

def toolmesh(kind):
    if kind == 'shears':
        left,right,pivot=shears_parts()
        return shear_rotation(left,.22)+shear_rotation(right,-.22)+pivot
    vertices = []
    box(vertices,(-.045,.50 if kind=='shovel' else 0,-.045),(.045,.90 if kind=='shovel' else .85,.045),True)
    box(vertices,(-.057,.68 if kind=='shovel' else .13,-.058),(.057,.76 if kind=='shovel' else .25,.058),True)
    if kind == 'brush':
        # Separate metal binding and staggered bristle bundles, each with depth.
        binding=[]
        box(binding,(-.17,.73,-.075),(.17,.88,.075))
        vertices.extend((*v[:7],.05+(v[7]-.05)*.18/.9,*v[8:]) for v in binding)
        for row in range(2):
            for column in range(8):
                bristles=[]
                x=-.164+column*.042
                z=-.069+row*.075
                box(bristles,(x,.875,z),(x+.031,1.10+((column+row)%3)*.014,z+.06))
                vertices.extend((*v[:7],.3+(v[7]-.05)*.65/.9,*v[8:]) for v in bristles)
        return vertices
    contours = {
        'pickaxe': [(-.44,.77),(-.31,.94),(0,1.02),(.31,.94),(.44,.77),(.24,.84),(0,.87),(-.24,.84)],
        'axe': [(-.08,.72),(-.08,1.04),(.23,1.02),(.39,.94),(.42,.73),(.29,.64),(.12,.73)],
        'shovel': [(-.045,.65),(-.17,.63),(-.19,.37),(-.12,.28),(0,.23),(.12,.28),(.19,.37),(.17,.63),(.045,.65)],
        'hoe': [(-.08,.84),(-.08,1.02),(.31,1.02),(.39,.94),(.39,.75),(.30,.75),(.27,.85)],
        'shears': [(-.14,.65),(-.24,1.02),(-.12,.91),(0,.62),(.12,.91),(.24,1.02),(.14,.65)],
        'brush': [(-.15,.75),(-.15,1.03),(.15,1.03),(.15,.75)]}
    extrusion(vertices, contours[kind], .05 if kind!='axe' else .075)
    box(vertices,(-.07,.78,-.07),(.07,.84,.07))
    if kind=='shovel':
        # Shovel blade at the lower end; D-shaped grip at the top.
        box(vertices,(-.13,.83,-.04),(-.08,1.03,.04),True)
        box(vertices,(.08,.83,-.04),(.13,1.03,.04),True)
        box(vertices,(-.13,.99,-.04),(.13,1.03,.04),True)
    return vertices

def icon(vertices, tex):
    size=96
    image=bytearray(size*size*4)
    depth=[-1e10]*(size*size)
    yaw=.55
    def rotate(p):
        x,y,z=p
        u=x*math.cos(yaw)+z*math.sin(yaw)
        d=-x*math.sin(yaw)+z*math.cos(yaw)
        return u, y*.91-d*.42, y*.42+d*.91
    transformed=[rotate(v[:3]) for v in vertices]
    minx,maxx=min(p[0] for p in transformed),max(p[0] for p in transformed)
    miny,maxy=min(p[1] for p in transformed),max(p[1] for p in transformed)
    scale=78/max(maxx-minx,maxy-miny)
    def screen(p):
        return 48+(p[0]-(minx+maxx)/2)*scale,48-(p[1]-(miny+maxy)/2)*scale,p[2]
    for start in range(0,len(vertices),3):
        vs=vertices[start:start+3]
        ps=[screen(p) for p in transformed[start:start+3]]
        a,b,c=ps
        det=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(det)<1e-8:
            continue
        shade=max(.35,min(1.15,.7+vs[0][4]*.35-vs[0][3]*.18-vs[0][5]*.1))
        for y in range(max(0,int(min(p[1] for p in ps))),min(size,int(max(p[1] for p in ps))+2)):
            for x in range(max(0,int(min(p[0] for p in ps))),min(size,int(max(p[0] for p in ps))+2)):
                w0=((b[1]-c[1])*(x+.5-c[0])+(c[0]-b[0])*(y+.5-c[1]))/det
                w1=((c[1]-a[1])*(x+.5-c[0])+(a[0]-c[0])*(y+.5-c[1]))/det
                w2=1-w0-w1
                if min(w0,w1,w2)<0:
                    continue
                weights=(w0,w1,w2)
                z=sum(p[2]*w for p,w in zip(ps,weights))
                index=y*size+x
                if z<depth[index]:
                    continue
                depth[index]=z
                u=sum(v[6]*w for v,w in zip(vs,weights))
                v=sum(v[7]*w for v,w in zip(vs,weights))
                offset=(min(255,max(0,int(v*255)))*256+min(255,max(0,int(u*255))))*4
                image[index*4:index*4+4]=bytes([min(255,int(tex[offset+i]*shade)) for i in range(3)]+[255])
    return image

def main():
    DEST.mkdir(parents=True,exist_ok=True)
    SOURCE.mkdir(parents=True,exist_ok=True)
    entries=[]
    for name,hp,color in ROCKS+EXTRAS:
        entries.append(dict(id=name,name=name.replace('-',' ').title(),hp=hp,color=color,block=name not in ('iron-ingot','gold-ingot','diamond','stick','coal','surface-deposit'),tool='none',tier=0,durability=0,speed=1))
    for kind in ('pickaxe','axe','shovel','hoe'):
        for name,tier,durability,speed,color in TIERS:
            entries.append(dict(id=name+'-'+kind,name=name.title()+' '+kind.title(),hp=1,color=color,block=False,tool=kind,tier=tier,durability=durability,speed=speed))
    for kind in ('shears','brush'):
        entries.append(dict(id=kind,name=kind.title(),hp=1,color=(184,190,193),block=False,tool=kind,tier=2,durability=238,speed=5))
    catalog=['[Schema]','Version=1','','[Catalog]',f'Count={len(entries)}']
    manifest=[]
    for n,item in enumerate(entries):
        name=item['id']
        wood = name in ('log','plank','chest','crafting-bench')
        loose = name in ('soil','sand','gravel','snow','tilled-soil')
        stone = name in {rock[0] for rock in ROCKS} or name in ('brick','concrete','furnace')
        ore = name.endswith('-ore')
        harvest_tool = 'axe' if wood else 'shovel' if loose else 'shears' if name=='leaves' else 'pickaxe' if stone or ore else 'none'
        if name=='surface-deposit': harvest_tool='brush'
        harvest_tier = 3 if name in ('diamond-ore','gold-ore') else 2 if name=='iron-ore' else 1 if name=='coal-ore' else 0
        if name=='surface-deposit': harvest_tier=2
        harvest_drop = 'diamond' if name=='diamond-ore' else 'coal' if name=='coal-ore' else name if item['block'] else 'none'
        if name=='tilled-soil': harvest_drop='soil'
        hand_speed = 20 if wood else 15 if loose or name=='leaves' else 5
        craft_group = 'stone-material' if stone and name!='furnace' else 'none'
        vertices=toolmesh(item['tool']) if item['tool']!='none' else resource_model(name)
        if name=='surface-deposit':
            vertices=[]
            for x,z,w,h in ((-.22,-.15,.3,.22),(.12,.12,.36,.3),(.28,-.24,.22,.17),(-.27,.28,.18,.13)):
                crumb=[]
                ring_model(crumb,[(w/2*math.cos(n*math.pi/3),w/2*math.sin(n*math.pi/3)) for n in range(6)],[(0,.7),(h*.2,1),(h,.5)])
                vertices.extend((v[0]+x,v[1],v[2]+z,*v[3:]) for v in crumb)
        if not vertices:
            if name=='torch':
                box(vertices,(-.08,0,-.08),(.08,.75,.08),True)
                box(vertices,(-.13,.75,-.13),(.13,.93,.13))
            elif name=='chest':
                box(vertices,(-.5,0,-.4),(.5,.72,.4),True)
                box(vertices,(-.51,.73,-.41),(.51,.88,.41),True)
                box(vertices,(-.08,.6,-.45),(.08,.8,-.4))
            else:
                box(vertices,(-.5,0,-.5),(.5,1,.5))
        if name in ROCK_RECIPES:
            # Rocks do not share the tools' two-part handle/head atlas. Map
            # their whole tile to every face so grain proportions stay intact.
            vertices=[(*v[:6],(v[6]-.53)/.44,(v[7]-.05)/.9,*v[8:]) for v in vertices]
        tex=texture(name,item['color'],item['tool']!='none')
        binary=b'M3D1'+struct.pack('<I',len(vertices))+b''.join(struct.pack('<12f',*v) for v in vertices)
        (DEST/(name+'.m3d')).write_bytes(binary)
        png(DEST/(name+'.png'),256,256,tex)
        png(DEST/(name+'.icon.png'),96,96,icon(vertices,tex))
        obj=[]
        for v in vertices:obj.append('v '+' '.join(f'{x:.6f}' for x in v[:3]))
        for v in vertices:obj.append(f'vt {v[6]:.6f} {1-v[7]:.6f}')
        for v in vertices:obj.append('vn '+' '.join(f'{x:.6f}' for x in v[3:6]))
        for t in range(0,len(vertices),3):obj.append('f '+' '.join(f'{i}/{i}/{i}' for i in range(t+1,t+4)))
        (SOURCE/(name+'.obj')).write_text('\n'.join(obj)+'\n',encoding='utf-8')
        manifest.append(dict(id=name,kind='item',triangles=len(vertices)//3,sha256=hashlib.sha256(binary).hexdigest(),texture_sha256=hashlib.sha256((DEST/(name+'.png')).read_bytes()).hexdigest(),icon_sha256=hashlib.sha256((DEST/(name+'.icon.png')).read_bytes()).hexdigest(),source_sha256=hashlib.sha256((SOURCE/(name+'.obj')).read_bytes()).hexdigest(),source=f'assets/models/source/builder/{name}.obj',license='Original project-authored asset'))
        if name in ROCK_RECIPES:manifest[-1]['material_recipe']=recipe_manifest(name)
        repair_material='none'
        if item['tool']!='none':
            tier_name=name.split('-')[0]
            repair_material={'wood':'plank','stone':'stone-material','iron':'iron-ingot','gold':'gold-ingot','diamond':'diamond','shears':'iron-ingot','brush':'plank'}[tier_name]
        catalog += ['',f'[Item{n}]',f'Id={name}',f'Name={item["name"]}',f'Block={int(item["block"])}',f'Tool={item["tool"]}',f'Tier={item["tier"]}',f'Durability={item["durability"]}',f'HP={item["hp"]}',f'Speed={item["speed"]}',f'BlastResistance={max(1,item["hp"]*1.5)}',
                    f'HarvestTool={harvest_tool}',f'HarvestTier={harvest_tier}',f'HarvestDrop={harvest_drop}',f'HandSpeed={hand_speed}',f'CraftGroup={craft_group}',
                    f'RepairMaterial={repair_material}',f'RepairCount={int(item["tool"]!="none")}',f'RepairAmount={math.ceil(item["durability"]/4)}']
    (ROOT/'data/builder.ini').write_text('\n'.join(catalog)+'\n',encoding='utf-8')
    recipes=[('planks','none','plank',4,[('log',1)]),('sticks','none','stick',4,[('plank',2)]),
             ('bench','none','crafting-bench',1,[('plank',4)]),('chest','crafting-bench','chest',1,[('plank',8)]),
             ('furnace','crafting-bench','furnace',1,[('stone-material',8)]),
             ('smelt-iron','furnace','iron-ingot',1,[('iron-ore',1),('coal',1)]),
             ('smelt-gold','furnace','gold-ingot',1,[('gold-ore',1),('coal',1)]),
             ('torches','none','torch',4,[('stick',1),('coal',1)])]
    for kind in ('pickaxe','axe','shovel','hoe'):
        amount={'pickaxe':3,'axe':3,'shovel':1,'hoe':2}[kind]
        for name,*_ in TIERS:
            material={'wood':'plank','stone':'stone-material','iron':'iron-ingot','gold':'gold-ingot','diamond':'diamond'}[name]
            recipes.append((name+'-'+kind,'none' if name=='wood' else 'crafting-bench',name+'-'+kind,1,[(material,amount),('stick',2)]))
    recipes += [('shears','crafting-bench','shears',1,[('iron-ingot',2)]),('brush','none','brush',1,[('stick',1),('plank',1)])]
    recipe_file=['[Schema]','Version=1','','[Recipes]',f'Count={len(recipes)}']
    for n,(name,station,result,count,inputs) in enumerate(recipes):
        recipe_file += ['',f'[Recipe{n}]',f'Id={name}',f'Station={station}',f'Result={result}',f'Count={count}',f'Inputs={len(inputs)}']
        for j,(ingredient,amount) in enumerate(inputs):recipe_file += [f'Input{j}={ingredient}',f'Input{j}Count={amount}']
    (ROOT/'data/builder-recipes.ini').write_text('\n'.join(recipe_file)+'\n',encoding='utf-8')
    for name,vertices in zip(('shears-half-left','shears-half-right','shears-pivot'),shears_parts()):
        binary=b'M3D1'+struct.pack('<I',len(vertices))+b''.join(struct.pack('<12f',*v) for v in vertices)
        (DEST/(name+'.m3d')).write_bytes(binary)
        tex=texture('shears',(184,190,193),True)
        png(DEST/(name+'.png'),256,256,tex)
        png(DEST/(name+'.icon.png'),96,96,icon(vertices,tex))
        obj=[]
        for v in vertices:obj.append('v '+' '.join(f'{x:.6f}' for x in v[:3]))
        for v in vertices:obj.append(f'vt {v[6]:.6f} {1-v[7]:.6f}')
        for v in vertices:obj.append('vn '+' '.join(f'{x:.6f}' for x in v[3:6]))
        for t in range(0,len(vertices),3):obj.append('f '+' '.join(f'{i}/{i}/{i}' for i in range(t+1,t+4)))
        (SOURCE/(name+'.obj')).write_text('\n'.join(obj)+'\n',encoding='utf-8')
        manifest.append(dict(id=name,kind='component',triangles=len(vertices)//3,sha256=hashlib.sha256(binary).hexdigest(),texture_sha256=hashlib.sha256((DEST/(name+'.png')).read_bytes()).hexdigest(),icon_sha256=hashlib.sha256((DEST/(name+'.icon.png')).read_bytes()).hexdigest(),source_sha256=hashlib.sha256((SOURCE/(name+'.obj')).read_bytes()).hexdigest(),source=f'assets/models/source/builder/{name}.obj',license='Original project-authored asset'))
    (SOURCE/'manifest.json').write_text(json.dumps(dict(generator='tools/build_builder_assets.py',catalog_items=len(entries),components=3,coordinates='Y up; tools normalized to approximately one authored unit; runtime scales by bounds',assets=manifest),indent=2)+'\n',encoding='utf-8')
    print(f'Generated {len(entries)} original textured builder assets and matching rendered icons')

if __name__=='__main__':
    main()
