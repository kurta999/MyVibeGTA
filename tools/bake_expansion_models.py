"""Bake downloaded authored geometry into rigid, material-preserving vehicle parts.

Run with Blender 4.5 LTS, --background --factory-startup --disable-autoexec.
No geometry is generated. Original connected components are grouped into a
bounded number of collision/debris assemblies; all source triangles survive.
"""
import bpy, pathlib, sys, json, math, struct, hashlib
from collections import defaultdict
from mathutils import Vector, Matrix

ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/models/source/expansion'
OUT=ROOT/'assets/models/baked/vehicles'
CONFIG=[
 ('mazda',SOURCE/'mazda.glb',96,28,0,()),
 ('range-rover',SOURCE/'range-rover.glb',96,28,0,()),
 ('motorcycle',SOURCE/'motorcycle.glb',30,13,0,()),
 ('skateboard',SOURCE/'skateboard.glb',30,4,0,()),
 ('bicycle',SOURCE/'bicycle.glb',36,12,-math.pi/2,()),
 ('tractor',SOURCE/'tractor.blend',54,22,math.pi,()),
 ('combine',next(SOURCE.glob('combine-candidate/**/*.obj')),135,26,math.pi,()),
 ('tank',SOURCE/'tank.blend',110,18,0,('Cube.003',)),
 ('truck',SOURCE/'truck.blend',116,24,0,('Plane.005',)),
 ('airplane',SOURCE/'cessna.blend',104,16,math.pi,()),
 ('trailer',next(SOURCE.glob('trailer-source/**/*.blend')),108,22,0,()),
 ('helicopter',ROOT/'assets/models/source/helicopter/helicopter.glb',163,24,math.pi,()),
 ('boat',ROOT/'assets/models/source/vehicles/motorboat.glb',48,10,0,()),
]

def open_source(path):
    if path.suffix=='.blend':
        bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False,use_scripts=False)
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        if path.suffix=='.obj':
            bpy.ops.wm.obj_import(filepath=str(path),forward_axis='Y',up_axis='Z')
        else:bpy.ops.import_scene.gltf(filepath=str(path))
    bpy.context.scene.frame_set(1)
    for o in bpy.context.scene.objects:
        for m in o.modifiers:
            if m.type=='SUBSURF':m.levels=min(m.levels,2)

def components(mesh):
    parent=list(range(len(mesh.vertices)))
    def find(i):
        while parent[i]!=i:
            parent[i]=parent[parent[i]];i=parent[i]
        return i
    for e in mesh.edges:
        a,b=map(find,e.vertices)
        if a!=b:parent[b]=a
    return [find(i) for i in range(len(parent))]

def bounds(points):
    return (Vector([min(p[i] for p in points) for i in range(3)]),
            Vector([max(p[i] for p in points) for i in range(3)]))

