"""Verify retained builder assets and the reproducibility of all 20 rock tiles.

Run with the bundled Python/Pillow runtime. Appearance is reviewed separately
in the texture sheet and native DX11 views; file uniqueness is not an art test.
"""
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from build_builder_assets import ROCKS
from builder_rock_materials import texture


def main():
    manifest = json.loads((ROOT / "assets/models/source/builder/manifest.json").read_text())
    assert manifest["catalog_items"] == 67 and manifest["components"] == 3
    assert len(manifest["assets"]) == 70
    assets = {item["id"]: item for item in manifest["assets"]}
    assert len(assets) == 70
    for name, entry in assets.items():
        folder = ROOT / "assets/models/baked/builder"
        paths = {"sha256": folder / (name + ".m3d"), "texture_sha256": folder / (name + ".png"),
                 "icon_sha256": folder / (name + ".icon.png"), "source_sha256": ROOT / entry["source"]}
        for key, path in paths.items():
            assert hashlib.sha256(path.read_bytes()).hexdigest() == entry[key], (name, key)
        binary = paths["sha256"].read_bytes()
        assert binary[:4] == b"M3D1", name
        count = struct.unpack_from("<I", binary, 4)[0]
        assert count > 0 and count % 3 == 0 and len(binary) == 8 + count * 48, name
        assert count // 3 == entry["triangles"], name
        vertices = list(struct.iter_unpack("<12f", binary[8:]))
        for vertex in vertices:
            assert all(math.isfinite(value) for value in vertex), name
            assert abs(sum(value ** 2 for value in vertex[3:6]) - 1) < .001, name
        with Image.open(paths["texture_sha256"]) as image:
            assert image.size == (256, 256) and image.mode == "RGBA", name
            assert image.getextrema()[3] == (255, 255), name
        with Image.open(paths["icon_sha256"]) as image:
            assert image.size == (96, 96) and image.mode == "RGBA", name
            assert image.getextrema()[3] == (0, 255), name
    for name, _, color in ROCKS:
        entry = assets[name]
        assert entry["material_recipe"]["version"] == 2
        binary = (ROOT / f"assets/models/baked/builder/{name}.m3d").read_bytes()
        vertices = list(struct.iter_unpack("<12f", binary[8:]))
        # Keep the same unit cube and outward normals. Only UVs/materials may
        # change the block's appearance; collision still spans exactly 40 units.
        assert len(vertices) == 36
        for axis, bounds in enumerate(((-.5, .5), (0, 1), (-.5, .5))):
            assert (min(v[axis] for v in vertices), max(v[axis] for v in vertices)) == bounds, name
        for face in range(6):
            group = vertices[face * 6:face * 6 + 6]
            assert {tuple(v[6:8]) for v in group} == {(0., 0.), (0., 1.), (1., 0.), (1., 1.)}, name
            for v in group:
                assert sum(a * b for a, b in zip((v[0], v[1] - .5, v[2]), v[3:6])) > .49, name
        with Image.open(ROOT / f"assets/models/baked/builder/{name}.png") as image:
            assert image.tobytes() == texture(name, color), (name, "recipe reproducibility")
    print("PASS: 70 model/texture/icon/OBJ hashes and binary/image layouts; all 20 rock recipes reproduce current pixels and preserve cube geometry with full-face UVs")


if __name__ == "__main__":
    main()
