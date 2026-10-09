"""Add original UV-mapped vehicle paint/glass details to retained source parts.

These source vehicles have material colors but no usable base-color images.
Run after the expansion baker, or directly on an existing bake. Geometry, part
pivots, indices and collision extents are preserved. Requires Pillow and NumPy.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/models/baked/vehicles"
SIZE = 1024


def maps(key="helicopter"):
    if key != "helicopter":
        return vehicle_maps(key)
    base = Image.new("RGB", (SIZE, 512), (223, 228, 231))
    orm = Image.new("RGB", base.size, (255, 102, 38))
    draw, surface = ImageDraw.Draw(base), ImageDraw.Draw(orm)

    def point(z, y):
        return (round((z + 85) / 170 * (SIZE - 1)), round((40 - y) / 64 * 511))

    def polygon(points, color, rough=102, metal=38):
        pixels = [point(*p) for p in points]
        draw.polygon(pixels, fill=color)
        surface.polygon(pixels, fill=(255, rough, metal))

    # Broad livery bands run continuously across the tail and fuselage.
    polygon([(-85, -3), (85, -3), (85, -24), (-85, -24)], (27, 48, 65))
    polygon([(-85, 3), (85, 3), (85, -2), (-85, -2)], (184, 42, 34))
    polygon([(-85, 40), (-51, 40), (-35, 9), (-85, 9)], (184, 42, 34))
    # Cabin and cockpit glazing, including painted frames and a windscreen divider.
    windows = [([(6, 7), (28, 7), (28, 20), (6, 20)], (38, 75, 95)),
               ([(33, 7), (48, 7), (48, 24), (33, 24)], (32, 66, 86)),
               ([(52, 7), (76, 7), (69, 24), (52, 24)], (27, 59, 79))]
    for shape, color in windows:
        polygon(shape, color, 35, 18)
        draw.line([point(*p) for p in shape + [shape[0]]], fill=(17, 31, 42), width=4)
        a, b = shape[0], shape[1]
        draw.line([point(a[0] + 2, a[1] + 2), point(b[0] - 2, b[1] + 2)], fill=(93, 140, 160), width=3)
    # Door outlines, access panels and fasteners are painted into the actual UV map.
    for z0, z1 in ((3, 30), (31, 50)):
        draw.line([point(z0, 23), point(z0, -13), point(z1, -13), point(z1, 23)], fill=(110, 122, 130), width=2)
        draw.line([point(z1 - 8, 4), point(z1 - 3, 4)], fill=(23, 32, 40), width=4)
    for z in range(-60, 2, 4):
        for y in (7, -7):
            x, yy = point(z, y)
            draw.ellipse((x - 1, yy - 1, x + 1, yy + 1), fill=(98, 113, 123))
    for z in range(-3, 2):
        draw.line([point(z * 2, 24), point(z * 2, 31)], fill=(25, 35, 40), width=3)
    font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 22)
    draw.text(point(-42, 19), "MC-04", font=font, fill=(30, 43, 53))
    graphite = Image.new("RGB", (256, 256), (43, 49, 54))
    g = ImageDraw.Draw(graphite)
    for row in range(0, 256, 32):
        g.line((0, row, 255, row), fill=(24, 29, 33), width=2)
    for x in (12, 243):
        g.line((x, 0, x, 255), fill=(79, 90, 100), width=4)
    return {
        "paint-base": base, "paint-orm": orm,
        "paint-normal": Image.new("RGB", base.size, (128, 128, 255)),
        "graphite-base": graphite,
        "graphite-orm": Image.new("RGB", graphite.size, (255, 140, 153)),
        "graphite-normal": Image.new("RGB", graphite.size, (128, 128, 255)),
    }


def vehicle_maps(key):
    """Original light-aircraft livery and material detail for the two bikes."""
    size = (1024, 512)
    base = Image.new("RGB", size, (229, 232, 234) if key == "airplane" else (235, 239, 243))
    orm = Image.new("RGB", size, (255, 90, 50))
    draw, surface = ImageDraw.Draw(base), ImageDraw.Draw(orm)
    if key in ("tank", "truck"):
        return heavy_vehicle_maps(key)
    if key == "airplane":
        def point(z, y):
            return (round((z + 55) / 110 * 1023), round((35 - y) / 51 * 511))
        for ya, yb, color in ((1, 5, (175, 35, 30)), (-3, 1, (31, 58, 87))):
            draw.rectangle((*point(-55, yb), *point(55, ya)), fill=color)
        # The cabin map follows the fuselage in longitudinal/vertical space.
        for za, zb in ((-13, -3), (0, 12)):
            pixels = [point(za, 7), point(zb, 7), point(zb, 17), point(za, 17)]
            draw.polygon(pixels, fill=(34, 73, 100));surface.polygon(pixels, fill=(255, 32, 18))
            draw.line(pixels + [pixels[0]], fill=(20, 39, 54), width=4)
        for z in (-16, 15, 30):
            draw.line((point(z, -12), point(z, 20)), fill=(107, 117, 124), width=2)
        font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 24)
        draw.text(point(-45, 15), "MC-156", font=font, fill=(26, 47, 70))
    else:
        # Neutral detail modulates the source's authored frame/fairing colors.
        # The small panels keep the original color/material sections distinct.
        draw.rectangle((0, 280, 1023, 300), fill=(165, 175, 184))
        for x in range(80, 1024, 220):
            draw.line((x, 0, x, 511), fill=(182, 191, 199), width=2)
            for y in (40, 245, 470):
                draw.ellipse((x - 3, y - 3, x + 3, y + 3), fill=(125, 137, 148))
        if key == "motorcycle":
            draw.polygon([(650, 150), (960, 150), (900, 175), (610, 175)], fill=(245, 245, 245))
            draw.line((0, 380, 1023, 380), fill=(123, 138, 151), width=3)
    graphite = Image.new("RGB", (256, 256), (145, 151, 157))
    g = ImageDraw.Draw(graphite)
    for row in range(0, 256, 24):
        g.line((0, row, 255, row + 12), fill=(96, 102, 108), width=3)
    for x in (10, 245):
        g.line((x, 0, x, 255), fill=(193, 198, 203), width=2)
    return {"paint-base": base, "paint-orm": orm,
            "paint-normal": Image.new("RGB", size, (128, 128, 255)),
            "graphite-base": graphite,
            "graphite-orm": Image.new("RGB", graphite.size, (255, 173, 26)),
            "graphite-normal": Image.new("RGB", graphite.size, (128, 128, 255))}


def heavy_vehicle_maps(key):
    """Neutral paint detail multiplies the original olive/red material colors."""
    rng = np.random.default_rng(902 if key == "tank" else 903)
    noise = rng.normal(0, 2.2, (512, 1024, 1))
    base = Image.fromarray(np.uint8(np.clip(230 + noise + np.zeros((512, 1024, 3)), 0, 255)))
    draw = ImageDraw.Draw(base)
    if key == "tank":
        for _ in range(65):
            x, y = rng.integers(0, 1024), rng.integers(0, 512)
            width, height = rng.integers(50, 145), rng.integers(20, 60)
            draw.polygon([(x, y), (x+width//3, y-height//2), (x+width, y),
                          (x+width*3//4, y+height), (x+width//4, y+height//2)],
                         fill=(174, 182, 162))
    for x in range(0, 1024, 256):
        draw.line((x, 0, x, 511), fill=(120, 126, 129), width=2)
        draw.line((x+3, 0, x+3, 511), fill=(245, 245, 240), width=1)
        for y in range(16, 512, 64):
            draw.ellipse((x+7, y-2, x+11, y+2), fill=(96, 101, 102))
    for y in (32, 478):
        draw.line((0, y, 1023, y), fill=(165, 170, 169), width=2)
    for _ in range(150):
        x,y=rng.integers(0,1024),rng.integers(400,512)
        draw.line((x,y,x+int(rng.integers(2,16)),y-2),fill=(133,125,111),width=1)
    graphite=Image.new("RGB",(256,256),(172,176,177));g=ImageDraw.Draw(graphite)
    for y in range(0,256,24):
        g.line((0,y,255,y+20),fill=(85,91,93),width=7)
        g.line((0,y+9,255,y+29),fill=(216,219,218),width=2)
    def normal(image):
        height=np.asarray(image,dtype=np.float32).mean(axis=2)/255
        dy,dx=np.gradient(height)
        vectors=np.stack((-dx*2,-dy*2,np.ones_like(height)),axis=2)
        vectors/=np.linalg.norm(vectors,axis=2,keepdims=True)
        return Image.fromarray(np.uint8(np.clip((vectors*.5+.5)*255,0,255)))
    return {"paint-base":base,"paint-normal":normal(base),
            "paint-orm":Image.new("RGB",base.size,(255,153 if key=="tank" else 104,55)),
            "graphite-base":graphite,"graphite-normal":normal(graphite),
            "graphite-orm":Image.new("RGB",graphite.size,(255,188,20))}


def apply(verify=False, key="helicopter"):
    generated = maps(key)
    for name, image in generated.items():
        path = OUT / f"expansion-{key}-{name}.png"
        if verify:
            with Image.open(path) as actual:
                assert actual.size == image.size and actual.convert("RGB").tobytes() == image.tobytes(), path
        else:
            image.save(path)
    report_path = OUT / f"expansion-{key}.import.json"
    report = json.loads(report_path.read_text())
    hashes = {}
    for part in report["parts"]:
        path = OUT / (part["mesh"].split("/")[-1] + ".m3d")
        raw = path.read_bytes()
        magic, count, indices = struct.unpack_from("<4sII", raw)
        assert magic == b"M3D2"
        vertices = np.frombuffer(raw, dtype="<f4", count=count * 12, offset=12).reshape(-1, 12).copy()
        geometry = vertices[:, :6].tobytes() + raw[12 + count * 48:]
        materials = path.with_suffix(".pbr").read_text().splitlines()
        sections = []
        previous = None
        for line in materials:
            if line.startswith("# HELICOPTER_SURFACE ") or line.startswith("# VEHICLE_SURFACE "):
                previous = line.split()[-1]
            elif line and not line.startswith("#"):
                fields = line.split()
                first, length = map(int, fields[:2])
                ids = np.frombuffer(raw, dtype="<u4", count=length, offset=12 + count * 48 + first * 4)
                color = vertices[ids[0], 8:11]
                kind = previous or ("graphite" if max(color) < .1 else "paint")
                previous = None
                sections.append((fields, ids, kind))
        if verify:
            assert all(fields[5] != "-" and np.ptp(vertices[ids, 6:8], axis=0).max() > .001
                       for fields, ids, kind in sections), path
        else:
            lines = []
            for fields, ids, kind in sections:
                p = vertices[ids, :3] + np.array(part["center"])
                if kind == "paint":
                    if key == "helicopter":
                        uv = np.column_stack(((p[:, 2] + 85) / 170, (40 - p[:, 1]) / 64))
                    elif key == "airplane":
                        uv = np.column_stack(((p[:, 2] + 55) / 110, (35 - p[:, 1]) / 51))
                    elif key in ("tank", "truck"):
                        # World-space projection with face-specific axes preserves detail
                        # on roofs, ends and side panels; authored normals split cube edges.
                        normals = np.abs(vertices[ids, 3:6])
                        dominant = np.argmax(normals, axis=1)
                        uv = np.empty((len(ids), 2), dtype=np.float32)
                        for axis, axes in ((0, (2, 1)), (1, (0, 2)), (2, (0, 1))):
                            mask = dominant == axis
                            uv[mask] = .02 + .96 * (p[mask][:, axes] + 80) / 160
                    else:
                        uv = np.column_stack(((p[:, 2] + 20) / 40, (12 - p[:, 1]) / 26))
                else:
                    # Planar mapping along the two widest axes, also for spinning blades.
                    axes = np.argsort(np.ptp(p, axis=0))[-2:]
                    q = p[:, axes]
                    uv = .02 + .96 * (q - q.min(axis=0)) / np.maximum(np.ptp(q, axis=0), .001)
                vertices[ids, 6:8] = np.clip(uv, .001, .999)
                if key in ("helicopter", "airplane"):
                    vertices[ids, 8:12] = 1
                fields[2:5] = ["0.4" if kind == "paint" else "0.55", "0.15" if kind == "paint" else "0.6", "0"]
                fields[5:8] = [f"expansion-{key}-{kind}-{suffix}.png" for suffix in ("base", "normal", "orm")]
                lines.extend(["# VEHICLE_SURFACE " + kind, " ".join(fields)])
            cooked = raw[:12] + vertices.astype("<f4").tobytes() + raw[12 + count * 48:]
            assert vertices[:, :6].tobytes() + cooked[12 + count * 48:] == geometry
            path.write_bytes(cooked)
            path.with_suffix(".pbr").write_text("\n".join(lines) + "\n")
        hashes[path.name] = hashlib.sha256(geometry).hexdigest()
    report["surface_geometry_sha256"] = hashes
    report["surface_modifications"] = "Original project-authored livery/material details and mechanical maps; projected UVs. Source geometry and rigid pivots unchanged. " + key
    if not verify:
        report_path.write_text(json.dumps(report, indent=2) + "\n")
    else:
        assert hashes == json.loads(report_path.read_text())["surface_geometry_sha256"]
    print(key + " surface " + ("verification" if verify else "bake") + f" passed: {len(hashes)} unchanged rigid parts; six texture maps.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--vehicle", choices=("helicopter", "airplane", "motorcycle", "bicycle", "tank", "truck"))
    args = parser.parse_args()
    for key in ((args.vehicle,) if args.vehicle else ("helicopter", "airplane", "motorcycle", "bicycle", "tank", "truck")):
        apply(args.verify, key)
