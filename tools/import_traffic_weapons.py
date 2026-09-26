"""Bake the selected CC0 OBJ/GLTF/GLB traffic and weapon meshes for MiniCity3D.

Source files live in assets/models/source; the game loads compact M3D1 files and
adjacent texture maps from assets/models/baked. Run with the bundled Python 3.
"""

import base64
import json
import math
import pathlib
import shutil
import struct

import convert_assets as gltf

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/models/source"
OUTPUT = ROOT / "assets/models/baked"


def write_mesh(path, vertices):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as target:
        target.write(struct.pack("<4sI", b"M3D1", len(vertices)))
        for vertex in vertices:
            target.write(struct.pack("<12f", *vertex))
    axes = list(zip(*(v[:3] for v in vertices)))
    print(path.relative_to(ROOT), len(vertices) // 3, "triangles",
          [(round(min(axis), 3), round(max(axis), 3)) for axis in axes])


def convert_obj(folder, stem, texture, output):
    positions, normals, uvs, vertices = [], [], [], []
    path = SOURCE / "ggbot-cars" / folder / (stem + ".obj")
    for line in path.read_text().splitlines():
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            positions.append(tuple(map(float, parts[1:4])))
        elif parts[0] == "vn":
            normals.append(tuple(map(float, parts[1:4])))
        elif parts[0] == "vt":
            uvs.append(tuple(map(float, parts[1:3])))
        elif parts[0] == "f":
            corners = [part.split("/") for part in parts[1:]]
            for i in range(1, len(corners)-1):
                for corner in (corners[0], corners[i], corners[i+1]):
                    p = positions[int(corner[0])-1]
                    uv = uvs[int(corner[1])-1] if len(corner)>1 and corner[1] else (0, 0)
                    n = normals[int(corner[2])-1] if len(corner)>2 and corner[2] else (0, 1, 0)
                    # Source +Z faces the nose, matching the game's vehicle transform.
                    vertices.append((p[0], p[1], p[2], n[0], n[1], n[2],
                                     uv[0], 1-uv[1], 1, 1, 1, 1))
    destination = OUTPUT / "vehicles" / (output + ".m3d")
    write_mesh(destination, vertices)
    shutil.copyfile(path.parent / texture, destination.with_suffix(".png"))


def read_scene(path):
    if path.suffix == ".glb":
        return gltf.read_glb(path)
    document = json.loads(path.read_text())
    uri = document["buffers"][0]["uri"]
    if uri.startswith("data:"):
        binary = base64.b64decode(uri.split(",", 1)[1])
    else:
        binary = (path.parent / uri).read_bytes()
    return document, binary


def embedded_image(document, binary, image_index, output):
    image = document["images"][image_index]
    if "bufferView" in image:
        view = document["bufferViews"][image["bufferView"]]
        start = view.get("byteOffset", 0)
        content = binary[start:start+view["byteLength"]]
        extension = ".png" if image.get("mimeType") == "image/png" else ".jpg"
    else:
        uri = image["uri"]
        if uri.startswith("data:"):
            content = base64.b64decode(uri.split(",", 1)[1])
            extension = ".png" if "image/png" in uri[:40] else ".jpg"
        else:
            content = (output.parent / uri).read_bytes()
            extension = pathlib.Path(uri).suffix
    name = output.stem + "_image" + str(image_index) + extension
    (output.parent / name).write_bytes(content)
    return name