def material_data(mat,key,number):
    color=(.55,.55,.55,1);rough=.5;metal=0;image=None
    if mat:
        color=tuple(mat.diffuse_color)
        if mat.use_nodes:
            bsdf=next((n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None)
            if bsdf:
                color=tuple(bsdf.inputs['Base Color'].default_value)
                rough=bsdf.inputs['Roughness'].default_value
                metal=bsdf.inputs['Metallic'].default_value
                socket=bsdf.inputs['Base Color']
                if socket.is_linked and socket.links[0].from_node.type=='TEX_IMAGE':
                    image=socket.links[0].from_node.image
        else:
            name=mat.name.lower()
            if any(s in name for s in ('silver','metal','chrome')):metal=.8;rough=.22
        if any(s in mat.name.lower() for s in ('glass','window')):
            color=(.12,.22,.28,1);rough=.12
    texture='-'
    if image:
        texture=f'expansion-{key}-texture-{number}.png'
        image.filepath_raw=str(OUT/texture);image.file_format='PNG';image.save()
        color=(1,1,1,1)
    return dict(color=color,rough=rough,metal=metal,texture=texture)

def cook(config,inspect=False):
    key,path,length,rest,yaw,excluded=config
    open_source(path)
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.name not in excluded]
    if key=='trailer':
        print('TRAILER_OBJECTS '+json.dumps([(o.name,list(o.dimensions),list(o.location)) for o in objects]),flush=True)
        if not inspect:objects=[o for o in objects if o.name in ('Cube','Wheels.001','Cube.001','Cube.002','Cube.004')]
    if not objects:raise RuntimeError('No authored mesh objects for '+key)
    rotate=Matrix.Rotation(yaw,4,'Z')
    evaluated=[];allpoints=[];materials={};comps=[]
    deps=bpy.context.evaluated_depsgraph_get()
    for o in objects:
        ev=o.evaluated_get(deps);m=ev.to_mesh(preserve_all_data_layers=True,depsgraph=deps)
        m.calc_loop_triangles()
        matrix=rotate@o.matrix_world
        points=[matrix@v.co for v in m.vertices]
        allpoints.extend(points)
        ids=components(m);groups=defaultdict(list)
        for i,c in enumerate(ids):groups[c].append(i)
        item=dict(object=o,ev=ev,mesh=m,matrix=matrix,points=points,ids=ids,groups=groups)
        evaluated.append(item)
        for c,indices in groups.items():
            lo,hi=bounds([points[i] for i in indices])
            comps.append((o.name,c,lo,hi,len(indices)))
    lo,hi=bounds(allpoints)
    print('SOURCE '+key+' '+json.dumps(dict(bounds=[list(lo),list(hi)],objects=[dict(name=i['object'].name,bounds=[list(v) for v in bounds(i['points'])]) for i in evaluated],
        components=[dict(object=n,id=c,bounds=[list(l),list(h)],vertices=v) for n,c,l,h,v in sorted(comps,key=lambda x:-x[4])[:25]])),flush=True)
    if inspect:
        for item in evaluated:item['ev'].to_mesh_clear()
        return
    # Blender Z up -> game Y up, source forward -Y -> game +Z.
    scale=length/(hi.y-lo.y)
    center=(lo+hi)/2
    if key=='combine':center.y=420  # body centre; the authored corn header projects forward
    def position(p):return Vector(((p.x-center.x)*scale,(p.z-lo.z)*scale-rest,-(p.y-center.y)*scale))
    def normal(n):return Vector((n.x,n.z,-n.y)).normalized()
    # The source tank is posed with its turret turned and gun elevated. Undo
    # that pose when baking, retaining separate authored joint assemblies.
    turret_objects={'Cube.001','Cylinder','Plane.002','Cube.013','Circle.000'}
    turret_pivot=position(Vector((.096639,.045987,.472497)))
    if key=='tank':
        gun=bpy.data.objects['Circle.020']
        axis=normal(gun.matrix_world.to_3x3()@Vector((0,0,1)))
        unyaw=Matrix.Rotation(-math.atan2(axis.x,axis.z),3,'Y')
        unpitch=Matrix.Rotation(math.asin(axis.y),3,'X')
        gun_pivot=turret_pivot+unyaw@(position(gun.location)-turret_pivot)
    local_components={}
    for n,c,l,h,v in comps:
        a,b=position(l),position(h)
        low=Vector((a.x,a.y,b.z));high=Vector((b.x,b.y,a.z))
        local_components[n,c]=(low,high,v)
    wheels=[]
    def wheel_name(name):
        name=name.lower()
        return ('wheel' in name and 'steering' not in name) or 'tire' in name or name.startswith('ruota')
    explicit={item['object'].name for item in evaluated if wheel_name(item['object'].name)}
    candidates=[]
    for item in evaluated:
        if item['object'].name not in explicit:continue
        a,b=bounds([position(p) for p in item['points']]);mid=(a+b)/2;size=b-a
        if size.x<size.y*.8 and .7<size.y/max(size.z,.01)<1.3 and mid.y<0 and mid.y+rest<size.y*.8:
            candidates.append((mid,size.y*.55,item['object'].name,None))
    if key!='helicopter':
        for (n,c),(l,h,verts) in local_components.items():
            if explicit and n not in explicit:continue
            size=h-l;mid=(h+l)/2
            if size.x<size.y*.7 and .7<size.y/max(size.z,.01)<1.3 and size.y>length*.055 and mid.y<0 and mid.y+rest<size.y*.8:
                candidates.append((mid,size.y*.55,n,c))
    for candidate in sorted(candidates,key=lambda c:-c[1]):
        mid,radius,n,c=candidate
        if not any((mid-w[0]).length<radius for w in wheels):wheels.append(candidate)
    wheels=wheels[:16]
    grouped=defaultdict(list);pivots={};spin={};source_triangles=0
    for item in evaluated:
        o,m,points,ids=item['object'],item['mesh'],item['points'],item['ids']
        nmat=item['matrix'].to_3x3().inverted().transposed()
        uv=m.uv_layers.active
        color_attr=m.color_attributes.active_color
        names=[slot.material.name if slot.material else '' for slot in o.material_slots]
        for tri in m.loop_triangles:
            source_triangles+=1
            comp=ids[tri.vertices[0]];l,h,verts=local_components[o.name,comp];mid=(h+l)/2
            assembly=None
            for index,(wc,radius,wn,component_id) in enumerate(wheels):
                same_object=o.name==wn
                if same_object and (mid-wc).length<radius*1.05 and max(h-l)<radius*2.05:
                    assembly=f'wheel-{index}';pivots[assembly]=wc;spin[assembly]=1;break
            if assembly is None:
                if key=='tank' and o.name in turret_objects:
                    assembly='turret';spin[assembly]=5;pivots[assembly]=turret_pivot
                elif key=='tank' and o.name=='Circle.020':
                    assembly='barrel';spin[assembly]=6;pivots[assembly]=gun_pivot
                elif key=='helicopter' and o.name in ('Box04_Box014','Box20183'):
                    assembly='rotor';spin[assembly]=2
                    pivots[assembly]=position(Vector((932.3,-172.9,235.9)))
                elif key=='airplane' and any(s in o.name.lower() for s in ('elica','pala')):
                    assembly='propeller';spin[assembly]=3
                    pivots[assembly]=position(rotate@Vector((0,1.11,1.125)))
                elif key=='combine' and mid.z>40 and mid.y>-18 and (h-l).x>40 and max((h-l).y,(h-l).z)<12:
                    assembly='cutting-auger';spin[assembly]=4
                else:
                    assembly='body-'+str((1 if mid.x>0 else 0)+(2 if mid.z>0 else 0)+(4 if mid.y>length*.18 else 0))
            mat=o.material_slots[tri.material_index].material if tri.material_index<len(o.material_slots) else None
            mk=(mat.name if mat else 'unpainted')
            if mk not in materials:materials[mk]=material_data(mat,key,len(materials))
            md=materials[mk]
            # The OBJ supplies geometry without its MTL; assign paint to retained source components.
            if key=='combine':
                md=dict(md)
                md['color']=(.025,.04,.025,1) if assembly.startswith('wheel') else (.17,.39,.035,1)
                if mid.z>40:md['color']=(.54,.48,.035,1)
            for vi,li in zip(tri.vertices,tri.loops):
                p=position(points[vi]);n=normal(nmat@m.corner_normals[li].vector)
                if key=='tank' and assembly in ('turret','barrel'):
                    p=turret_pivot+unyaw@(p-turret_pivot);n=unyaw@n
                    if assembly=='barrel':p=gun_pivot+unpitch@(p-gun_pivot);n=unpitch@n
                tex=uv.data[li].uv if uv else (0,0)
                col=list(md['color'])
                if color_attr:
                    source_col=color_attr.data[li if color_attr.domain=='CORNER' else vi].color
                    col=[a*b for a,b in zip(col,source_col)]
                grouped[assembly,mk].append(tuple(p)+tuple(n)+(tex[0],1-tex[1])+tuple(col))
                materials[mk]=md
    assemblies=sorted(set(a for a,m in grouped))
    parts=[];total=0
    for assembly in assemblies:
        groups=[(mk,vs) for (a,mk),vs in grouped.items() if a==assembly]
        vs=[v for mk,vertices in groups for v in vertices]
        l,h=bounds([Vector(v[:3]) for v in vs]);pivot=pivots.get(assembly,(l+h)/2)
        size=h-l
        part=f'expansion-{key}-{assembly}';unique=[];indices=[];lookup={};ranges=[]
        for mk,vertices in groups:
            start=len(indices)
            for v in vertices:
                packed=struct.pack('<12f',*(tuple(Vector(v[:3])-pivot)+v[3:]))
                if packed not in lookup:lookup[packed]=len(unique);unique.append(packed)
                indices.append(lookup[packed])
            md=materials[mk]
            ranges.append(f"{start} {len(indices)-start} {md['rough']:.5f} {md['metal']:.5f} 0 {md['texture']} - - - - OPAQUE 0.5")
        destination=OUT/(part+'.m3d')
        with destination.open('wb') as target:
            target.write(struct.pack('<4sII',b'M3D2',len(unique),len(indices)))
            target.write(b''.join(unique));target.write(struct.pack('<'+str(len(indices))+'I',*indices))
        destination.with_suffix('.pbr').write_text('\n'.join(ranges)+'\n')
        total+=len(indices)//3
        parts.append(dict(mesh='vehicles/'+part,center=list(pivot),size=[max(.5,v) for v in size],spin=spin.get(assembly,0)))
    assert total==source_triangles
    report=dict(source=str(path.relative_to(ROOT)),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        source_triangles=source_triangles,baked_triangles=total,excluded_objects=list(excluded),parts=parts,
        conversion='evaluated source geometry; original UVs, normals, colors and PBR base maps; rigid assemblies')
    (OUT/f'expansion-{key}.import.json').write_text(json.dumps(report,indent=2)+'\n')
    for item in evaluated:item['ev'].to_mesh_clear()
    print('BAKED '+key+' '+str(total)+' triangles, '+str(len(parts))+' rigid parts',flush=True)
    return parts

