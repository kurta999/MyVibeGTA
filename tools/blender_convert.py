"""Run in pinned Blender: --background --factory-startup --python this.py -- ...

FBX and authored .blend scenes become canonical glTF 2.0 GLB files. This is
conversion only; runtime cooking and shader-effect baking are separate stages.
Existing glTF sources should enter the cooker directly to retain their indices.
"""
import argparse
import hashlib
import json
import pathlib
import struct
import sys

import bpy

PINNED_VERSION = (4, 5, 14)
EXPORT_SETTINGS = dict(
    export_format="GLB", export_yup=True, export_apply=False,
    export_texcoords=True, export_normals=True, export_tangents=True,
    export_materials="EXPORT", export_vertex_color="ACTIVE",
    export_all_vertex_colors=True, export_attributes=True, export_extras=True,
    export_skins=True, export_all_influences=True, export_def_bones=False,
    export_hierarchy_flatten_bones=False, export_hierarchy_flatten_objs=False,
    export_armature_object_remove=False, export_leaf_bone=False,
    export_animations=True, export_animation_mode="ACTIONS",
    export_force_sampling=True, export_frame_step=1,
    export_optimize_animation_size=False, export_anim_slide_to_zero=False,
    export_morph=True, export_morph_normal=True, export_morph_tangent=True,
    export_morph_animation=True, export_cameras=True, export_lights=True,
    export_image_format="AUTO", export_image_quality=100,
    export_draco_mesh_compression_enable=False,
)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def glb_document(path):
    data = path.read_bytes()
    magic, version, total, length, kind = struct.unpack_from("<5I", data)
    if magic != 0x46546C67 or version != 2 or total != len(data) or kind != 0x4E4F534A:
        raise ValueError("Invalid exported glTF 2.0 GLB")
    return json.loads(data[20:20 + length])


