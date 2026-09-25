"""Bake selected CC0 GLB scene geometry into Mini City mesh files.

The runtime keeps its own compact static mesh format; original GLBs remain under
assets/models/source so geometry, source, and licenses are reviewable.
"""

import json
import math
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/models/source"
OUTPUT = ROOT / "assets/models/baked"
COMPONENT = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def multiply(a, b):
    return [sum(a[k * 4 + row] * b[col * 4 + k] for k in range(4))
            for col in range(4) for row in range(4)]


def local_matrix(node, override=None):
    override = override or {}
    if "matrix" in node:
        return node["matrix"]
    x, y, z, w = override.get("rotation", node.get("rotation", [0, 0, 0, 1]))
    sx, sy, sz = override.get("scale", node.get("scale", [1, 1, 1]))
    tx, ty, tz = override.get("translation", node.get("translation", [0, 0, 0]))
    return [(1 - 2 * (y*y + z*z))*sx, 2*(x*y + z*w)*sx, 2*(x*z - y*w)*sx, 0,
            2*(x*y - z*w)*sy, (1 - 2*(x*x + z*z))*sy, 2*(y*z + x*w)*sy, 0,
            2*(x*z + y*w)*sz, 2*(y*z - x*w)*sz, (1 - 2*(x*x + y*y))*sz, 0,
            tx, ty, tz, 1]


def transform(m, p, direction=False):
    x, y, z = p
    result = (m[0]*x + m[4]*y + m[8]*z + (0 if direction else m[12]),
              m[1]*x + m[5]*y + m[9]*z + (0 if direction else m[13]),
              m[2]*x + m[6]*y + m[10]*z + (0 if direction else m[14]))
    if direction:
        length = math.sqrt(sum(v*v for v in result)) or 1
        return tuple(v/length for v in result)
    return result


def read_glb(path):
    data = path.read_bytes()
    magic, version, total = struct.unpack_from("<III", data)
    assert magic == 0x46546C67 and version == 2 and total == len(data), path
    json_size, json_type = struct.unpack_from("<II", data, 12)
    assert json_type == 0x4E4F534A
    document = json.loads(data[20:20+json_size])
    bin_offset = 20 + json_size
    bin_size, bin_type = struct.unpack_from("<II", data, bin_offset)
    assert bin_type == 0x004E4942
    binary = data[bin_offset+8:bin_offset+8+bin_size]
    return document, binary


def accessor(document, binary, index):
    item = document["accessors"][index]
    view = document["bufferViews"][item["bufferView"]]
    code, size = COMPONENT[item["componentType"]]
    width = WIDTH[item["type"]]
    stride = view.get("byteStride", width * size)
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    normalized = item.get("normalized", False)
    result = []
    for n in range(item["count"]):
        values = struct.unpack_from("<" + code*width, binary, start + n*stride)
        if normalized and code != "f":
            divisor = float((1 << (size * 8)) - 1)
            values = tuple(v/divisor for v in values)
        result.append(values)
    return result


def animation_pose(document, binary, animation_name, fraction):
    if not animation_name:
        return {}
    animation = next((entry for entry in document.get("animations", [])
                      if entry.get("name", "").endswith("|" + animation_name) or
                      entry.get("name", "").endswith("|Female_" + animation_name)), None)
    if animation is None:
        if animation_name != "Idle":
            return animation_pose(document, binary, "Idle", fraction)
        raise ValueError(f"Missing animation {animation_name}")
    duration = max(accessor(document, binary, sampler["input"])[-1][0]
                   for sampler in animation["samplers"])
    time = duration * fraction
    pose = {}
    for channel in animation["channels"]:
        target = channel["target"]
        if target["path"] not in ("translation", "rotation", "scale"):
            continue
        sampler = animation["samplers"][channel["sampler"]]
        times = [entry[0] for entry in accessor(document, binary, sampler["input"])]
        values = accessor(document, binary, sampler["output"])
        assert sampler.get("interpolation", "LINEAR") in ("LINEAR", "STEP")
        last = next((i for i in range(len(times)-1) if times[i+1] >= time), len(times)-1)
        next_index = min(last+1, len(times)-1)
        blend = 0 if next_index == last or sampler.get("interpolation") == "STEP" else \
            (time-times[last])/(times[next_index]-times[last])
        a, b = values[last], values[next_index]
        if target["path"] == "rotation" and sum(x*y for x, y in zip(a, b)) < 0:
            b = tuple(-x for x in b)
        result = [a[i]*(1-blend)+b[i]*blend for i in range(len(a))]
        if target["path"] == "rotation":
            length = math.sqrt(sum(x*x for x in result)) or 1
            result = [x/length for x in result]
        pose.setdefault(target["node"], {})[target["path"]] = result
    return pose