if __name__=='__main__':
    args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
    OUT.mkdir(parents=True,exist_ok=True)
    inspect='--inspect' in args
    selected=[c for c in CONFIG if not args or inspect or c[0] in args]
    catalog={}
    for config in selected:catalog[config[0]]=cook(config,inspect)
    if not inspect:
        path=ROOT/'data/vehicle-models.ini'
        old={}
        if path.exists():
            import configparser
            ini=configparser.ConfigParser();ini.optionxform=str;ini.read(path)
            for s in ini.sections():old[s]=dict(ini[s])
        for key,parts in catalog.items():
            old[key]={'Count':str(len(parts))}
            if key=='tank':
                gun=next(p for p in parts if p['spin']==6)
                raw=(OUT/(gun['mesh'].split('/')[-1]+'.m3d')).read_bytes()
                count=struct.unpack_from('<I',raw,4)[0]
                old[key]['GunLength']=f'{max(struct.unpack_from("<3f",raw,12+i*48)[2] for i in range(count)):.6f}'
            for i,p in enumerate(parts):
                old[key][f'Part{i}']=' '.join([p['mesh']]+[f'{v:.6f}' for v in p['center']+p['size']]+[str(p['spin'])])
        path.write_text('; Authored mesh assemblies; spin: 0 body, 1 wheel, 2/3/4 rotor, 5 turret, 6 gun\n'+
            '\n'.join('['+s+']\n'+'\n'.join(k+'='+v for k,v in fields.items())+'\n' for s,fields in old.items()))
