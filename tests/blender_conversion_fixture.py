"""Create small original conversion fixtures inside pinned Blender."""
import pathlib
import sys
import bpy

directory = pathlib.Path(sys.argv[sys.argv.index("--") + 1]).resolve()
directory.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
root = bpy.data.objects.new("FixtureRoot", None)
bpy.context.collection.objects.link(root)
root.location = (0.2, 0.3, 0.1)
mesh = bpy.data.meshes.new("FixtureGeometry")
mesh.from_pydata([(-0.3, 0, 0), (0.3, 0, 0), (0.3, 0, 1.2), (-0.3, 0, 1.2)],
                 [], [(0, 1, 2), (0, 2, 3)])
mesh.update()
obj = bpy.data.objects.new("FixtureMesh", mesh)
bpy.context.collection.objects.link(obj)
obj.scale = (1.2, 0.8, 1.1)
for name, factor in (("PrimaryUV", 1), ("SecondaryUV", 0.5)):
    uv = mesh.uv_layers.new(name=name)
    for loop in mesh.loops:
        vertex = mesh.vertices[loop.vertex_index].co
        uv.data[loop.index].uv = ((vertex.x + 0.3) / 0.6 * factor, vertex.z / 1.2 * factor)
colors = mesh.color_attributes.new(name="Color", type="FLOAT_COLOR", domain="CORNER")
for color in colors.data:
    color.color = (0.6, 0.2, 0.1, 1)
mesh.color_attributes.active_color = colors
material = bpy.data.materials.new("FixturePaint")
material.use_nodes = True
principled = material.node_tree.nodes.get("Principled BSDF")
principled.inputs["Base Color"].default_value = (0.3, 0.5, 0.7, 1)
principled.inputs["Roughness"].default_value = 0.35
principled.inputs["Metallic"].default_value = 0.4
principled.inputs["Coat Weight"].default_value = 0.25
obj.data.materials.append(material)

armature = bpy.data.armatures.new("FixtureSkeleton")
rig = bpy.data.objects.new("FixtureRig", armature)
bpy.context.collection.objects.link(rig)
rig.parent = root;obj.parent = rig
bpy.context.view_layer.objects.active = rig;rig.select_set(True)
bpy.ops.object.mode_set(mode="EDIT")
base = armature.edit_bones.new("Base");base.head = (0, 0, 0);base.tail = (0, 0, 0.6)
tip = armature.edit_bones.new("Tip");tip.head = base.tail;tip.tail = (0, 0, 1.2)
tip.parent = base;tip.use_connect = True
bpy.ops.object.mode_set(mode="OBJECT")
for name, indices in (("Base", [0, 1]), ("Tip", [2, 3])):
    group = obj.vertex_groups.new(name=name);group.add(indices, 1, "REPLACE")
modifier = obj.modifiers.new("FixtureSkin", "ARMATURE");modifier.object = rig
bone = rig.pose.bones["Tip"];bone.rotation_mode = "QUATERNION"
bone.rotation_quaternion = (1, 0, 0, 0);bone.keyframe_insert("rotation_quaternion", frame=1)
bone.rotation_quaternion = (0.9800666, 0, 0.1986693, 0)
bone.keyframe_insert("rotation_quaternion", frame=12)
rig.animation_data.action.name = "FixtureWave"
bpy.context.scene.frame_start = 1;bpy.context.scene.frame_end = 12
bpy.context.scene.frame_set(1)
obj["attachment"] = "fixture-hand"
bpy.ops.wm.save_as_mainfile(filepath=str(directory / "scene.blend"))
bpy.ops.export_scene.fbx(filepath=str(directory / "scene.fbx"),
                         object_types={"EMPTY", "ARMATURE", "MESH"},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_bones=True)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 0.01
bpy.ops.wm.save_as_mainfile(filepath=str(directory / "centimetres.blend"))
bpy.context.scene.unit_settings.scale_length = 1.0

noise = material.node_tree.nodes.new("ShaderNodeTexNoise")
material.node_tree.links.new(noise.outputs["Color"], principled.inputs["Base Color"])
bpy.ops.wm.save_as_mainfile(filepath=str(directory / "unsupported.blend"))
