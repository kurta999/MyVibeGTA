"""Identify window triangles in the existing baked vehicle meshes.

Keep the original meshes intact. DX11 splits the listed triangles into a glass
pass on load. Run again after rebaking vehicles (requires Pillow).
"""
import pathlib
import struct
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[1]
for name in [f"traffic-{i}" for i in range(1, 6)] + ["sports-car", "sedan"]:
    path = ROOT / "assets/models/baked/vehicles" / (name + ".m3d")
    vertices = list(struct.iter_unpack("<12f", path.read_bytes()[8:]))
    low, high = min(v[1] for v in vertices), max(v[1] for v in vertices)
    image = Image.open(path.with_suffix(".png")).convert("RGB") if name.startswith("traffic") else None
    windows = []
    for first in range(0, len(vertices), 3):
        tri = vertices[first:first + 3]
        if min(v[1] for v in tri) < low + (high - low) * .54:
            continue
        if image:
            u, v = [sum(p[i] for p in tri) / 3 for i in (6, 7)]
            r, g, b = image.getpixel((min(image.width - 1, int(u * image.width)),
                                     min(image.height - 1, int(v * image.height))))
            glass = abs(g - b) <= 1 and 2 <= g - r <= 9 and g < 75
        else:
            # Kenney's dedicated black glass material, separate from tyres.
            glass = all(.02 < v[8] < .04 and abs(v[8] - v[9]) < .001 for v in tri)
        if glass:
            windows.append(first // 3)
    assert len(windows) >= 10, (name, len(windows))
    path.with_suffix(".glass").write_text("\n".join(map(str, windows)) + "\n")
    print(name, len(windows), "window triangles")
