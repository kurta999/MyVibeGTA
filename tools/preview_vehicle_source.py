"""Inspect and render an authored source mesh; never executes embedded scripts."""
import bpy, pathlib, sys, json, math
from mathutils import Vector

args=sys.argv[sys.argv.index('--')+1:]
path=pathlib.Path(args[0]).resolve()
out=pathlib.Path(args[1]).resolve()
if path.suffix=='.blend':
    bpy.ops.wm.open_mainfile(filepath=str(path),load_ui=False,use_scripts=False)
else:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if path.suffix=='.obj':bpy.ops.wm.obj_import(filepath=str(path),forward_axis='Y',up_axis='Z')
    else:bpy.ops.import_scene.gltf(filepath=str(path))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and o.name!='Plane.005']
for o in list(bpy.context.scene.objects):
    if o not in meshes:bpy.data.objects.remove(o,do_unlink=True)
for m in bpy.data.materials:
    if not m.use_nodes:
        color=m.diffuse_color[:];m.use_nodes=True
        m.node_tree.nodes.get('Principled BSDF').inputs['Base Color'].default_value=color
        m.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value=.4
points=[o.matrix_world@Vector(c) for o in meshes for c in o.bound_box]
lo=Vector([min(p[i] for p in points) for i in range(3)])
hi=Vector([max(p[i] for p in points) for i in range(3)])
center=(lo+hi)/2;span=max(hi-lo)
print('SOURCE_MESH '+json.dumps(dict(objects=len(meshes),vertices=sum(len(o.data.vertices) for o in meshes),bounds=[list(lo),list(hi)])))
scene=bpy.context.scene
scene.render.engine='BLENDER_EEVEE_NEXT'
scene.world=bpy.data.worlds.new('Preview World');scene.world.use_nodes=True
scene.world.node_tree.nodes.get('Background').inputs[0].default_value=(.1,.12,.15,1)
scene.world.node_tree.nodes.get('Background').inputs[1].default_value=.5
for offset,power,size in [((1,-2,3),1000,2),((-2,-1,1),600,2),((0,2,2),800,1)]:
    data=bpy.data.lights.new('Preview light','AREA');data.energy=power*span*span;data.shape='DISK';data.size=size*span
    o=bpy.data.objects.new(data.name,data);scene.collection.objects.link(o)
    o.location=center+Vector(offset)*span;o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Source camera');camera=bpy.data.objects.new(data.name,data);scene.collection.objects.link(camera)
camera.location=center+Vector((1.5,-2,1.25))*span
camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
data.type='ORTHO';data.ortho_scale=span*1.55;data.clip_end=span*20;data.clip_start=.001;scene.camera=camera
scene.render.resolution_x=1000;scene.render.resolution_y=750;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(out)
bpy.ops.render.render(write_still=True)
