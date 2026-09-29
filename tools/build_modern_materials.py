"""Original seamless 2K PBR surfaces. Requires NumPy and Pillow; no source images.

One UV repeat represents 1.667 metres in the modern mesh authoring convention.
Normals are derivatives of the same periodic height fields used for colour and
roughness. Maps contain surface variation, never baked sunlight or reflections.
"""
import hashlib
import json
from pathlib import Path
import numpy as np
import PIL
from PIL import Image

SIZE = 2048
METRES_PER_REPEAT = 1 / .6


def noise(rng, rows, cols):
    """Periodic smooth value noise, sampled at texel centres."""
    grid = rng.uniform(-1, 1, (rows, cols)).astype(np.float32)
    y = (np.arange(SIZE, dtype=np.float32) + .5) * rows / SIZE
    x = (np.arange(SIZE, dtype=np.float32) + .5) * cols / SIZE
    iy, ix = y.astype(int), x.astype(int)
    fy, fx = y - iy, x - ix
    fy = (fy * fy * (3 - 2 * fy))[:, None]
    fx = (fx * fx * (3 - 2 * fx))[None, :]
    a = grid[iy[:, None], ix[None, :]]
    b = grid[iy[:, None], (ix[None, :] + 1) % cols]
    c = grid[(iy[:, None] + 1) % rows, ix[None, :]]
    d = grid[(iy[:, None] + 1) % rows, (ix[None, :] + 1) % cols]
    return ((a * (1 - fx) + b * fx) * (1 - fy) +
            (c * (1 - fx) + d * fx) * fy).astype(np.float32)


def surface(name, pattern):
    seed = int.from_bytes(hashlib.sha256(name.encode()).digest()[:8], 'little')
    rng = np.random.default_rng(seed)
    macro = noise(rng, 8, 8)
    grain = noise(rng, 128, 128)
    fine = noise(rng, 512, 512)
    micro = rng.uniform(-1, 1, (SIZE, SIZE)).astype(np.float32)
    u = (np.arange(SIZE, dtype=np.float32)[None, :] + .5) / SIZE
    v = (np.arange(SIZE, dtype=np.float32)[:, None] + .5) / SIZE
    ao = np.ones((SIZE, SIZE), dtype=np.float32)
    if name == 'foliage':
        veins = np.maximum(0, np.cos(u * np.pi * 32 + np.sin(v * np.pi * 8))) ** 14
        variation = macro * .11 + grain * .04 + veins * .045
        height = grain * .000035 + veins * .00012
        rough = .92 + fine * .045
    elif pattern == 'stone':
        if name == 'concrete':
            pits = np.maximum(0, -fine - .30) ** 2
            height = grain * .0005 + fine * .00008 - pits * .0008
            variation = macro * .045 + grain * .025 - pits * .14 + micro * .008
            ao -= pits * .20
            rough = .95 + grain * .04 - pits * .10
        else:
            flecks = np.maximum(0, fine - .16) ** 2
            variation = macro * .045 + grain * .075 - flecks * .30 + micro * .015
            height = grain * .00012 + fine * .000065
            rough = .88 + grain * .06 + flecks * .08
    elif pattern == 'wood':
        warp = noise(rng, 8, 16) * .20
        fibres = np.sin(v * np.pi * 192 + warp * 8 + np.sin(u * np.pi * 4) * 2)
        rings = np.sin(v * np.pi * 28 + warp * 4 + np.sin(u * np.pi * 2))
        pores = noise(rng, 768, 32)
        variation = macro * .075 + rings * .075 + fibres * .032 + pores * .02
        height = rings * .0001 + fibres * .00004 + pores * .000025
        rough = .90 + fibres * .045 + macro * .04
        ao -= np.maximum(0, -pores - .3) * .05
    elif pattern == 'brushed':
        strokes = noise(rng, 1024, 16)
        variation = macro * .025 + strokes * .035 + micro * .004
        height = strokes * .000009 + fine * .000001
        rough = .90 + strokes * .055 + macro * .02
    elif pattern == 'rubber':
        height = grain * .00005 + fine * .00002
        variation = macro * .04 + fine * .045 + micro * .012
        rough = .96 + fine * .035
    elif pattern == 'lens':
        flutes = np.cos(u * np.pi * 96)
        height = np.broadcast_to(flutes * .00015, (SIZE, SIZE)).copy()
        variation = np.broadcast_to(flutes * .012, (SIZE, SIZE)).copy()
        rough = np.ones((SIZE, SIZE), dtype=np.float32) * .94 + fine * .012
    else:
        # Glass remains optically smooth; paint has fine orange-peel structure.
        glass = name in ('glass', 'car_glass')
        # Sub-8-bit optical perturbations quantize to alternating 127/128
        # normals and create grain in a smooth sun highlight. Keep panes flat.
        height = np.zeros_like(fine) if glass else fine * .000006
        variation = fine * (.0015 if glass else .008) + macro * (.001 if glass else .008)
        rough = .96 + fine * (.004 if glass else .025)
    dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * SIZE / (2 * METRES_PER_REPEAT)
    dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * SIZE / (2 * METRES_PER_REPEAT)
    vectors = np.stack((-dx, -dy, np.ones_like(dx)), axis=2)
    vectors /= np.linalg.norm(vectors, axis=2, keepdims=True)
    return variation, vectors, np.clip(ao, .75, 1), np.clip(rough, .7, 1)


def build(materials, out):
    report = {'schema': 1, 'generator': 'build_modern_materials.py', 'size': SIZE,
              'metres_per_repeat': METRES_PER_REPEAT,
              'dependencies': {'numpy': np.__version__, 'pillow': PIL.__version__}, 'materials': []}
    for name, (colour, roughness, metalness, emission, pattern) in materials.items():
        variation, vectors, ao, rough = surface(name, pattern)
        base = np.empty((SIZE, SIZE, 4), dtype=np.uint8)
        base[:, :, :3] = np.clip(np.asarray(colour)[None, None, :] *
                                (1 + variation[:, :, None]), 0, 255).round().astype(np.uint8)
        base[:, :, 3] = 255
        normal = np.full_like(base, 255)
        normal[:, :, :3] = ((vectors * .5 + .5) * 255).round().astype(np.uint8)
        orm = np.full_like(base, 255)
        orm[:, :, 0] = (ao * 255).round().astype(np.uint8)
        orm[:, :, 1] = (rough * 255).round().astype(np.uint8)
        entry = {'name': name, 'pattern': pattern, 'roughness_factor': roughness,
                 'metallic_factor': metalness, 'maps': {}}
        for channel, pixels in [('base', base), ('normal', normal), ('orm', orm)]:
            path = out / f'{name}-{channel}.png'
            Image.fromarray(pixels).save(path, compress_level=6)
            entry['maps'][channel] = {'file': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
        report['materials'].append(entry)
        print(f'Authored {name}: {SIZE}x{SIZE}', flush=True)
    (out / 'materials.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