def convert(path, animation_name=None, fraction=0, suffix=""):
    document, binary = read_glb(path)
    vertices = []
    textures = document.get("textures", [])
    materials = document.get("materials", [])
    image_index = None
    textured_triangles = 0
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    pose = animation_pose(document, binary, animation_name, fraction)
    globals_by_node = [None] * len(document["nodes"])

    def build_transforms(node_index, parent):
        node = document["nodes"][node_index]
        global_matrix = multiply(parent, local_matrix(node, pose.get(node_index)))
        globals_by_node[node_index] = global_matrix
        for child in node.get("children", []):
            build_transforms(child, global_matrix)

    scene = document["scenes"][document.get("scene", 0)]
    for node in scene["nodes"]:
        build_transforms(node, identity)

    def visit(node_index):
        nonlocal image_index, textured_triangles
        node = document["nodes"][node_index]
        matrix = globals_by_node[node_index]
        if "mesh" in node:
            joint_matrices = None
            if "skin" in node:
                skin = document["skins"][node["skin"]]
                inverses = accessor(document, binary, skin["inverseBindMatrices"])
                joint_matrices = [multiply(globals_by_node[joint], inverse)
                                  for joint, inverse in zip(skin["joints"], inverses)]
            for primitive in document["meshes"][node["mesh"]]["primitives"]:
                if primitive.get("mode", 4) != 4:
                    continue
                attributes = primitive["attributes"]
                positions = accessor(document, binary, attributes["POSITION"])
                normals = accessor(document, binary, attributes["NORMAL"]) if "NORMAL" in attributes else [(0, 1, 0)]*len(positions)
                uvs = accessor(document, binary, attributes["TEXCOORD_0"]) if "TEXCOORD_0" in attributes else [(0, 0)]*len(positions)
                colors = accessor(document, binary, attributes["COLOR_0"]) if "COLOR_0" in attributes else [(1, 1, 1, 1)]*len(positions)
                joints = accessor(document, binary, attributes["JOINTS_0"]) if joint_matrices and "JOINTS_0" in attributes else None
                weights = accessor(document, binary, attributes["WEIGHTS_0"]) if joints and "WEIGHTS_0" in attributes else None
                indices = [entry[0] for entry in accessor(document, binary, primitive["indices"])] if "indices" in primitive else list(range(len(positions)))
                material = materials[primitive.get("material", 0)] if materials else {}
                pbr = material.get("pbrMetallicRoughness", {})
                base_color = pbr.get("baseColorFactor", [1, 1, 1, 1])
                texture = pbr.get("baseColorTexture")
                if texture is not None:
                    candidate = textures[texture["index"]]["source"]
                    if image_index is None:
                        image_index = candidate
                    assert image_index == candidate, f"Multiple textures in {path}"
                    textured_triangles += len(indices)//3
                for element in indices:
                    color = colors[element]
                    if len(color) == 3:
                        color = (*color, 1)
                    r, g, b, a = (color[k]*base_color[k] for k in range(4))
                    if joints and weights:
                        positions_baked = [transform(joint_matrices[int(j)], positions[element])
                                           for j in joints[element]]
                        normals_baked = [transform(joint_matrices[int(j)], normals[element], True)
                                         for j in joints[element]]
                        position = tuple(sum(weights[element][k]*positions_baked[k][axis]
                                             for k in range(len(weights[element]))) for axis in range(3))
                        normal = tuple(sum(weights[element][k]*normals_baked[k][axis]
                                           for k in range(len(weights[element]))) for axis in range(3))
                        length = math.sqrt(sum(v*v for v in normal)) or 1
                        normal = tuple(v/length for v in normal)
                    else:
                        position = transform(matrix, positions[element])
                        normal = transform(matrix, normals[element], True)
                    u, v = uvs[element]
                    vertices.append((*position, *normal, u, 1-v, r, g, b, a))
        for child in node.get("children", []):
            visit(child)

    for node in scene["nodes"]:
        visit(node)
    if not vertices:
        raise ValueError(f"No triangles in {path}")
    output = OUTPUT / path.relative_to(SOURCE).with_name(path.stem+suffix+".m3d")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as target:
        target.write(struct.pack("<4sI", b"M3D1", len(vertices)))
        for vertex in vertices:
            target.write(struct.pack("<12f", *vertex))
    if image_index is not None:
        image = document["images"][image_index]
        if "bufferView" in image:
            view = document["bufferViews"][image["bufferView"]]
            content = binary[view.get("byteOffset", 0):view.get("byteOffset", 0)+view["byteLength"]]
            extension = ".png" if image["mimeType"] == "image/png" else ".jpg"
        else:
            image_path = path.parent / image["uri"]
            content = image_path.read_bytes()
            extension = image_path.suffix
        output.with_suffix(extension).write_bytes(content)
    points = list(zip(*(vertex[:3] for vertex in vertices)))
    bounds = [(round(min(axis), 3), round(max(axis), 3)) for axis in points]
    print(f"{path.name}: {len(vertices)//3} triangles, texture={image_index is not None}, bounds={bounds}, output={output.name}")