def convert_gun(source_name, filename, output_name, excluded=()):
    path = SOURCE / source_name / filename
    document, binary = read_scene(path)
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    groups = {}

    def visit(index, parent):
        node = document["nodes"][index]
        matrix = gltf.multiply(parent, gltf.local_matrix(node))
        if "mesh" in node and node.get("name", "") not in excluded:
            for primitive in document["meshes"][node["mesh"]]["primitives"]:
                if primitive.get("mode", 4) != 4:
                    continue
                attr = primitive["attributes"]
                positions = gltf.accessor(document, binary, attr["POSITION"])
                normals = gltf.accessor(document, binary, attr["NORMAL"]) if "NORMAL" in attr else [(0,1,0)]*len(positions)
                uvs = gltf.accessor(document, binary, attr["TEXCOORD_0"]) if "TEXCOORD_0" in attr else [(0,0)]*len(positions)
                indices = [v[0] for v in gltf.accessor(document, binary, primitive["indices"])] if "indices" in primitive else range(len(positions))
                material = primitive.get("material", 0)
                group = groups.setdefault(material, [])
                for element in indices:
                    p = gltf.transform(matrix, positions[element])
                    n = gltf.transform(matrix, normals[element], True)
                    u, v = uvs[element]
                    group.append((*p, *n, u, 1-v, 1, 1, 1, 1))
        for child in node.get("children", []):
            visit(child, matrix)

    for index in document["scenes"][document.get("scene", 0)]["nodes"]:
        visit(index, identity)
    all_vertices = [v for group in groups.values() for v in group]
    if not all_vertices:
        raise ValueError("No gun triangles: " + str(path))
    axes = list(zip(*(v[:3] for v in all_vertices)))
    lengths = [max(axis)-min(axis) for axis in axes]
    long_axis = lengths.index(max(lengths))
    center = [(max(axis)+min(axis))*0.5 for axis in axes]
    def orient(p, direction=False):
        x, y, z = [p[i]-(0 if direction else center[i]) for i in range(3)]
        if source_name == "pistol":
            # This asset's barrel runs along -Y, while Z describes the grip.
            return x, z, -y
        if long_axis == 0:
            return -z, y, x
        if long_axis == 1:
            return x, -z, y
        return x, y, z
    # Prefer the thin end (the muzzle) at positive Z. Preserve triangle winding.
    low = high = low_count = high_count = 0
    for vertex in all_vertices:
        p = orient(vertex[:3])
        if p[2] < -lengths[long_axis]*0.38:
            low += p[0]*p[0]+p[1]*p[1]; low_count += 1
        if p[2] > lengths[long_axis]*0.38:
            high += p[0]*p[0]+p[1]*p[1]; high_count += 1
    reverse = source_name != "pistol" and low_count and high_count and low/low_count < high/high_count
    vertices, ranges = [], []
    for material_index, group in groups.items():
        start = len(vertices)
        for i in range(0, len(group), 3):
            triangle = []
            for v in group[i:i+3]:
                p = orient(v[:3]); n = orient(v[3:6], True)
                if reverse:
                    p = (-p[0], p[1], -p[2]); n = (-n[0], n[1], -n[2])
                triangle.append((*p, *n, *v[6:]))
            vertices.extend(triangle)
        ranges.append((material_index, start, len(vertices)-start))
    destination = OUTPUT / "weapons" / (output_name + ".m3d")
    write_mesh(destination, vertices)
    image_cache = {}
    def texture_name(info):
        if info is None:
            return "-"
        image_index = document["textures"][info["index"]]["source"]
        if image_index not in image_cache:
            image_cache[image_index] = embedded_image(document, binary, image_index, destination)
        return image_cache[image_index]
    with destination.with_suffix(".pbr").open("w") as target:
        for material_index, start, count in ranges:
            material = document.get("materials", [{}])[material_index]
            pbr = material.get("pbrMetallicRoughness", {})
            base = texture_name(pbr.get("baseColorTexture"))
            normal = texture_name(material.get("normalTexture"))
            target.write(f"{start} {count} {pbr.get('roughnessFactor',0.75)} "
                         f"{pbr.get('metallicFactor',0.15)} 0 {base} {normal} - - -\n")


if __name__ == "__main__":
    for number, texture in ((1,"car_blue.png"),(2,"car2_red.png"),
                            (3,"car3_yellow.png"),(4,"car4_lightorange.png"),
                            (5,"car5_green.png")):
        convert_obj(f"Car {number:02d}", "Car" if number==1 else f"Car{number}",
                    texture, f"traffic-{number}")
    convert_gun("pistol", "Pistol.gltf", "pistol")
    convert_gun("ak", "AK.gltf", "ak", excluded=("Bullet","BulletBox","BulletFired"))
    convert_gun("lightning", "lightning.glb", "lightning")
