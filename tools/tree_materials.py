"""Cook source-resolution tree foliage and replace solid canopy shells."""
from functools import cache
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile

import numpy as np
from PIL import Image
from cook_modern_textures import ROOT, TEXCONV, TEXCONV_SHA256

SOURCE = ROOT / 'assets/models/source/polyhaven'
OUT = ROOT / 'assets/models/baked/nature'
ALPHA_CUTOFF = .42

# The three island trees ship byte-identical leaf maps and share a material.
SCAN_LEAVES = {
    'tree_small_02': ('leaves', 'tree_small_02'),
    'island_tree_01': ('leaves', 'island_tree_01'),
    'island_tree_02': ('leaves', 'island_tree_01'),
    'island_tree_03': ('leaves', 'island_tree_01'),
    'jacaranda_tree': ('leaves', 'jacaranda_tree'),
    'fir_sapling': ('twigs', 'fir_sapling'),
    'pine_sapling_small': ('twig', 'pine_sapling_small'),
    'quiver_tree_01': ('leaf', 'quiver_tree_01'),
}

# Each island UV box encloses a whole scanned leaf and its stem. The
# jacaranda box encloses the compound-leaf spray at the top of its atlas.
BROADLEAF = ('island_tree_01', tuple(
    tuple(edge / 256 for edge in bounds)
    for bounds in ((3, 3, 39, 106), (41, 3, 88, 102), (91, 6, 125, 94),
                   (131, 10, 168, 94), (176, 3, 210, 107))))
FEATHERED = ('jacaranda_tree', ((.16, .025, .985, .475),))
CROWNS = {
    'tree_blocks': ('leaf', BROADLEAF),
    'tree_fat': ('leaf', BROADLEAF),
    'tree_simple': ('leaf', BROADLEAF),
    'tree_palmDetailedShort': ('leaf', BROADLEAF),
    'tree_palmTall': ('leaf', BROADLEAF),
    'tree_plateau': ('leafgreen', FEATHERED),
    'tree_small': ('leafgreen', BROADLEAF),
    'tree_urbanCherry': ('blossom', FEATHERED),
}


def scan_material(path, name):
    scan = SCAN_LEAVES.get(path.stem)
    return scan[1] if scan and name == f'{path.stem}_{scan[0]}' else None


@cache
def cooked_foliage(scan, blossom=False):
    if not TEXCONV.is_file() or hashlib.sha256(TEXCONV.read_bytes()).hexdigest() != TEXCONV_SHA256:
        raise RuntimeError('Run tools/bootstrap_texconv.ps1 before cooking tree materials')
    prefix = SCAN_LEAVES[scan][0]
    folder = SOURCE / scan / 'textures'
    stem = f'{scan}_{prefix}'
    diffuse = folder / f'{stem}_diff_2k.jpg'
    normal = folder / f'{stem}_nor_dx_2k.jpg'
    alpha = folder / f'{stem}_alpha_2k.png'
    rgba = np.array(Image.open(diffuse).convert('RGBA'))
    assert rgba.shape == (2048, 2048, 4)
    if alpha.exists():
        image = Image.open(alpha)
        mask = np.asarray(image.convert('L') if 'R' in image.getbands() else image)
        if mask.dtype.itemsize > 1:
            mask = (mask.astype(np.float32) / 257).round()
        rgba[:, :, 3] = mask.astype(np.uint8)
    else:
        assert scan == 'quiver_tree_01'  # Its leaf outline is modeled in geometry.
    if blossom:
        # Preserve the ornamental tree's pink palette over the scanned veins.
        intensity = rgba[:, :, :3].astype(np.float32).mean(axis=2) / 255
        rgba[:, :, :3] = np.clip(np.array([119, 56, 73]) +
            intensity[:, :, None] * np.array([187, 177, 179]), 0, 255).astype(np.uint8)
    base_name = f'foliage-{scan}{"-blossom" if blossom else ""}'
    normal_name = f'foliage-{scan}-normal'
    with tempfile.TemporaryDirectory(prefix='tree-foliage-', dir=ROOT / 'build-tools') as directory:
        working = Path(directory)
        base = working / f'{base_name}.png'
        Image.fromarray(rgba).save(base)
        normal_input = working / f'{normal_name}.jpg'
        shutil.copyfile(normal, normal_input)
        for image, fmt, options in (
            (base, 'BC7_UNORM_SRGB', ['-srgb', '--keep-coverage', str(ALPHA_CUTOFF)]),
            (normal_input, 'BC5_UNORM', []),
        ):
            subprocess.run([str(TEXCONV), '-nologo', '-y', '-dx10', '-m', '0',
                            '-f', fmt, *options, '-o', directory, str(image)],
                           check=True, stdout=subprocess.DEVNULL)
            image.with_suffix('.dds').replace(OUT / f'{image.stem}.dds')
    return f'{base_name}.dds', f'{normal_name}.dds', scan != 'quiver_tree_01'


