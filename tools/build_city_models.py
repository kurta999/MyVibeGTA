"""Bake 30 individual realistic trees and 30 textured urban building meshes.

Run after fetch_city_sources.ps1. The output is the DX11 M3D1 vertex format;
source glTF/GLB files and their CC0 provenance remain in source/.
"""

import io
import csv
import json
import math
import pathlib
import struct
import sys

import numpy as np
from PIL import Image, ImageEnhance, ImageOps, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/models/source"
BAKED = ROOT / "assets/models/baked"
TREE_IDS = [
    "tree_blocks", "tree_default", "tree_detailed", "tree_fat",
    "tree_oak", "tree_simple", "tree_tall", "tree_thin", "tree_cone",
    "tree_pineDefaultA", "tree_pineDefaultB", "tree_pineGroundA",
    "tree_pineGroundB", "tree_pineRoundA", "tree_pineRoundB",
    "tree_pineRoundC", "tree_pineRoundD", "tree_pineSmallA",
    "tree_pineSmallB", "tree_pineTallA", "tree_pineTallB",
    "tree_palmDetailedShort", "tree_palmDetailedTall", "tree_palmShort",
    "tree_palmTall", "tree_palm", "tree_palmBend", "tree_plateau",
    "tree_small", "tree_urbanCherry",
]
BUSH_SOURCES = [
    "shrub_01", "shrub_02", "shrub_03", "shrub_04", "shrub_sorrel_01",
    "wild_rooibos_bush", "fern_02", "nettle_plant", "periwinkle_plant",
    "weed_plant_02", "crystalline_iceplant",
]
BUSH_IDS = [f"bush_{index:02d}" for index in range(36)]
POLY_TREE_AUTHORS = {
    "tree_small_02": "Rico Cilliers",
    "fir_sapling": "Rob Tuytel; Rico Cilliers",
    "quiver_tree_01": "James Ray Cock; Dario Barresi; Rico Cilliers",
    "quiver_tree_02": "Dario Barresi; Rico Cilliers",
    "island_tree_01": "Rob Tuytel; Rico Cilliers",
    "island_tree_02": "Rob Tuytel; Rico Cilliers",
    "island_tree_03": "Rob Tuytel; Rico Cilliers",
    "jacaranda_tree": "Rob Tuytel; Rico Cilliers",
    "pine_sapling_small": "Rob Tuytel; Rico Cilliers",
}
OTHER_TREES = {
    "tree_blocks": "greek-island-village-and-harbour-olive-tree-b7baf3c7",
    "tree_fat": "jungle-temple-and-stone-city-buttress-root-tree-44607ba7",
    "tree_simple": "korean-hanok-village-and-street-persimmon-tree-ec7be2f6",
    "tree_pineGroundB": "alpine-and-arctic-biomes-timberline-tree-4975b730",
    "tree_palmDetailedShort": "greek-island-village-and-harbour-fig-tree-15e48fe1",
    "tree_palmShort": "jungle-temple-and-stone-city-palm-sapling-6702b25c",
    "tree_palmTall": "jungle-temple-and-stone-city-jungle-canopy-tree-6e668f41",
    "tree_palmDetailedTall": "desert-mosque-and-madrasa-courtyard-date-palm-c311e205",
    "tree_plateau": "exotic-wildlife-hd-umbrella-acacia-850a62bb",
    "tree_small": "exotic-wildlife-hd-baobab-tree-fb196805",
    "tree_urbanCherry": "japanese-school-and-city-street-cherry-tree-blossom-618a9f6d",
}
DTYPES = {5120: np.int8, 5121: np.uint8, 5122: np.int16,
          5123: np.uint16, 5125: np.uint32, 5126: np.float32}
WIDTHS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def read_scene(path):
    if path.suffix == ".glb":
        raw = path.read_bytes()
        magic, version, length = struct.unpack_from("<III", raw)
        if magic != 0x46546C67 or version != 2 or length != len(raw):
            raise ValueError(f"Bad GLB: {path}")
        json_length, _ = struct.unpack_from("<II", raw, 12)
        doc = json.loads(raw[20:20 + json_length])
        offset = 20 + json_length
        bin_length, _ = struct.unpack_from("<II", raw, offset)
        binary = memoryview(raw)[offset + 8:offset + 8 + bin_length]
    else:
        doc = json.loads(path.read_text(encoding="utf-8"))
        binary = memoryview((path.parent / doc["buffers"][0]["uri"]).read_bytes())
    return doc, binary


