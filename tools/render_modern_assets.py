"""Inspect original editable GLBs with Blender's background Eevee renderer.

Run blender.exe --background --factory-startup --python tools/render_modern_assets.py
"""
from pathlib import Path
import os
import sys
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
OUT=Path(os.environ.get('MODERN_STUDIO_OUTPUT',str(ROOT/'evidence/modern-assets-20260928/studio')))
OUT.mkdir(parents=True,exist_ok=True)
NAMES=("coastal-office","terrace-apartments",
       "compact-pistol","carbine","street-lamp","twin-lamp","bench","bin",
       "bollard","bike-rack","planter","hydrant")
if "--" in sys.argv:
    requested=sys.argv[sys.argv.index("--")+1:]
    if any(name not in NAMES for name in requested):raise ValueError("Unknown preview asset")
    NAMES=tuple(requested) or NAMES

def aim(obj,point):obj.rotation_euler=(Vector(point)-obj.location).to_track_quat('-Z','Y').to_euler()

for name in NAMES:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(ROOT/f"assets/models/source/modern/{name}.glb"))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    corners=[o.matrix_world@Vector(c) for o in meshes for c in o.bound_box]
    lo=Vector([min(p[i] for p in corners) for i in range(3)])
    hi=Vector([max(p[i] for p in corners) for i in range(3)])
    scale=3.8/max(hi-lo);center=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
    for obj in meshes:
        obj.location=(obj.location-center)*scale+Vector((0,0,.035))
        obj.scale*=scale
    scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE_NEXT'
    scene.render.resolution_x=600;scene.render.resolution_y=460;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    scene.world=bpy.data.worlds.new("Studio world");scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.30,.34,.40,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.45
    bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,0))
    floor=bpy.context.object;floor.name='Studio ground'
    mat=bpy.data.materials.new('Studio ground');mat.diffuse_color=(.21,.24,.27,1)
    floor.data.materials.append(mat)
    for pos,power,size in (((4,-5,7),950,5),((-4,-1,4),600,4),((2,4,6),1400,3)):
        bpy.ops.object.light_add(type='AREA',location=pos)
        lamp=bpy.context.object;lamp.data.energy=power;lamp.data.shape='DISK';lamp.data.size=size
        aim(lamp,(0,0,1.2))
    height=(hi.z-lo.z)*scale
    bpy.ops.object.camera_add(location=(5,-7,5))
    camera=bpy.context.object;camera.data.type='ORTHO';camera.data.ortho_scale=6.6
    aim(camera,(0,0,max(.7,height*.46)))
    scene.camera=camera;scene.view_settings.view_transform='AgX'
    scene.render.filepath=str(OUT/f"{name}.png")
    bpy.ops.render.render(write_still=True)
    print(f"Modern asset studio render: {name}",flush=True)