def normalize(vectors):
    return vectors / np.maximum(np.linalg.norm(vectors, axis=1, keepdims=True), 1e-8)


def crown_sprigs(source, uv_boxes, rng):
    """Scatter textured sprigs through the volume described by a low-poly crown."""
    triangles = source[:, :3].reshape((-1, 3, 3))
    crosses = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
    areas = np.linalg.norm(crosses, axis=1) * .5
    count = int(np.clip(len(triangles), 750, 2200))
    chosen = rng.choice(len(triangles), count, p=areas / areas.sum())
    r = np.sqrt(rng.random(count))
    t = rng.random(count)
    weights = np.stack((1 - r, r * (1 - t), r * t), axis=1)
    centers = (triangles[chosen] * weights[:, :, None]).sum(axis=1)
    # A second layer just inside the surface fills the crown between sprigs.
    surface = normalize(crosses[chosen])
    normal = normalize(surface * .55 + normalize(rng.normal(size=(count, 3))))
    right = normalize(np.cross(normal, np.array([0., 1., 0.])))
    up = np.cross(right, normal)
    lean = rng.uniform(-1.2, 1.2, size=(count, 1))
    right, up = right * np.cos(lean) + up * np.sin(lean), up * np.cos(lean) - right * np.sin(lean)
    u0, v0, u1, v1 = np.asarray(uv_boxes)[rng.integers(len(uv_boxes), size=count)].T
    aspect = np.sqrt((u1 - u0) / (v1 - v0))[:, None]
    uv = np.stack((u0, v0, u0, v1, u1, v1, u1, v0), axis=1).reshape((-1, 4, 2))
    length = np.sqrt(areas.sum() / count)
    centers -= surface * (length * rng.uniform(.02, .55, size=(count, 1)))
    size = length * rng.uniform(1.25, 2.30, size=(count, 1))
    shade = rng.uniform(.76, 1.10, size=(count, 1))
    lower, upper = triangles.min(axis=(0, 1)), triangles.max(axis=(0, 1))

    def pack(n, scale):
        half_right = right[:n] * size[:n] * scale * aspect[:n] * .5
        half_up = up[:n] * size[:n] * scale / aspect[:n] * .5
        center = centers[:n]
        points = np.stack((center - half_right + half_up, center - half_right - half_up,
                           center + half_right - half_up, center + half_right + half_up), axis=1)
        vertices = np.ones((n, 4, 12), np.float32)
        vertices[:, :, :3] = points
        vertices[:, :, 3:6] = normal[:n, None, :]
        vertices[:, :, 6:8] = uv[:n]
        vertices[:, :, 8:11] = shade[:n, None, :]
        return vertices[:, (0, 1, 2, 0, 2, 3), :].reshape((-1, 12))

    near, lod = pack(count, 1.), pack(count // 3, 1.55)
    # Fit complete cards to the original crown bounds without flattening edges.
    start, end = near[:, :3].min(axis=0), near[:, :3].max(axis=0)
    for vertices in (near, lod):
        vertices[:, :3] = lower + (vertices[:, :3] - start) * (
            (upper - lower) / np.maximum(end - start, 1e-8))
    return near, lod
