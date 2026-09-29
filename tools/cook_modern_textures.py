"""Cook authored PNGs to BC7 colour/ORM, BC5 normal DDS with authored full mips.

Colour is filtered in linear light. Normal mips retain vector coherence in ORM
alpha, because BC5 stores only X/Y; the DX11 shader uses coherence to widen the
base roughness. Requires NumPy, Pillow and pinned DirectXTex May 2026 texconv.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'assets/models/baked/modern'
TEXCONV = ROOT / 'build-tools/directxtex-may2026/texconv.exe'
TEXCONV_SHA256 = 'dcfdec10244e02cf5037fba089c55fb7e1326b1c8181742d77d15fa5cb5eef06'


def average(a):
    h, w = a.shape[:2]
    return a.reshape(h // 2, 2, w // 2, 2, *a.shape[2:]).mean(axis=(1, 3))


def srgb_to_linear(a):
    return np.where(a <= .04045, a / 12.92, ((a + .055) / 1.055) ** 2.4)


def linear_to_srgb(a):
    return np.where(a <= .0031308, a * 12.92, 1.055 * np.maximum(a, 0) ** (1 / 2.4) - .055)


def mipmaps(base, normal, orm):
    colour = srgb_to_linear(base[:, :, :3] / 255.)
    vectors = normal[:, :, :3].astype(np.float32) / 127.5 - 1
    vectors /= np.linalg.norm(vectors, axis=2, keepdims=True)
    data = orm.astype(np.float32) / 255.
    chains = [[], [], []]
    while True:
        c = np.full((*colour.shape[:2], 4), 255, dtype=np.uint8)
        c[:, :, :3] = np.clip(linear_to_srgb(colour) * 255, 0, 255).round().astype(np.uint8)
        coherence = np.clip(np.linalg.norm(vectors, axis=2), 0, 1)
        unit = vectors / np.maximum(coherence[:, :, None], 1e-8)
        n = np.full_like(c, 255)
        n[:, :, :3] = np.clip((unit * .5 + .5) * 255, 0, 255).round().astype(np.uint8)
        o = np.clip(data * 255, 0, 255).round().astype(np.uint8)
        o[:, :, 3] = (coherence * 255).round().astype(np.uint8)
        for chain, pixels in zip(chains, (c, n, o)):
            chain.append(pixels)
        if colour.shape[0] == 1:
            break
        colour, vectors, data = average(colour), average(vectors), average(data)
    return chains


def write_rgba_dds(path, levels, srgb=False):
    h, w = levels[0].shape[:2]
    header = [0] * 37
    header[0:8] = [0x20534444, 124, 0x2100f, h, w, w * 4, 0, len(levels)]
    header[19:22] = [32, 4, 0x30315844]
    header[27] = 0x401008
    header[32:37] = [29 if srgb else 28, 3, 0, 1, 1]
    path.write_bytes(struct.pack('<37I', *header) + b''.join(a.tobytes() for a in levels))


def cook(texconv=TEXCONV, names=None):
    if not texconv.is_file() or hashlib.sha256(texconv.read_bytes()).hexdigest() != TEXCONV_SHA256:
        raise RuntimeError('Run tools/bootstrap_texconv.ps1 to install the pinned compressor')
    materials = json.loads((FOLDER / 'materials.json').read_text())
    known={m['name'] for m in materials['materials']}
    if names and not set(names)<=known:raise ValueError('Unknown material name')
    report = {'schema': 1, 'tool': 'Microsoft DirectXTex May 2026', 'tool_sha256': TEXCONV_SHA256,
              'compression': 'texconv default DirectCompute adapter; CPU fallback if unavailable',
              'colour_filter': 'linear-light box', 'normal_filter': 'vector average and renormalization',
              'coherence_channel': 'ORM alpha', 'textures': []}
    if names:
        previous=json.loads((FOLDER/'textures.json').read_text())
        changed={f'{name}-{channel}.dds' for name in names for channel in ('base','normal','orm')}
        report['textures']=[t for t in previous['textures'] if t['file'] not in changed]
    with tempfile.TemporaryDirectory(prefix='modern-cook-', dir=ROOT / 'build-tools') as directory:
        temp = Path(directory)
        for material in materials['materials']:
            name = material['name']
            if names and name not in names:continue
            maps = [np.asarray(Image.open(FOLDER / f'{name}-{channel}.png').convert('RGBA'))
                    for channel in ('base', 'normal', 'orm')]
            chains = mipmaps(*maps)
            for channel, levels, fmt in zip(('base', 'normal', 'orm'), chains,
                                           ('BC7_UNORM_SRGB', 'BC5_UNORM', 'BC7_UNORM')):
                path = temp / f'{name}-{channel}.dds'
                write_rgba_dds(path, levels, channel == 'base')
                command = [str(texconv), '-nologo', '-y', '-dx10', '-m', str(len(levels)),
                           '-f', fmt, '-o', str(FOLDER), str(path)]
                subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
                cooked = FOLDER / path.name
                header = struct.unpack('<37I', cooked.read_bytes()[:148])
                assert header[7] == len(levels) and header[32] == {'base':99,'normal':83,'orm':98}[channel]
                payload = cooked.stat().st_size - 148
                expected = sum(max(1, (a.shape[0]+3)//4) * max(1, (a.shape[1]+3)//4) * 16 for a in levels)
                assert payload == expected
                report['textures'].append({'file': cooked.name, 'sha256': hashlib.sha256(cooked.read_bytes()).hexdigest(),
                    'source_sha256': material['maps'][channel]['sha256'], 'width': header[4], 'height': header[3],
                    'mips': header[7], 'format': fmt, 'bytes': cooked.stat().st_size,
                    'uncompressed_bytes': sum(a.nbytes for a in levels)})
            print(f'Cooked {name}: {len(chains[0])} mips, BC7/BC5/BC7', flush=True)
    for item in report['textures']:
        assert hashlib.sha256((FOLDER/item['file']).read_bytes()).hexdigest()==item['sha256']
        assert hashlib.sha256((FOLDER/item['file'].replace('.dds','.png')).read_bytes()).hexdigest()==item['source_sha256']
    report['textures'].sort(key=lambda item:item['file'])
    (FOLDER / 'textures.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--texconv', type=Path, default=TEXCONV)
    parser.add_argument('names',nargs='*',help='Optional material subset; retain and verify the remaining recorded cook')
    args=parser.parse_args();cook(args.texconv,args.names)
