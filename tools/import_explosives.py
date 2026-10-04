"""Run in pinned Blender to bake the downloaded CC0 sticky bomb and authored throwables."""
import bpy, pathlib, struct, math, json, hashlib
from mathutils import Vector
ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/models/source/explosives'
DEST=ROOT/'assets/models/baked/weapons'
DEST.mkdir(parents=True,exist_ok=True)

def bake(name,objects):
    bpy.context.view_layer.update()
    vertices=[]
    for obj in objects:
        if obj.type!='MESH': continue
        mesh=obj.to_mesh();mesh.calc_loop_triangles()
        for tri in mesh.loop_triangles:
            material=obj.material_slots[tri.material_index].material if obj.material_slots else None
            tint=tuple(material.diffuse_color[:3]) if material else (.35,.4,.2)
            for loop_index in tri.loops:
                loop=mesh.loops[loop_index]
                point=obj.matrix_world@mesh.vertices[loop.vertex_index].co
                normal=(obj.matrix_world.to_3x3().inverted().transposed()@loop.normal).normalized()
                uv=mesh.uv_layers.active.data[loop_index].uv if mesh.uv_layers.active else (0,0)
                vertices.append([*point,*normal,uv[0],1-uv[1],*tint,1])
        obj.to_mesh_clear()
    lows=[min(v[i] for v in vertices) for i in range(3)]
    highs=[max(v[i] for v in vertices) for i in range(3)]
    scale=1/max(highs[i]-lows[i] for i in range(3))
    for v in vertices:
        for i in range(3):v[i]=(v[i]-(lows[i] if i==1 else (lows[i]+highs[i])/2))*scale
    with (DEST/(name+'.m3d')).open('wb') as f:
        f.write(struct.pack('<4sI',b'M3D1',len(vertices)))
        for v in vertices:f.write(struct.pack('<12f',*v))
    print(name,len(vertices)//3,'triangles',flush=True)
    render_icon(name,vertices)

def render_icon(name,vertices):
    # Render the same cooked geometry used by the game into a transparent icon.
    scene=bpy.context.scene
    original=list(scene.objects)
    for obj in original:obj.hide_render=True
    mesh=bpy.data.meshes.new('icon');mesh.from_pydata([v[:3] for v in vertices],[],
        [(i,i+1,i+2) for i in range(0,len(vertices),3)])
    mesh.update();obj=bpy.data.objects.new('icon',mesh);scene.collection.objects.link(obj)
    colors=mesh.color_attributes.new(name='Color',type='FLOAT_COLOR',domain='CORNER')
    uv=mesh.uv_layers.new(name='UVMap')
    for i,v in enumerate(vertices):colors.data[i].color=(*v[8:11],1);uv.data[i].uv=(v[6],1-v[7])
    mat=bpy.data.materials.new('icon-surface');mat.use_nodes=True
    nodes=mat.node_tree.nodes;bsdf=nodes.get('Principled BSDF');bsdf.inputs['Roughness'].default_value=.7
    if name=='c4':
        tex=nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(SOURCE/'sticky-bomb/Sticky Bomb.png'),check_existing=True)
        mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
    else:
        tex=nodes.new('ShaderNodeVertexColor');tex.layer_name='Color'
        mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
    obj.data.materials.append(mat)
    def aim(obj):obj.rotation_euler=(Vector((0,.48,0))-obj.location).to_track_quat('-Z','Y').to_euler()
    bpy.ops.object.camera_add(location=(1.7,1.6,2.5));camera=bpy.context.object;aim(camera)
    camera.data.type='ORTHO';camera.data.ortho_scale=1.6;scene.camera=camera
    bpy.ops.object.light_add(type='AREA',location=(2,4,3));light=bpy.context.object
    light.data.energy=450;light.data.shape='DISK';light.data.size=4;aim(light)
    scene.world=bpy.data.worlds.new('icon-world');scene.world.use_nodes=True
    scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.6
    scene.render.engine='BLENDER_EEVEE_NEXT';scene.render.film_transparent=True
    scene.render.resolution_x=scene.render.resolution_y=160;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
    output=ROOT/'assets/icons/weapons';output.mkdir(parents=True,exist_ok=True)
    scene.render.filepath=str(output/(name+'.png'));bpy.ops.render.render(write_still=True)
    for added in (obj,camera,light):bpy.data.objects.remove(added,do_unlink=True)
    for obj in original:obj.hide_render=False

bpy.ops.wm.open_mainfile(filepath=str(SOURCE/'sticky-bomb/Sticky Bomb.blend'))
objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
print('Source meshes',[(o.name,len(o.data.polygons)) for o in objects],flush=True)
bake('c4',objects)
# Preserve original UVs and use the supplied image rather than old Blender Internal materials.
import shutil
shutil.copyfile(SOURCE/'sticky-bomb/Sticky Bomb.png',DEST/'c4.png')

def material(name,color):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);return m
def cube(location,scale,color):
    bpy.ops.mesh.primitive_cube_add(size=1,location=location)
    o=bpy.context.object;o.scale=scale;o.data.materials.append(material('surface',color));return o
def sphere(location,scale,color):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=8,location=location)
    o=bpy.context.object;o.scale=scale;o.data.materials.append(material('surface',color));return o
def cylinder(location,radius,depth,color):
    bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=radius,depth=depth,location=location)
    o=bpy.context.object;o.rotation_euler.x=math.pi/2;o.data.materials.append(material('surface',color));return o
for name,color in [('grenade',(.25,.31,.13)),('smoke-grenade',(.62,.67,.61)),('flashbang',(.28,.31,.33)),
                   ('molotov',(.16,.35,.17)),('remote-trigger',(.1,.12,.13)),('timed-bomb',(.2,.23,.16))]:
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    if name=='molotov':
        sphere((0,.42,0),(.22,.4,.22),color);cylinder((0,.94,0),.07,.35,color)
        cube((.04,1.16,0),(.06,.22,.1),(.74,.65,.41))
    elif name=='remote-trigger':
        cube((0,.3,0),(.5,.6,.15),color);cube((0,.52,-.09),(.2,.18,.05),(.8,.08,.04))
        cylinder((.15,.82,0),.018,.5,(.2,.2,.2))
    elif name=='timed-bomb':
        cube((0,.2,0),(1,.4,.7),color);cube((0,.44,0),(.5,.08,.4),(.07,.08,.06))
        cube((0,.5,0),(.36,.04,.15),(.84,.15,.05))
    else:
        sphere((0,.38,0),(.3,.38,.3),color) if name=='grenade' else cylinder((0,.4,0),.25,.8,color)
        cube((0,.84,0),(.18,.12,.15),(.16,.17,.16));cube((.16,.7,0),(.08,.46,.1),(.48,.48,.43))
    bake(name,list(bpy.context.scene.objects))

archive=SOURCE/'sticky-bomb.zip'
(SOURCE/'manifest.json').write_text(json.dumps({'title':'Low poly Sticky-Bomb','author':'Lucian Pavel',
    'license':'CC0-1.0','source':'https://opengameart.org/content/low-poly-sticky-bomb',
    'download':'https://opengameart.org/sites/default/files/Sticky%20Bomb.zip',
    'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),
    'modifications':'Normalized mesh, preserved UVs and supplied texture; cooked to M3D1.',
    'other_meshes':'Original procedural throwable, remote and timed-bomb models.'},indent=2))