def accessor(doc, binary, index):
    item = doc["accessors"][index]
    view = doc["bufferViews"][item["bufferView"]]
    dtype = np.dtype(DTYPES[item["componentType"]]).newbyteorder("<")
    width = WIDTHS[item["type"]]
    stride = view.get("byteStride", width * dtype.itemsize)
    start = view.get("byteOffset", 0) + item.get("byteOffset", 0)
    data = np.ndarray((item["count"], width), dtype=dtype, buffer=binary,
                      offset=start, strides=(stride, dtype.itemsize))
    if item.get("normalized", False) and dtype.kind != "f":
        limit = np.iinfo(dtype).max
        return np.maximum(-1, data.astype(np.float32) / limit)
    return data


def node_matrix(node):
    if "matrix" in node:
        return np.array(node["matrix"], dtype=np.float64).reshape((4, 4), order="F")
    t = np.array(node.get("translation", [0, 0, 0]), dtype=np.float64)
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    s = node.get("scale", [1, 1, 1])
    m = np.array([
        [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w), t[0]],
        [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w), t[1]],
        [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y), t[2]],
        [0, 0, 0, 1]], dtype=np.float64)
    m[:3, :3] *= np.asarray(s)[None, :]
    return m


def image_from_material(doc, binary, path, material):
    pbr = material.get("pbrMetallicRoughness", {})
    base = pbr.get("baseColorTexture")
    if base is None:
        factor = pbr.get("baseColorFactor", [1, 1, 1, 1])
        name = material.get("name", "").lower()
        if any(word in name for word in ("leaf", "leav", "foliage", "blossom")):
            sample = SOURCE / "polyhaven/tree_small_02/textures/tree_small_02_leaves_diff_1k.jpg"
        elif any(word in name for word in ("stone", "concrete")):
            sample = ROOT / "assets/materials/Concrete001_Color.jpg"
        else:
            sample = ROOT / "assets/materials/Bark001_Color.jpg"
        grey = ImageOps.grayscale(Image.open(sample)).resize((512, 512))
        rgb = tuple(int(max(0, min(1, v)) * 255) for v in factor[:3])
        low = tuple(int(channel * 0.50) for channel in rgb)
        high = tuple(min(255, int(channel * 1.45)) for channel in rgb)
        return ImageOps.colorize(grey, low, high).convert("RGBA")
    texture = doc["textures"][base["index"]]
    source_index = texture.get("source", texture.get("extensions", {}).get("EXT_texture_webp", {}).get("source"))
    image = doc["images"][source_index]
    if "uri" in image:
        result = Image.open(path.parent / image["uri"])
    else:
        view = doc["bufferViews"][image["bufferView"]]
        start = view.get("byteOffset", 0)
        result = Image.open(io.BytesIO(binary[start:start + view["byteLength"]]))
    result = result.convert("RGBA")
    name = material.get("name", "").lower()
    if ("tree_small_02_leaves" in name or
            (material.get("alphaMode") in ("MASK", "BLEND") and
             np.mean(np.max(np.asarray(result)[:, :, :3], axis=2) < 40) > 0.15)):
        arr = np.asarray(result).copy()
        brightness = np.max(arr[:, :, :3], axis=2)
        arr[:, :, 3] = np.clip((brightness.astype(np.int16) - 23) * 12, 0, 255).astype(np.uint8)
        result = Image.fromarray(arr, "RGBA")
    return result