def scene_inventory():
    return {
        "objects": [{"name": obj.name, "type": obj.type,
                     "parent": obj.parent.name if obj.parent else None,
                     "matrix_local": [list(row) for row in obj.matrix_local],
                     "vertices": len(obj.data.vertices) if obj.type == "MESH" else None,
                     "uv_sets": [layer.name for layer in obj.data.uv_layers] if obj.type == "MESH" else [],
                     "color_sets": [layer.name for layer in obj.data.color_attributes] if obj.type == "MESH" else [],
                     "bones": [bone.name for bone in obj.data.bones] if obj.type == "ARMATURE" else []}
                    for obj in bpy.context.scene.objects],
        "actions": [action.name for action in bpy.data.actions],
        "materials": [material.name for material in bpy.data.materials],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--source-url", required=True)
    parser.add_argument("--license", required=True)
    parser.add_argument("--credit", required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
    if bpy.app.version != PINNED_VERSION:
        raise RuntimeError(f"Expected Blender {PINNED_VERSION}; got {bpy.app.version}")
    source, output = args.input.resolve(), args.output.resolve()
    if source == output or output.suffix.lower() != ".glb":
        raise ValueError("Conversion requires a separate .glb output")
    output.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if source.suffix.lower() == ".fbx":
        bpy.ops.import_scene.fbx(filepath=str(source), use_anim=True,
                                 use_custom_props=True, use_manual_orientation=False,
                                 global_scale=1.0, bake_space_transform=False)
    elif source.suffix.lower() == ".blend":
        bpy.ops.wm.open_mainfile(filepath=str(source), load_ui=False, use_scripts=False)
    else:
        raise ValueError("Use FBX or an authored .blend; existing glTF goes directly to the cooker")
    imported = scene_inventory()
    unit_scale = bpy.context.scene.unit_settings.scale_length
    if bpy.context.scene.unit_settings.system == "NONE":
        unit_scale = 1.0
    if unit_scale != 1.0:
        normalization = bpy.data.objects.new("CanonicalMetreScale", None)
        bpy.context.scene.collection.objects.link(normalization)
        for obj in list(bpy.context.scene.objects):
            if obj != normalization and obj.parent is None:
                obj.parent = normalization
        normalization.scale = (unit_scale,) * 3
    report = {
        "schema": "MiniCityCanonicalConversion/1", "source": str(source),
        "source_sha256": sha256(source), "source_url": args.source_url,
        "license": args.license, "credit": args.credit,
        "blender_version": bpy.app.version_string,
        "blender_build_hash": bpy.app.build_hash.decode(),
        "units": "glTF metres; FBX embedded axes/units imported once by Blender",
        "metre_scale": unit_scale,
        "export_settings": EXPORT_SETTINGS, "imported_scene": imported,
        "image_dependencies": [], "unsupported": [],
        "conversion_changes": [
            "FBX/.blend is converted to indexed glTF primitives; source polygon identity is not retained",
            "Animations are sampled each source scene frame into local glTF TRS channels",
            "The runtime cooker must validate all exported extensions, influences, and UV sets",
        ],
    }
    for image in bpy.data.images:
        if image.source != "FILE":
            if image.source not in {"GENERATED"}:
                report["unsupported"].append(f"Image {image.name}: {image.source}")
            continue
        path = pathlib.Path(bpy.path.abspath(image.filepath)).resolve()
        if not path.is_file() and not image.packed_file:
            report["unsupported"].append(f"Missing image: {path}")
        elif path.is_file():
            report["image_dependencies"].append({"path": str(path), "sha256": sha256(path)})
    for obj in bpy.context.scene.objects:
        for modifier in obj.modifiers:
            if modifier.type != "ARMATURE" and modifier.show_render:
                report["unsupported"].append(f"{obj.name}: modifier {modifier.type}; bake required")
    supported_nodes = {"BSDF_PRINCIPLED", "OUTPUT_MATERIAL", "TEX_IMAGE", "NORMAL_MAP",
                       "TEX_COORD", "MAPPING", "SEPARATE_COLOR", "RGB", "VALUE", "REROUTE"}
    for material in bpy.data.materials:
        if material.use_nodes:
            # Check only nodes connected to the active output. Unused authoring
            # nodes do not affect the exported surface.
            outputs = [node for node in material.node_tree.nodes
                       if node.type == "OUTPUT_MATERIAL" and node.is_active_output]
            visited, pending = set(), outputs[:]
            while pending:
                node = pending.pop()
                if node in visited:
                    continue
                visited.add(node)
                if node.type not in supported_nodes:
                    report["unsupported"].append(f"{material.name}: shader node {node.type}; bake required")
                if node.type == "TEX_COORD":
                    for socket in node.outputs:
                        if socket.is_linked and socket.name != "UV":
                            report["unsupported"].append(f"{material.name}: {socket.name} texture coordinates; bake required")
                if node.type == "TEX_IMAGE" and node.projection != "FLAT":
                    report["unsupported"].append(f"{material.name}: {node.projection} image projection; bake required")
                pending.extend(link.from_node for socket in node.inputs for link in socket.links)
    report_path = output.with_suffix(".conversion.json")
    if report["unsupported"]:
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        raise RuntimeError("Source needs explicit shader/image baking; see conversion report")
    bpy.ops.export_scene.gltf(filepath=str(output), **EXPORT_SETTINGS)
    document = glb_document(output)
    primitives = [primitive for mesh in document.get("meshes", []) for primitive in mesh["primitives"]]
    report["output"] = {
        "path": str(output), "sha256": sha256(output),
        "nodes": len(document.get("nodes", [])), "meshes": len(document.get("meshes", [])),
        "skins": len(document.get("skins", [])), "animations": len(document.get("animations", [])),
        "materials": len(document.get("materials", [])),
        "extensions": document.get("extensionsUsed", []),
        "primitive_attributes": [sorted(primitive["attributes"]) for primitive in primitives],
        "all_primitives_indexed": all("indices" in primitive for primitive in primitives),
    }
    if any(obj["type"] == "ARMATURE" for obj in imported["objects"]) and not document.get("skins"):
        raise RuntimeError("Imported armature produced no skin")
    if imported["actions"] and not document.get("animations"):
        raise RuntimeError("Imported actions produced no animation")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("CANONICAL_CONVERSION_PASS", output)


if __name__ == "__main__":
    main()
