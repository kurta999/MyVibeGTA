"""Build original Marina Part-inspired DX11 facade meshes and texture atlases.

The reference photographs inform the massing, not the pixels. This script uses
only Python's standard library and can regenerate every authored asset below.
"""

from pathlib import Path
import math
import struct
import zlib
import json
import argparse
import hashlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "models" / "baked" / "marina"
ATLAS = 2048
CELL = ATLAS // 4

PATCHES = {
    "plaster": 0, "pale": 1, "sand": 2, "glass": 3,
    "lit_glass": 4, "balcony_glass": 5, "metal": 6, "silver": 7,
    "stone": 8, "concrete": 9, "wood": 10, "roof": 11,
    "planter": 12, "retail": 13, "recess": 14, "canopy": 15,
}
COLORS = [
    (223, 214, 196), (238, 232, 216), (200, 187, 165), (47, 91, 121),
    (126, 147, 146), (132, 172, 183), (64, 72, 78), (171, 183, 183),
    (199, 193, 178), (179, 177, 169), (142, 110, 78), (107, 111, 109),
    (68, 111, 69), (51, 70, 82), (67, 64, 63), (106, 122, 124),
]


def png(path, normal=False):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    stream = zlib.compressobj(6)
    encoded = bytearray()
    for y in range(ATLAS):
        row = bytearray(1 + ATLAS * 4)
        for x in range(ATLAS):
            cell = (y // CELL) * 4 + x // CELL
            lx, ly = x % CELL, y % CELL
            fine = ((x * 37 + y * 19 + (x * y) % 53) % 19) - 9
            broad = (((x // 23) * 13 + (y // 29) * 17) % 15) - 7
            if normal:
                rough = cell in (0, 1, 2, 8, 9, 10, 11, 12)
                r, g, b = (128 + fine // 3, 128 + broad // 3, 253) if rough else (128, 128, 255)
            else:
                r, g, b = COLORS[cell]
                if cell in (0, 1, 2, 8, 9, 11):
                    jitter = fine + broad // 2
                    r, g, b = r + jitter, g + jitter, b + jitter
                    if cell in (8, 9) and (lx % 126 < 3 or ly % 124 < 3):
                        r, g, b = r - 19, g - 19, b - 19
                elif cell in (3, 4, 5, 13):
                    gleam = 34 if abs((lx - ly // 3) % 227 - 113) < 13 else 0
                    vertical = (ly * 22) // CELL
                    r, g, b = r + gleam + vertical, g + gleam + vertical, b + gleam + vertical
                    if cell == 4 and ly > CELL * 2 // 3:
                        r, g, b = r + 32, g + 20, b + 5
                elif cell in (6, 7, 10, 15):
                    stripe = 9 if lx % 46 < 3 else 0
                    r, g, b = r + stripe + fine // 3, g + stripe + fine // 3, b + stripe + fine // 3
                elif cell == 12:
                    r, g, b = r + fine, g + fine * 2, b + fine
                else:
                    r, g, b = r + fine // 2, g + fine // 2, b + fine // 2
                r, g, b = (max(0, min(255, v)) for v in (r, g, b))
            offset = 1 + x * 4
            row[offset:offset + 4] = bytes((r, g, b, 255))
        encoded.extend(stream.compress(row))
    encoded.extend(stream.flush())
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", ATLAS, ATLAS, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", bytes(encoded))
        + chunk(b"IEND", b"")
    )


def uv(patch):
    index = PATCHES[patch]
    x, y = index % 4, index // 4
    pad = 12
    return ((x * CELL + pad) / ATLAS, (y * CELL + pad) / ATLAS,
            ((x + 1) * CELL - pad) / ATLAS, ((y + 1) * CELL - pad) / ATLAS)


class Mesh:
    def __init__(self):
        self.vertices = []

    def quad(self, a, b, c, d, normal, patch):
        u0, v0, u1, v1 = uv(patch)
        corners = ((a, u0, v1), (b, u1, v1), (c, u1, v0),
                   (a, u0, v1), (c, u1, v0), (d, u0, v0))
        for point, u, v in corners:
            self.vertices.extend((*point, *normal, u, v, 1.0, 1.0, 1.0, 1.0))

    def box(self, x0, y0, z0, x1, y1, z1, patch, top=None):
        if x1 <= x0 or y1 <= y0 or z1 <= z0:
            return
        self.quad((x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(0,0,-1),patch)
        self.quad((x1,y0,z1),(x0,y0,z1),(x0,y1,z1),(x1,y1,z1),(0,0,1),patch)
        self.quad((x0,y0,z1),(x0,y0,z0),(x0,y1,z0),(x0,y1,z1),(-1,0,0),patch)
        self.quad((x1,y0,z0),(x1,y0,z1),(x1,y1,z1),(x1,y1,z0),(1,0,0),patch)
        self.quad((x0,y1,z0),(x1,y1,z0),(x1,y1,z1),(x0,y1,z1),(0,1,0),top or patch)
        self.quad((x0,y0,z1),(x1,y0,z1),(x1,y0,z0),(x0,y0,z0),(0,-1,0),patch)

    def save(self, name):
        target = OUT / (name + ".m3d")
        with target.open("wb") as file:
            file.write(b"M3D1" + struct.pack("<I", len(self.vertices) // 12))
            file.write(struct.pack("<" + "f" * len(self.vertices), *self.vertices))
        print(f"{target.relative_to(ROOT)}: {len(self.vertices) // 12} vertices")

    def save_indexed(self, name):
        vertices, indices = [], []
        # The authoring primitive is a quad with four distinct source corners.
        # Keep its normal/UV seams separate from every adjacent quad.
        for start in range(0, len(self.vertices), 72):
            quad = self.vertices[start:start + 72]
            first = len(vertices) // 12
            for corner in (0, 1, 2, 5):
                vertices.extend(quad[corner * 12:(corner + 1) * 12])
            indices.extend(first + corner for corner in (0, 1, 2, 0, 2, 3))
        target = OUT / (name + ".m3d")
        with target.open("wb") as file:
            file.write(struct.pack("<4sII", b"M3D2", len(vertices) // 12, len(indices)))
            file.write(struct.pack("<" + "f" * len(vertices), *vertices))
            file.write(struct.pack("<" + "I" * len(indices), *indices))
        return {"file": target.name, "triangles": len(indices) // 3,
                "vertices": len(vertices) // 12,
                "sha256": hashlib.sha256(target.read_bytes()).hexdigest()}


def building(style, lod=False, level=0):
    # A normalized footprint is scaled to each parcel by the DX11 model system.
    width, depth = (200, 180) if style != "bayfront" else (250, 195)
    floors = 9 if style == "wave" else 8
    if style == "terrace":
        floors = 8
    floor_height = 17.0
    mesh = Mesh()
    mesh.box(0, 0, 0, width, 2, depth, "stone", "concrete")
    for floor in range(floors):
        y = 2 + floor * floor_height
        setback = (max(0, floor - 4) * 2.2 if style == "terrace" else 0)
        west = 15 + setback
        east = width - 8
        north, south = 8, depth - 8
        wall = "pale" if style == "wave" else "plaster" if floor % 3 else "sand"
        mesh.box(west, y, north, east, y + floor_height, south, wall, "stone")
        if lod:
            # Keep large window rhythms at distance; omit balcony structure.
            for bay in range(5):
                z0 = north + 7 + bay * (south - north - 14) / 5
                mesh.box(west - .3, y + 5, z0, west, y + 13,
                         z0 + (south - north - 14) / 5 * .65, "glass")
            continue
        # River-facing facade: doors, windows, projecting slabs, glazed guards.
        bays = 6 if style == "wave" else 5
        span = (south - north - 12) / bays
        for bay in range(bays):
            z0 = north + 6 + bay * span
            z1 = z0 + span * .88
            glass = "lit_glass" if (floor * 7 + bay * 3) % 13 == 0 else "glass"
            if level == 0:
                mesh.box(west - .35, y + 4, z0 + 2, west + .15,
                         y + 14, z1 - 2, glass)
                for edge in (z0 + 2, z1 - 3):
                    mesh.box(west - .7, y + 4, edge, west,
                             y + 14, edge + 1.2, "silver")
                mesh.box(west - .7, y + 14, z0 + 2, west,
                         y + 15.2, z1 - 2, "silver")
            else:
                x = west - .36
                mesh.quad((x,y+4,z1-2),(x,y+4,z0+2),(x,y+14,z0+2),(x,y+14,z1-2),(-1,0,0),glass)
            if floor > 0 and (style != "courtyard" or bay % 2 == floor % 2):
                curve = 2.5 * math.sin(bay * .8 + floor * .17) if style == "wave" else 0
                front = max(0, 2.5 + setback + curve)
                mesh.box(front, y + 1.1, z0, west + 2, y + 2.4, z1,
                         "stone", "pale")
                if level < 2:
                    mesh.box(front, y + 2.4, z0 + .6, front + .75,
                             y + 8.7, z1 - .6, "balcony_glass")
                    mesh.box(front - .2, y + 8.7, z0, front + 1.2,
                             y + 9.5, z1, "silver")
                    for end in (z0, z1 - 1):
                        mesh.box(front, y + 2.4, end, west,
                                 y + 8.5, end + 1, "balcony_glass")
                else:
                    guard = front - .2
                    mesh.quad((guard,y+2.4,z1-.6),(guard,y+2.4,z0+.6),
                              (guard,y+8.7,z0+.6),(guard,y+8.7,z1-.6),(-1,0,0),"balcony_glass")
                    for end in (z0, z1):
                        corners = ((front,y+2.4,end),(west,y+2.4,end),
                                   (west,y+8.5,end),(front,y+8.5,end))
                        if end == z1: corners = tuple(reversed(corners))
                        mesh.quad(*corners,(0,0,-1 if end == z0 else 1),"balcony_glass")
        # Courtyard and street elevations retain articulated window rhythms.
        for bay in range(5):
            z0 = north + 7 + bay * (south - north - 14) / 5
            z1 = z0 + (south - north - 14) / 5 * .64
            if level == 0:
                mesh.box(east, y + 4, z0, east + .45, y + 13, z1, "glass")
            else:
                x = east + .46
                mesh.quad((x,y+4,z0),(x,y+4,z1),(x,y+13,z1),(x,y+13,z0),(1,0,0),"glass")
        for bay in range(4):
            x0 = west + 11 + bay * (east - west - 22) / 4
            x1 = x0 + (east - west - 22) / 4 * .62
            if level == 0 or (level == 1 and style == "courtyard"):
                mesh.box(x0, y + 4, south, x1, y + 13, south + .45, "glass")
                mesh.box(x0, y + 4, north - .45, x1, y + 13, north, "glass")
            else:
                z = south + .46
                mesh.quad((x1,y+4,z),(x0,y+4,z),(x0,y+13,z),(x1,y+13,z),(0,0,1),"glass")
                z = north - .46
                mesh.quad((x0,y+4,z),(x1,y+4,z),(x1,y+13,z),(x0,y+13,z),(0,0,-1),"glass")
        if level < 3:
            mesh.box(west - 1, y + floor_height - 1.1, north,
                     east + .5, y + floor_height, south, "pale")
    top = 2 + floors * floor_height
    mesh.box(12, top, 6, width - 6, top + 2.4, depth - 6, "roof")
    for z in (5, depth - 8):
        mesh.box(10, top + 2.4, z, width - 5, top + 5.5, z + 3,
                 "pale")
    mesh.box(width * .54, top + 2.4, depth * .38,
             width * .76, top + 12, depth * .66, "silver", "roof")
    if not lod and level < 2:
        # Ground-floor shopfronts, entrance canopy, and rooftop planting.
        for bay in range(4):
            z0 = 14 + bay * (depth - 28) / 4
            mesh.box(14.4, 3, z0, 15, 14, z0 + (depth - 28) / 4 * .74,
                     "retail")
        mesh.box(2, 15.0, depth * .42, 26, 17, depth * .60, "canopy")
        if style in ("terrace", "bayfront"):
            for index in range(6):
                x = 19 + index * (width - 38) / 6
                mesh.box(x, top + 2.4, depth - 23,
                         x + 8, top + 5.8, depth - 15, "planter")
        if style == "bayfront":
            for floor in range(1, floors):
                y = 2 + floor * floor_height
                mesh.box(width * .18, y + 1, depth - 6,
                         width * .82, y + 2.4, depth, "stone")
                mesh.box(width * .18, y + 2.4, depth - .8,
                         width * .82, y + 8.3, depth, "balcony_glass")
    return mesh


def authored_lods():
    reports = []
    for style in ("wave", "terrace", "courtyard", "bayfront"):
        base = building(style)
        base_triangles = len(base.vertices) // 36
        levels = [{"file": "marina-" + style + ".m3d", "triangles": base_triangles,
                   "ratio": 1.0, "target_ratio": 1.0}]
        for level in (1, 2, 3):
            reduced = building(style, level=level) if level < 3 else coarse_building(style)
            # LOD3 retains individual window rhythms but replaces the balcony
            # bay structure by one slab/guard per occupied floor.
            record = reduced.save_indexed(f"marina-{style}-lod{level}")
            record.update(ratio=record["triangles"] / base_triangles,
                          target_ratio=(0.5, 0.2, 0.08)[level - 1])
            levels.append(record)
        reports.append({"asset": "marina/" + "marina-" + style,
                        "authoring": "Original parameterized geometry; seams kept per quad",
                        "levels": levels})
        catalog = OUT / ("marina-" + style + ".lod")
        catalog.write_text("MCLOD1\n" + "\n".join(
            f"marina/marina-{style}{'' if level == 0 else '-lod'+str(level)} {threshold}"
            for level, threshold in enumerate((500, 240, 80, 0))) + "\n", encoding="utf-8")
    (OUT / "AUTHORED_LODS.json").write_text(json.dumps(reports, indent=2) + "\n", encoding="utf-8")
    print(json.dumps([(item["asset"], [round(level["ratio"], 3) for level in item["levels"]])
                      for item in reports]))


def coarse_building(style):
    # Keep window rhythms, footprint, roof and parapets. Collapse the detailed
    # balcony bays to one slab/guard per floor for the smallest projected size.
    mesh = Mesh()
    width, depth = (250, 195) if style == "bayfront" else (200, 180)
    floors = 9 if style == "wave" else 8
    mesh.box(0, 0, 0, width, 2, depth, "stone", "concrete")
    for floor in range(floors):
        y = 2 + floor * 17
        setback = max(0, floor - 4) * 2.2 if style == "terrace" else 0
        west, east, north, south = 15 + setback, width - 8, 8, depth - 8
        wall = "pale" if style == "wave" else "plaster" if floor % 3 else "sand"
        mesh.box(west, y, north, east, y + 17, south, wall, "stone")
        if floor > 0:
            front = min(max(0, 2.5 + setback +
                            (2.5 * math.sin(bay * .8 + floor * .17) if style == "wave" else 0))
                        for bay in range(6 if style == "wave" else 5))
            mesh.box(front, y + 1.1, north + 6, west + 2, y + 2.4, south - 6, "stone", "pale")
            guard = front - .2
            mesh.quad((guard,y+2.4,south-6),(guard,y+2.4,north+6),
                      (guard,y+8.7,north+6),(guard,y+8.7,south-6),(-1,0,0),"balcony_glass")
        for side, bays in ((0, 6 if style == "wave" else 5), (1, 5), (2, 4), (3, 4)):
            for bay in range(bays):
                if side < 2:
                    z0 = north + 7 + bay * (south - north - 14) / bays
                    z1 = z0 + (south - north - 14) / bays * .64
                    x = west - .36 if side == 0 else east + .46
                    corners = ((x,y+4,z1),(x,y+4,z0),(x,y+13,z0),(x,y+13,z1))
                    if side == 1: corners = tuple(reversed(corners))
                    mesh.quad(*corners,(-1 if side == 0 else 1,0,0),"glass")
                else:
                    x0 = west + 11 + bay * (east - west - 22) / bays
                    x1 = x0 + (east - west - 22) / bays * .62
                    z = south + .46 if side == 2 else north - .46
                    corners = ((x1,y+4,z),(x0,y+4,z),(x0,y+13,z),(x1,y+13,z))
                    if side == 3: corners = tuple(reversed(corners))
                    mesh.quad(*corners,(0,0,1 if side == 2 else -1),"glass")
    top = 2 + floors * 17
    mesh.box(12, top, 6, width - 6, top + 2.4, depth - 6, "roof")
    for z in (5, depth - 8):
        mesh.box(10, top + 2.4, z, width - 5, top + 5.5, z + 3, "pale")
    mesh.box(width*.54, top+2.4, depth*.38, width*.76, top+12, depth*.66, "silver", "roof")
    return mesh


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--lods-only", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    if args.lods_only:
        authored_lods()
        return
    png(OUT / "MarinaFacade_Color.png")
    png(OUT / "MarinaFacade_NormalDX.png", normal=True)
    for style in ("wave", "terrace", "courtyard", "bayfront"):
        building(style).save("marina-" + style)
        building(style, lod=True).save("marina-" + style + "-lod")
    authored_lods()


if __name__ == "__main__":
    main()