def material_atlas(doc, binary, path, variant, cell=512):
    materials = doc.get("materials", [{}])
    cols = 2 if len(materials) <= 4 else 4
    rows = math.ceil(len(materials) / cols)
    atlas = Image.new("RGBA", (cols * cell, rows * cell), (255, 255, 255, 255))
    for index, material in enumerate(materials):
        tile = image_from_material(doc, binary, path, material)
        name = material.get("name", "").lower()
        if (any(word in name for word in ("leaf", "leav", "twig", "foliage", "flower", "blossom"))
                or path.parent.name in BUSH_SOURCES):
            # Closely related species vary in foliage tone without sharing a texture file.
            tint = ((variant * 37) % 43 - 21) * 0.009
            arr = np.asarray(tile).copy()
            rgb = arr[:, :, :3].astype(np.float32)
            rgb[:, :, 0] *= 1 + tint * 0.6
            rgb[:, :, 1] *= 1 - tint * 0.3
            rgb[:, :, 2] *= 1 + tint * 0.25
            arr[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
            tile = Image.fromarray(arr, "RGBA")
        tile = tile.resize((cell, cell), Image.Resampling.LANCZOS)
        atlas.paste(tile, ((index % cols) * cell, (index // cols) * cell))
    return atlas, cols, rows


def write_mesh(path, vertices):
    path.parent.mkdir(parents=True, exist_ok=True)
    arr = np.asarray(vertices, dtype="<f4").reshape((-1, 12))
    if len(arr) == 0 or len(arr) > 3_000_000 or not np.isfinite(arr).all():
        raise ValueError(f"Invalid mesh {path}: {len(arr)} vertices")
    with path.open("wb") as file:
        file.write(struct.pack("<4sI", b"M3D1", len(arr)))
        file.write(arr.tobytes())
    return len(arr) // 3


def tree_source(name):
    if name in OTHER_TREES:
        slug = OTHER_TREES[name]
        return SOURCE / "3dassets" / (slug + ".glb"), None
    polyhaven = {"tree_detailed": "island_tree_03",
                 "tree_thin": "jacaranda_tree",
                 "tree_cone": "fir_sapling",
                 "tree_pineDefaultB": "fir_sapling",
                 "tree_palm": "quiver_tree_01",
                 "tree_palmBend": "quiver_tree_02"}
    if name in polyhaven:
        asset = polyhaven[name]
        return SOURCE / "polyhaven" / asset / (asset + ".gltf"), None
    if name.startswith("tree_pine"):
        return SOURCE / "polyhaven/pine_sapling_small/pine_sapling_small.gltf", None
    if name == "tree_oak":
        return SOURCE / "polyhaven/island_tree_01/island_tree_01.gltf", None
    if name == "tree_tall":
        return SOURCE / "polyhaven/island_tree_02/island_tree_02.gltf", None
    return SOURCE / "polyhaven/tree_small_02/tree_small_02.gltf", None


def palm_fronds(variant, lod=False):
    """Fine leaflets replace the date-palm source's flat crown polygons."""
    vertices = []
    fronds = 12 if lod else 22
    steps = 12 if lod else 29
    tile = 2  # Foliage tile in the date palm's 2x2 material atlas.

    def add_triangle(a, b, c, uv):
        edge1 = np.asarray(b) - a
        edge2 = np.asarray(c) - a
        normal = np.cross(edge1, edge2)
        normal /= max(1e-8, np.linalg.norm(normal))
        for p, (u, v) in zip((a, b, c), uv):
            vertices.append((*p, *normal, (u + 0.002) / 2,
                             (1 + v * 0.996 + 0.002) / 2, 1, 1, 1, 1))

    for f in range(fronds):
        angle = f * (2 * math.pi / fronds) + variant * 0.13
        radial = np.array([math.cos(angle), 0, math.sin(angle)])
        side = np.array([-math.sin(angle), 0, math.cos(angle)])

        def spine(t):
            radius = 0.12 + (2.3 + variant % 3 * 0.18) * t
            return radial * radius + np.array([0, 5.62 + 0.45 * math.sin(math.pi*t)
                                               - 1.10 * t*t, 0])

        for s in range(steps):
            t0 = (s + 0.5) / steps
            t1 = (s + 1.5) / steps
            base = spine(t0)
            next_point = spine(min(1, t1))
            add_triangle(base - side * 0.045, base + side * 0.045,
                         next_point, ((0.22, 0.12), (0.35, 0.12), (0.29, 0.88)))
            extent = (0.22 + 0.65 * math.sin(math.pi*t0)) * (1 - t0*0.20)
            for direction in (-1, 1):
                root = base + side * direction * 0.025
                mid = root + side * direction * extent * 0.56 + radial * 0.09
                tip = root + side * direction * extent + radial * 0.22
                mid[1] -= 0.08 + t0 * 0.09
                tip[1] -= 0.24 + t0 * 0.16
                width = 0.085 * (1-t0*0.62)
                add_triangle(root - radial * width, root + radial * width, mid,
                             ((0.13, 0.12), (0.66, 0.12), (0.44, 0.50)))
                add_triangle(mid - radial * width * 0.6, mid + radial * width * 0.6,
                             tip, ((0.26, 0.46), (0.67, 0.46), (0.50, 0.94)))
    return np.asarray(vertices, dtype=np.float32)


def bake_tree(name, variant, source_override=None):
    path = source_override if source_override else tree_source(name)[0]
    bush = name.startswith("bush_")
    doc, binary = read_scene(path)
    atlas, cols, rows = material_atlas(doc, binary, path, variant)
    destination = BAKED / "nature" / name
    atlas.save(destination.with_suffix(".png"), optimize=True)
    meshes = doc["meshes"]
    nodes = doc["nodes"]
    scene = doc["scenes"][doc.get("scene", 0)]
    rng = np.random.default_rng(variant * 7919 + 17)
    chosen = []
    if path.name in ("fir_sapling.gltf", "pine_sapling_small.gltf"):
        chosen = [variant % 3]
    full_parts = []
    lod_parts = []

    def visit(index, parent):
        node = nodes[index]
        matrix = parent @ node_matrix(node)
        if "mesh" in node and (not chosen or node["mesh"] in chosen):
            for primitive in meshes[node["mesh"]]["primitives"]:
                if primitive.get("mode", 4) != 4:
                    continue
                attrs = primitive["attributes"]
                pos = accessor(doc, binary, attrs["POSITION"]).astype(np.float32)
                normals = accessor(doc, binary, attrs["NORMAL"]).astype(np.float32) if "NORMAL" in attrs else None
                material_index = primitive.get("material", 0)
                material = doc.get("materials", [{}])[material_index]
                base = material.get("pbrMetallicRoughness", {}).get("baseColorTexture", {})
                tc = base.get("texCoord", 0)
                uv_name = f"TEXCOORD_{tc}"
                if uv_name in attrs:
                    uv = accessor(doc, binary, attrs[uv_name]).astype(np.float32)
                else:
                    # Geometry without UVs still receives a spatial bark/leaf map.
                    uv = pos[:, [0, 1]].astype(np.float32) * 0.37
                transform = base.get("extensions", {}).get("KHR_texture_transform", {})
                uv = uv * np.asarray(transform.get("scale", [1, 1]), np.float32) + np.asarray(transform.get("offset", [0, 0]), np.float32)
                indices = accessor(doc, binary, primitive["indices"]).reshape(-1).astype(np.int32) if "indices" in primitive else np.arange(len(pos), dtype=np.int32)
                tri = indices.reshape((-1, 3))
                name_lower = material.get("name", "").lower()
                foliage = any(word in name_lower for word in ("leaf", "leav", "twig", "foliage", "flower", "blossom"))
                if path.suffix == ".glb" and "date-palm" in path.name and "foliage" in name_lower:
                    continue
                cap = (35000 if path.name == "tree_small_02.gltf" else
                       25000 if path.name in ("fir_sapling.gltf", "pine_sapling_small.gltf") else 35000) if foliage else (
                           6000 if "branch" in name_lower else 10000)
                if bush:
                    cap = 20000
                if len(tri) > cap:
                    if foliage and (path.name in ("tree_small_02.gltf", "fir_sapling.gltf", "pine_sapling_small.gltf") or
                                    path.name.startswith("island_tree_")) and len(tri) % 2 == 0:
                        # The source leaf cards are triangle pairs; retain both halves.
                        pairs = tri.reshape((-1, 2, 3))
                        selection = np.sort(rng.choice(len(pairs), cap // 2, replace=False))
                        tri = pairs[selection].reshape((-1, 3))
                    else:
                        selection = np.sort(rng.choice(len(tri), cap, replace=False))
                        tri = tri[selection]
                lod_cap = (5000 if path.name == "tree_small_02.gltf" or
                           path.name.startswith("island_tree_") else
                           4000 if path.name in ("fir_sapling.gltf", "pine_sapling_small.gltf") else 1800) if foliage else 650
                if foliage and (path.name in ("tree_small_02.gltf", "fir_sapling.gltf", "pine_sapling_small.gltf") or
                                path.name.startswith("island_tree_")) and len(tri) % 2 == 0:
                    pairs = tri.reshape((-1, 2, 3))
                    selection = np.sort(rng.choice(len(pairs), min(lod_cap // 2, len(pairs)), replace=False))
                    lod_tri = pairs[selection].reshape((-1, 3))
                else:
                    lod_tri = tri[np.sort(rng.choice(len(tri), min(lod_cap, len(tri)), replace=False))]

                def pack(t, is_lod=False):
                    ids = t.reshape(-1)
                    points = pos[ids] @ matrix[:3, :3].T + matrix[:3, 3]
                    if foliage and (path.name in ("tree_small_02.gltf", "fir_sapling.gltf", "pine_sapling_small.gltf") or
                                    path.name.startswith("island_tree_")) and len(points) % 6 == 0:
                        # The source uses tiny two-triangle leaf cards. Enlarging
                        # each card retains its detailed texture and closes the
                        # holes left by the game-sized leaf sample.
                        cards = points.reshape((-1, 6, 3))
                        center = cards.mean(axis=1, keepdims=True)
                        card_scale = (12.0 if path.name == "tree_small_02.gltf" else
                                      6.2 if path.name in ("fir_sapling.gltf", "pine_sapling_small.gltf") else 11.0) if is_lod else (
                                      5.0 if path.name == "tree_small_02.gltf" else
                                      3.0 if path.name in ("fir_sapling.gltf", "pine_sapling_small.gltf") else 4.8)
                        cards[:] = center + (cards - center) * card_scale
                    # Change crown asymmetry and branch spread while preserving the trunk base.
                    points[:, 0] += np.sin(points[:, 1] * (0.65 + variant % 5 * 0.07) + variant) * points[:, 1] * (0.004 + variant % 3 * 0.002)
                    points[:, 2] += np.cos(points[:, 1] * 0.52 + variant * 1.3) * points[:, 1] * 0.004
                    if bush:
                        points[:, 0] *= 0.88 + ((variant * 7) % 9) * 0.035
                        points[:, 2] *= 0.88 + ((variant * 11) % 9) * 0.035
                    if normals is not None:
                        normal = normals[ids] @ np.linalg.inv(matrix[:3, :3])
                    else:
                        edges = points.reshape((-1, 3, 3))
                        n = np.cross(edges[:, 1] - edges[:, 0], edges[:, 2] - edges[:, 0])
                        normal = np.repeat(n, 3, axis=0)
                    normal /= np.maximum(np.linalg.norm(normal, axis=1, keepdims=True), 1e-8)
                    tex = np.mod(uv[ids], 1.0)
                    tex[:, 1] = 1.0 - tex[:, 1]
                    tex[:, 0] = (material_index % cols + tex[:, 0] * 0.998 + 0.001) / cols
                    tex[:, 1] = (material_index // cols + tex[:, 1] * 0.998 + 0.001) / rows
                    factor = material.get("pbrMetallicRoughness", {}).get("baseColorFactor", [1, 1, 1, 1])
                    if "baseColorTexture" not in material.get("pbrMetallicRoughness", {}):
                        factor = [1, 1, 1, 1]  # Applied to the fallback texture above.
                    color = np.tile(np.asarray(factor, np.float32), (len(ids), 1))
                    return np.concatenate((points, normal, tex, color), axis=1).astype(np.float32)

                full_parts.append(pack(tri))
                lod_parts.append(pack(lod_tri, is_lod=True))
        for child in node.get("children", []):
            visit(child, matrix)

    for index in scene["nodes"]:
        visit(index, np.eye(4))
    if "date-palm" in path.name:
        full_parts.append(palm_fronds(variant))
        lod_parts.append(palm_fronds(variant, lod=True))
    full = np.concatenate(full_parts)
    lod = np.concatenate(lod_parts)
    if bush and len(lod) > 4200 * 3:
        triangles = lod.reshape((-1, 3, 12))
        selection = np.sort(rng.choice(len(triangles), 4200, replace=False))
        lod = triangles[selection].reshape((-1, 12))
    # Some kit assets place the model slightly above the origin; runtime aligns minY.
    print(f"{name}: {write_mesh(destination.with_suffix('.m3d'), full)} triangles, "
          f"LOD {write_mesh(destination.with_name(name + '-lod.m3d'), lod)}")


def building_atlas(index):
    urban = SOURCE / "polyhaven/modular_urban_apartments_facade/textures"
    factory = SOURCE / "polyhaven/modular_factory_facade/textures"
    plaster = Image.open(urban / "modular_urban_apartments_facade_plaster_diff_1k.jpg").convert("RGB")
    brick = Image.open(factory / "modular_factory_facade_brick_diff_1k.jpg").convert("RGB")
    windows = Image.open(factory / "modular_factory_facade_windows_diff_1k.jpg").convert("RGB")
    trim = Image.open(factory / "modular_factory_facade_trim_01_diff_1k.jpg").convert("RGB")
    doors = Image.open(factory / "modular_factory_facade_doors_diff_1k.jpg").convert("RGB")
    concrete = Image.open(ROOT / "assets/materials/Concrete001_Color.jpg").convert("RGB")
    atlas = Image.new("RGBA", (2048, 1024), (255, 255, 255, 255))
    palettes = [
        ((217, 223, 225), (75, 96, 108)), ((195, 183, 170), (62, 74, 82)),
        ((153, 165, 174), (55, 77, 91)), ((219, 205, 186), (69, 83, 88)),
        ((183, 194, 191), (61, 77, 80)), ((198, 171, 153), (54, 68, 80)),
    ]
    wall, glass = palettes[index % len(palettes)]

    def colorize(img, color, amount):
        base = img.resize((512, 512), Image.Resampling.LANCZOS)
        tint = Image.new("RGB", base.size, color)
        return Image.blend(base, tint, amount).convert("RGBA")

    tiles = [
        colorize(plaster, wall, 0.46),
        colorize(brick if index % 3 == 2 else concrete, wall, 0.32),
        colorize(windows.crop((10, 530, 1020, 1020)), glass, 0.38),
        colorize(windows.crop((10, 530, 1020, 1020)), (26, 42, 57), 0.52),
        colorize(trim, (77, 85, 91), 0.45),
        colorize(concrete, (82, 88, 93), 0.26),
        colorize(doors, wall, 0.25),
        Image.new("RGBA", (512, 512), (35, 46, 55, 255)),
    ]
    draw = ImageDraw.Draw(tiles[7])
    accent = [(227, 150, 84), (109, 189, 203), (221, 206, 146),
              (166, 190, 164), (206, 145, 152)][index % 5]
    draw.rectangle((16, 16, 496, 496), outline=accent + (255,), width=9)
    for n in range(3 + index % 4):
        draw.rectangle((45 + n * 59, 140, 68 + n * 59, 375), fill=accent + (255,))
    for tile, img in enumerate(tiles):
        atlas.paste(img, ((tile % 4) * 512, (tile // 4) * 512))
    return atlas


class BuildingMesh:
    def __init__(self):
        self.vertices = []

    @staticmethod
    def uv(tile, u, v):
        return ((tile % 4 + u * 0.998 + 0.001) / 4,
                (tile // 4 + v * 0.998 + 0.001) / 2)

    def quad(self, points, normal, tile, color=(1, 1, 1)):
        uv = [self.uv(tile, 0, 1), self.uv(tile, 1, 1),
              self.uv(tile, 1, 0), self.uv(tile, 0, 0)]
        for idx in (0, 1, 2, 0, 2, 3):
            p = points[idx]
            self.vertices.append((*p, *normal, *uv[idx], *color, 1.0))

    def box(self, x, y, z, w, h, d, tile, color=(1, 1, 1)):
        a, b = x - w / 2, x + w / 2
        c, e = z - d / 2, z + d / 2
        low, top = y, y + h
        self.quad([(a, low, c), (b, low, c), (b, top, c), (a, top, c)], (0, 0, -1), tile, color)
        self.quad([(b, low, e), (a, low, e), (a, top, e), (b, top, e)], (0, 0, 1), tile, color)
        self.quad([(a, low, e), (a, low, c), (a, top, c), (a, top, e)], (-1, 0, 0), tile, color)
        self.quad([(b, low, c), (b, low, e), (b, top, e), (b, top, c)], (1, 0, 0), tile, color)
        self.quad([(a, top, c), (b, top, c), (b, top, e), (a, top, e)], (0, 1, 0), tile, color)

    def facade(self, face, span0, span1, y0, y1, outward, tile, color=(1, 1, 1)):
        if face == 0:
            points = [(span0, y0, -0.5-outward), (span1, y0, -0.5-outward),
                      (span1, y1, -0.5-outward), (span0, y1, -0.5-outward)]
            normal = (0, 0, -1)
        elif face == 1:
            points = [(span1, y0, 0.5+outward), (span0, y0, 0.5+outward),
                      (span0, y1, 0.5+outward), (span1, y1, 0.5+outward)]
            normal = (0, 0, 1)
        elif face == 2:
            points = [(-0.5-outward, y0, span1), (-0.5-outward, y0, span0),
                      (-0.5-outward, y1, span0), (-0.5-outward, y1, span1)]
            normal = (-1, 0, 0)
        else:
            points = [(0.5+outward, y0, span0), (0.5+outward, y0, span1),
                      (0.5+outward, y1, span1), (0.5+outward, y1, span0)]
            normal = (1, 0, 0)
        self.quad(points, normal, tile, color)


def bake_building(index):
    name = f"urban-{index:02d}"
    atlas = building_atlas(index)
    dest = BAKED / "buildings" / name
    atlas.save(dest.with_suffix(".png"), optimize=True)
    full = BuildingMesh()
    lod = BuildingMesh()
    family = index % 6
    stories = 5 + (index * 7) % 11
    bays = 4 + (index * 3) % 4
    wall_tile = 1 if family in (2, 5) else 0
    window_tile = 3 if family in (1, 5) else 2
    full.box(0, 0, 0, 1, 1, 1, wall_tile)
    lod.box(0, 0, 0, 1, 1, 1, wall_tile)
    floor_height = 1 / stories
    bay_width = 1 / bays
    for face in range(4):
        for floor in range(stories):
            y0, y1 = floor * floor_height, (floor + 1) * floor_height
            for bay in range(bays):
                x0, x1 = -0.5 + bay * bay_width, -0.5 + (bay + 1) * bay_width
                if floor == 0 and face == 0 and bay == bays // 2:
                    tile = 6
                else:
                    tile = window_tile
                margin_x = bay_width * (0.10 if family == 0 else 0.18)
                margin_y = floor_height * (0.14 if family in (0, 4) else 0.23)
                left, right = x0 + margin_x, x1 - margin_x
                bottom, top = y0 + margin_y, y1 - margin_y
                lit = (index * 19 + face * 13 + floor * 7 + bay * 5) % 17 == 0
                tint = (1.17, 1.06, 0.78) if lit else (0.70, 0.86, 0.98)
                full.facade(face, left, right, bottom, top, 0.003, tile, tint)
                lod.facade(face, left, right, bottom, top, 0.003, tile, tint)
                frame_w = bay_width * 0.025
                frame_h = floor_height * 0.028
                full.facade(face, left, left + frame_w, bottom, top, 0.012, 4)
                full.facade(face, right - frame_w, right, bottom, top, 0.012, 4)
                full.facade(face, left, right, bottom, bottom + frame_h, 0.012, 4)
                full.facade(face, left, right, top - frame_h, top, 0.012, 4)
                if family in (0, 2, 4):
                    middle = (left + right) * 0.5
                    full.facade(face, middle - frame_w * 0.4,
                                middle + frame_w * 0.4, bottom, top, 0.014, 4)
                    cross = bottom + (top - bottom) * 0.67
                    full.facade(face, left, right, cross,
                                cross + frame_h * 0.65, 0.014, 4)
                if face == 0 and family in (1, 3, 5) and floor > 0:
                    # Projecting sun shades and sill caps give the facade depth.
                    full.box((left + right) * 0.5, top, -0.515,
                             right - left + frame_w * 4, frame_h * 0.7, 0.045, 4)
                if face < 2 and (family == 1 or family == 3) and floor > 0 and bay % 2 == index % 2:
                    full.facade(face, x0 + 0.01, x1 - 0.01, y0 + floor_height * 0.1,
                                y0 + floor_height * 0.14, 0.038, 5)
                    full.facade(face, x0 + 0.01, x1 - 0.01, y0 + floor_height * 0.14,
                                y0 + floor_height * 0.34, 0.041, 4, (0.65, 0.69, 0.72))
                if family in (0, 4) and bay % 2 == 0:
                    full.facade(face, right, right + bay_width * 0.045, y0, y1, 0.009, 4)
            if family in (2, 5) and floor % 3 == 2:
                full.facade(face, -0.5, 0.5, y1 - floor_height * 0.035, y1, 0.011, 4)
    # A distinct podium, cornice, rooftop volume and safety parapet break the box silhouette.
    podium = 0.07 + (index % 3) * 0.018
    full.box(0, 0, 0, 1.06, podium, 1.06, 1)
    full.box(0, podium, 0, 1.055, 0.008, 1.055, 4)
    full.box(0, 1, 0, 1.035, 0.012, 1.035, 5)
    for side in (-1, 1):
        full.box(side * 0.502, 1, 0, 0.015, 0.028, 1.03, 4)
        full.box(0, 1, side * 0.502, 1.03, 0.028, 0.015, 4)
    rooftop = 0.10 + (index % 4) * 0.013
    full.box(0.12 if index % 2 else -0.13, 1.01, 0.10, 0.27, rooftop, 0.22, 5)
    full.box(-0.22, 1.01, -0.18, 0.18, 0.035, 0.16, 4)
    for n in range(3 + index % 4):
        x = -0.38 + n * 0.17
        full.box(x, 1.015, 0.26, 0.09, 0.028, 0.035, 4)
    if family in (0, 4):
        full.box(0, 0.11, -0.513, 0.73, 0.015, 0.045, 4)
        full.facade(0, -0.34, 0.34, 0.055, 0.105, 0.04, 7)
    else:
        full.box(0, 0.09, -0.528, 0.66, 0.012, 0.065, 4)
    full_tri = write_mesh(dest.with_suffix(".m3d"), full.vertices)
    lod_tri = write_mesh(dest.with_name(name + "-lod.m3d"), lod.vertices)
    print(f"{name}: {full_tri} triangles, LOD {lod_tri}")


def write_manifests():
    nature_path = ROOT / "assets/models/NATURE_MANIFEST.csv"
    with nature_path.open(newline="", encoding="utf-8") as file:
        previous = list(csv.DictReader(file))
    props = [row for row in previous if not row["model_id"].startswith(("tree_", "bush_"))]
    trees = []
    for name in TREE_IDS:
        source, _ = tree_source(name)
        relative = source.relative_to(ROOT / "assets/models").as_posix()
        if "polyhaven" in relative:
            asset_id = source.parent.name
            url = f"https://polyhaven.com/a/{asset_id}"
            creator = POLY_TREE_AUTHORS[asset_id]
        else:
            url = f"https://3dassets.dev/assets/{source.stem}"
            creator = "3D Assets"
        trees.append(dict(model_id=name, source_path=relative,
                          baked_path=f"baked/nature/{name}.m3d", source_url=url,
                          creator=creator, license="CC0-1.0",
                          download_date="2026-09-25", attribution_required="no"))
    bushes = []
    for index, name in enumerate(BUSH_IDS):
        source_id = BUSH_SOURCES[index % len(BUSH_SOURCES)]
        source = SOURCE / "polyhaven" / source_id / f"{source_id}.gltf"
        bushes.append(dict(model_id=name,
                           source_path=source.relative_to(ROOT / "assets/models").as_posix(),
                           baked_path=f"baked/nature/{name}.m3d",
                           source_url=f"https://polyhaven.com/a/{source_id}",
                           creator="Poly Haven", license="CC0-1.0",
                           download_date="2026-09-25", attribution_required="no"))
    with nature_path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=list(trees[0]))
        writer.writeheader()
        writer.writerows(props + trees + bushes)
    city_path = ROOT / "assets/models/CITY_MANIFEST.csv"
    with city_path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(("model_id", "baked_path", "texture_path", "source_urls", "license"))
        urls = "https://polyhaven.com/a/modular_urban_apartments_facade|https://polyhaven.com/a/modular_factory_facade"
        for index in range(30):
            name = f"urban-{index:02d}"
            writer.writerow((name, f"baked/buildings/{name}.m3d",
                             f"baked/buildings/{name}.png", urls, "CC0-1.0"))


if __name__ == "__main__":
    if "--manifest-only" in sys.argv:
        write_manifests()
        sys.exit(0)
    if "--sample-tree" in sys.argv:
        bake_tree(TREE_IDS[0], 0)
    elif "--sample-building" in sys.argv:
        bake_building(0)
    elif "--buildings" in sys.argv:
        for index in range(30):
            bake_building(index)
    elif "--trees" in sys.argv:
        for index, tree_id in enumerate(TREE_IDS):
            bake_tree(tree_id, index)
    elif "--bushes" in sys.argv:
        for index, name in enumerate(BUSH_IDS):
            source_id = BUSH_SOURCES[index % len(BUSH_SOURCES)]
            bake_tree(name, index, SOURCE / "polyhaven" / source_id / f"{source_id}.gltf")
    else:
        for index, tree_id in enumerate(TREE_IDS):
            bake_tree(tree_id, index)
        for index in range(30):
            bake_building(index)
        for index, name in enumerate(BUSH_IDS):
            source_id = BUSH_SOURCES[index % len(BUSH_SOURCES)]
            bake_tree(name, index, SOURCE / "polyhaven" / source_id / f"{source_id}.gltf")
    if "--sample-tree" not in sys.argv and "--sample-building" not in sys.argv:
        write_manifests()