def convert_skin(path):
    """Export bind vertices and sampled joint palettes for runtime skinning."""
    document, binary = read_glb(path)
    skin_groups = []
    skin_offsets = []
    for skin in document["skins"]:
        group = (skin["joints"], accessor(document, binary, skin["inverseBindMatrices"]))
        try:
            offset = next(offset for known, offset in skin_groups if known == group)
        except StopIteration:
            offset = sum(len(known[0]) for known, _ in skin_groups)
            skin_groups.append((group, offset))
        skin_offsets.append(offset)
    joint_count = sum(len(group[0]) for group, _ in skin_groups)
    assert joint_count <= 255
    def ragdoll_part(name):
        if "Head" in name or "Neck" in name:
            return 1
        if name.endswith(".L"):
            return 4 if any(word in name for word in ("Leg", "Foot", "PT")) else 2
        if name.endswith(".R"):
            return 5 if any(word in name for word in ("Leg", "Foot", "PT")) else 3
        return 0
    joint_parts = [ragdoll_part(document["nodes"][joint].get("name", ""))
                   for (group, _) in skin_groups for joint in group[0]]
    vertices = []
    materials = document.get("materials", [])
    for node in document["nodes"]:
        if "mesh" not in node or "skin" not in node:
            continue
        joint_offset = skin_offsets[node["skin"]]
        for primitive in document["meshes"][node["mesh"]]["primitives"]:
            if primitive.get("mode", 4) != 4:
                continue
            attributes = primitive["attributes"]
            positions = accessor(document, binary, attributes["POSITION"])
            normals = accessor(document, binary, attributes["NORMAL"])
            uvs = accessor(document, binary, attributes["TEXCOORD_0"]) if "TEXCOORD_0" in attributes else [(0, 0)]*len(positions)
            colors = accessor(document, binary, attributes["COLOR_0"]) if "COLOR_0" in attributes else [(1, 1, 1, 1)]*len(positions)
            vertex_joints = accessor(document, binary, attributes["JOINTS_0"])
            weights = accessor(document, binary, attributes["WEIGHTS_0"])
            indices = [entry[0] for entry in accessor(document, binary, primitive["indices"])] if "indices" in primitive else range(len(positions))
            material = materials[primitive.get("material", 0)] if materials else {}
            base = material.get("pbrMetallicRoughness", {}).get("baseColorFactor", [1, 1, 1, 1])
            for index in indices:
                color = colors[index]
                if len(color) == 3:
                    color = (*color, 1)
                uv = uvs[index]
                vertex = (*positions[index], *normals[index], uv[0], 1-uv[1],
                          *(color[i]*base[i] for i in range(4)))
                vertices.append((vertex, [int(j)+joint_offset for j in vertex_joints[index]], weights[index]))
    names = [("Idle", "Idle"), ("Walk", "Walk"), ("Run", "Run"),
             ("Aim", "Idle_Gun_Pointing"), ("Hit", "HitRecieve"), ("Death", "Death"),
             ("Fire", "Gun_Shoot"), ("Reload", "Interact")]
    female = path.stem == "casual-woman"
    names += [("Fall", "RunningJump" if female else "HitRecieve_2"),
              ("Enter", "Sitting" if female else "Interact")]
    hips = next((i for i, node in enumerate(document["nodes"])
                 if node.get("name") == "Hips"), None)
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    clips = []
    for label, animation_name in names:
        animation = next((a for a in document.get("animations", [])
                          if a.get("name", "").endswith("|"+animation_name) or
                          a.get("name", "").endswith("|Female_"+animation_name)), None)
        if animation is None:
            animation_name = "Idle"
            animation = next(a for a in document["animations"]
                             if a.get("name", "").endswith("|Idle") or
                             a.get("name", "").endswith("|Female_Idle"))
        duration = max(accessor(document, binary, sampler["input"])[-1][0]
                       for sampler in animation["samplers"])
        frames = []
        attachments = []
        for frame in range(16):
            pose = animation_pose(document, binary, animation_name, frame/16)
            if label == "Enter" and not female and hips is not None:
                # The source interaction gesture has no seated finish. Lower
                # the hips as the character reaches the vehicle doorway.
                base = pose.get(hips, {}).get("translation",
                    document["nodes"][hips].get("translation", [0, 0, 0]))
                translation = list(base)
                fraction = frame/15
                translation[1] -= 0.28*fraction*fraction*(3-2*fraction)
                pose.setdefault(hips, {})["translation"] = translation
            globals_by_node = [None]*len(document["nodes"])
            def build(node_index, parent):
                matrix = multiply(parent, local_matrix(document["nodes"][node_index], pose.get(node_index)))
                globals_by_node[node_index] = matrix
                for child in document["nodes"][node_index].get("children", []):
                    build(child, matrix)
            for root in document["scenes"][document.get("scene", 0)]["nodes"]:
                build(root, identity)
            frames.append([multiply(globals_by_node[joint], inverse)
                           for (group, _) in skin_groups
                           for joint, inverse in zip(group[0], group[1])])
            hand = next((i for i, node in enumerate(document["nodes"])
                         if node.get("name", "").endswith("Wrist.R")), None)
            matrix = globals_by_node[hand] if hand is not None else identity
            attachments.append((matrix[12], matrix[13], matrix[14]))
        clips.append((label, duration, frames, attachments))
    output = OUTPUT / path.relative_to(SOURCE).with_suffix(".m3s")
    with output.open("wb") as target:
        target.write(struct.pack("<4sIII", b"M3S3", len(vertices), joint_count, len(clips)))
        for vertex, vertex_joints, weights in vertices:
            target.write(struct.pack("<12f4B4f", *vertex, *(int(j) for j in vertex_joints), *weights))
        target.write(bytes(joint_parts))
        for label, duration, frames, attachments in clips:
            target.write(struct.pack("<16sIf", label.encode("ascii"), len(frames), duration))
            for frame in frames:
                for matrix in frame:
                    target.write(struct.pack("<16f", *matrix))
            for position in attachments:
                target.write(struct.pack("<3f", *position))
    print(f"{path.name}: skinned vertices={len(vertices)}, joints={joint_count}, clips={len(clips)}, output={output.name}")


if __name__ == "__main__":
    files = list(SOURCE.rglob("*.glb")) if len(sys.argv) == 1 else list(map(pathlib.Path, sys.argv[1:]))
    for filename in files:
        filename = filename.resolve()
        if filename.parent.name == "characters":
            convert_skin(filename)
            convert(filename, "Idle")
            convert(filename, "Idle_Gun_Pointing", suffix="-aim")
            for frame in range(4):
                convert(filename, "Walk", frame/4, f"-walk{frame}")
                convert(filename, "Run", frame/4, f"-run{frame}")
        else:
            convert(filename)
