"""Bake the selected CC0 OBJ/GLTF/GLB traffic and weapon meshes for MiniCity3D.

Source files live in assets/models/source; the game loads indexed M3D2 files and
adjacent texture maps from assets/models/baked. Run with the bundled Python 3.
"""

import argparse
import base64
import hashlib
import json
import math
import pathlib
import shutil
import struct

import convert_assets as gltf

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/models/source"
OUTPUT = ROOT / "assets/models/baked"


def write_mesh(path, vertices, source_keys=None):
    path.parent.mkdir(parents=True, exist_ok=True)
    unique, indices, lookup = [], [], {}
    if source_keys is not None and len(source_keys) != len(vertices):
        raise ValueError("Source index count does not match vertex count")
    for offset, vertex in enumerate(vertices):
        packed = struct.pack("<12f", *vertex)
        # A glTF vertex belongs to its primitive, even when another primitive
        # happens to have identical attributes. Keep that identity for material
        # sections and later tangent/skin data instead of merging by float bits.
        key = source_keys[offset] if source_keys is not None else packed
        index = lookup.get(key)
        if index is None:
            index = len(unique)
            lookup[key] = index
            unique.append(packed)
        elif unique[index] != packed:
            raise ValueError(f"Source vertex has inconsistent attributes: {path}")
        indices.append(index)
    if not indices or len(indices) % 3 or len(unique) > 3_000_000:
        raise ValueError(f"Invalid indexed mesh: {path}")
    with path.open("wb") as target:
        target.write(struct.pack("<4sII", b"M3D2", len(unique), len(indices)))
        for vertex in unique:
            target.write(vertex)
        for index in indices:
            target.write(struct.pack("<I", index))
    axes = list(zip(*(v[:3] for v in vertices)))
    print(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path,
          len(vertices) // 3, "triangles",
          len(unique), "unique vertices",
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
    if len(document.get("buffers", [])) != 1:
        raise ValueError(f"Expected exactly one glTF buffer in {path}")
    uri = document["buffers"][0]["uri"]
    if uri.startswith("data:"):
        binary = base64.b64decode(uri.split(",", 1)[1])
    else:
        binary = (path.parent / uri).read_bytes()
    return document, binary


def transformed_normal(matrix, normal):
    """Apply the inverse transpose of a glTF node's linear transform."""
    a, b, c = matrix[0:3], matrix[4:7], matrix[8:11]
    cross = lambda u, v: (u[1]*v[2]-u[2]*v[1],
                          u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
    x, y, z = cross(b, c), cross(c, a), cross(a, b)
    determinant = sum(a[i]*x[i] for i in range(3))
    if abs(determinant) < 1e-10:
        raise ValueError("Singular glTF node transform")
    result = tuple((normal[0]*x[i]+normal[1]*y[i]+normal[2]*z[i]) /
                   determinant for i in range(3))
    length = math.sqrt(sum(component*component for component in result))
    if length < 1e-10:
        raise ValueError("Zero-length glTF normal")
    return tuple(component/length for component in result)


def embedded_image(document, binary, image_index, source, output):
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
            content = (source.parent / uri).read_bytes()
            extension = pathlib.Path(uri).suffix
    name = output.stem + "_image" + str(image_index) + extension
    (output.parent / name).write_bytes(content)
    return name


def conversion_report(document, source, attributes):
    """Describe every static-weapon conversion loss in machine-readable form."""
    retained = {"POSITION", "NORMAL", "TEXCOORD_0", "COLOR_0"}
    missing = sorted(attributes - retained)
    animations = [animation.get("name") or f"animation-{index}"
                  for index, animation in enumerate(document.get("animations", []))]
    report = {
        "schema": 1,
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "source_attributes": sorted(attributes),
        "retained_attributes": sorted(attributes & retained),
        "discarded_attributes": missing,
        "discarded_skins": len(document.get("skins", [])),
        "discarded_animations": animations,
        "flattened_node_hierarchy": True,
        "excluded_nodes": [],
        "source_samplers": document.get("samplers", []),
        "sampler_substitution": "DX11 model sampler uses clamp and graphics-setting anisotropy",
        "double_sided_materials": [index for index, material in
            enumerate(document.get("materials", [])) if material.get("doubleSided")],
        "cull_substitution": "DX11 currently draws all model materials double sided",
    }
    return report


def convert_gun(source_name, filename, output_name, excluded=()):
    path = SOURCE / source_name / filename
    document, binary = read_scene(path)
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    groups = {}
    source_attributes = set()

    def visit(index, parent):
        node = document["nodes"][index]
        matrix = gltf.multiply(parent, gltf.local_matrix(node))
        if "mesh" in node and node.get("name", "") not in excluded:
            for primitive_index, primitive in enumerate(
                    document["meshes"][node["mesh"]]["primitives"]):
                if primitive.get("mode", 4) != 4:
                    raise ValueError(f"Only TRIANGLES glTF primitives are supported: {path}")
                attr = primitive["attributes"]
                source_attributes.update(attr)
                positions = gltf.accessor(document, binary, attr["POSITION"])
                normals = gltf.accessor(document, binary, attr["NORMAL"]) if "NORMAL" in attr else [(0,1,0)]*len(positions)
                uvs = gltf.accessor(document, binary, attr["TEXCOORD_0"]) if "TEXCOORD_0" in attr else [(0,0)]*len(positions)
                colors = gltf.accessor(document, binary, attr["COLOR_0"]) if "COLOR_0" in attr else [(1,1,1,1)]*len(positions)
                for color_set in (key for key in attr if key.startswith("COLOR_") and key != "COLOR_0"):
                    if any(any(abs(value-1.0)>1e-5 for value in color)
                           for color in gltf.accessor(document, binary, attr[color_set])):
                        raise ValueError(f"Nonwhite {color_set} cannot be preserved in {path}")
                indices = [v[0] for v in gltf.accessor(document, binary, primitive["indices"])] if "indices" in primitive else range(len(positions))
                material = primitive.get("material", 0)
                material_data = document.get("materials", [{}])[material]
                tint = material_data.get("pbrMetallicRoughness", {}).get(
                    "baseColorFactor", [1, 1, 1, 1])
                group = groups.setdefault(material, [])
                for element in indices:
                    p = gltf.transform(matrix, positions[element])
                    n = transformed_normal(matrix, normals[element])
                    u, v = uvs[element]
                    color = colors[element]
                    rgba = tuple(tint[channel]*(color[channel] if channel < len(color)
                        else 1.0) for channel in range(4))
                    group.append(((*p, *n, u, 1-v, *rgba),
                                  (index, primitive_index, element)))
        for child in node.get("children", []):
            visit(child, matrix)

    for index in document["scenes"][document.get("scene", 0)]["nodes"]:
        visit(index, identity)
    all_vertices = [vertex for group in groups.values() for vertex, _ in group]
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
    vertices, source_keys, ranges = [], [], []
    for material_index, group in groups.items():
        start = len(vertices)
        for i in range(0, len(group), 3):
            triangle = []
            for v, key in group[i:i+3]:
                p = orient(v[:3]); n = orient(v[3:6], True)
                if reverse:
                    p = (-p[0], p[1], -p[2]); n = (-n[0], n[1], -n[2])
                triangle.append((*p, *n, *v[6:]))
                source_keys.append(key)
            vertices.extend(triangle)
        ranges.append((material_index, start, len(vertices)-start))
    destination = OUTPUT / "weapons" / (output_name + ".m3d")
    write_mesh(destination, vertices, source_keys)
    image_cache = {}
    def texture_name(info):
        if info is None:
            return "-"
        if info.get("texCoord", 0) != 0 or "extensions" in info:
            raise ValueError(f"Unsupported texture UV set or transform in {path}")
        image_index = document["textures"][info["index"]]["source"]
        if image_index not in image_cache:
            image_cache[image_index] = embedded_image(
                document, binary, image_index, path, destination)
        return image_cache[image_index]
    with destination.with_suffix(".pbr").open("w") as target:
        for material_index, start, count in ranges:
            material = document.get("materials", [{}])[material_index]
            if material.get("extensions"):
                raise ValueError(f"Unsupported material extension in {path}")
            pbr = material.get("pbrMetallicRoughness", {})
            base = texture_name(pbr.get("baseColorTexture"))
            normal = texture_name(material.get("normalTexture"))
            orm = texture_name(pbr.get("metallicRoughnessTexture"))
            occlusion = texture_name(material.get("occlusionTexture"))
            emissive = texture_name(material.get("emissiveTexture"))
            if material.get("normalTexture", {}).get("scale", 1) != 1 or \
                    material.get("occlusionTexture", {}).get("strength", 1) != 1:
                raise ValueError(f"Unsupported texture scale or strength in {path}")
            emissive_factor = material.get("emissiveFactor", [0, 0, 0])
            if max(emissive_factor) - min(emissive_factor) > 1e-6:
                raise ValueError(f"Non-gray emissive factor needs RGB metadata: {path}")
            alpha_mode = material.get("alphaMode", "OPAQUE")
            if alpha_mode not in ("OPAQUE", "MASK"):
                raise ValueError(f"Unsupported alpha mode {alpha_mode} in {path}")
            alpha_cutoff = material.get("alphaCutoff", 0.5)
            target.write(f"{start} {count} {pbr.get('roughnessFactor',0.75)} "
                         f"{pbr.get('metallicFactor',0.15)} "
                         f"{emissive_factor[0]} {base} {normal} {orm} "
                         f"{occlusion} {emissive} "
                         f"{alpha_mode} {alpha_cutoff}\n")
    report = conversion_report(document, path, source_attributes)
    report["excluded_nodes"] = list(excluded)
    destination.with_suffix(".import.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n")
    if report["discarded_attributes"] or report["discarded_skins"] or \
            report["discarded_animations"]:
        print(f"WARNING: {output_name} is a static conversion; inspect "
              f"{destination.with_suffix('.import.json')}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--weapons-only", action="store_true")
    parser.add_argument("--traffic-only", action="store_true")
    args = parser.parse_args()
    if args.weapons_only and args.traffic_only:
        parser.error("Choose at most one asset group")
    if not args.weapons_only:
        for number, texture in ((1,"car_blue.png"),(2,"car2_red.png"),
                                (3,"car3_yellow.png"),(4,"car4_lightorange.png"),
                                (5,"car5_green.png")):
            convert_obj(f"Car {number:02d}", "Car" if number==1 else f"Car{number}",
                        texture, f"traffic-{number}")
    if not args.traffic_only:
        convert_gun("pistol", "Pistol.gltf", "pistol")
        convert_gun("ak", "AK.gltf", "ak", excluded=("Bullet","BulletBox","BulletFired"))
        convert_gun("lightning", "lightning.glb", "lightning")
