"""Cook original DX11 scene captures to SH irradiance, GGX cubes and DFG LUT.

No tone mapping or color compression is applied to lighting data. Tested with
NumPy 2.3.5; output uses the versioned MCPB1 reader in src/dx11_probes.cpp.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import time
import numpy as np

HOURS = (12.0, 18.5, 22.0)


def directions(size):
    axis = (np.arange(size, dtype=np.float64) + .5) * 2 / size - 1
    u, v = np.meshgrid(axis, axis)
    one = np.ones_like(u)
    faces = np.stack((np.stack((one, -v, -u), -1),
                      np.stack((-one, -v, u), -1),
                      np.stack((u, one, v), -1),
                      np.stack((u, -one, -v), -1),
                      np.stack((u, -v, one), -1),
                      np.stack((-u, -v, -one), -1)))
    return faces / np.linalg.norm(faces, axis=-1, keepdims=True)


def sh_basis(n):
    x, y, z = np.moveaxis(n, -1, 0)
    return np.stack((np.ones_like(x)*.282094792, .488602512*y,
                     .488602512*z, .488602512*x, 1.092548431*x*y,
                     1.092548431*y*z, .315391565*(3*z*z-1),
                     1.092548431*x*z, .546274215*(x*x-y*y)), -1)


def irradiance(cube):
    size = cube.shape[1]
    axis = (np.arange(size) + .5) * 2 / size - 1
    u, v = np.meshgrid(axis, axis)
    weights = 4 / (size*size * (1 + u*u + v*v)**1.5)
    weights *= 4*math.pi / (6*weights.sum())
    coefficient = np.einsum('fhwc,fhwi,hw->ic', cube[..., :3],
                            sh_basis(directions(size)), weights)
    # Cosine convolution divided by pi: irradiance * Lambert BRDF.
    coefficient *= np.array([1, 2/3, 2/3, 2/3, .25, .25, .25, .25, .25])[:, None]
    return np.concatenate((coefficient, np.zeros((9, 1))), -1)


def sample_cube(cube, n):
    """Bilinear sampling with D3D face orientation, arbitrary leading shape."""
    shape = n.shape[:-1]
    n = n.reshape(-1, 3)
    dominant = np.argmax(np.abs(n), axis=-1)
    x, y, z = n.T
    divisor = np.max(np.abs(n), axis=-1)
    face = dominant*2 + (n[np.arange(len(n)), dominant] < 0)
    us = np.choose(face, (-z, z, x, x, x, -x)) / divisor
    vs = np.choose(face, (-y, -y, z, -z, -y, -y)) / divisor
    size = cube.shape[1]
    px = np.clip((us+1)*size/2-.5, 0, size-1)
    py = np.clip((vs+1)*size/2-.5, 0, size-1)
    x0, y0 = px.astype(int), py.astype(int)
    x1, y1 = np.minimum(x0+1, size-1), np.minimum(y0+1, size-1)
    fx, fy = (px-x0)[:, None], (py-y0)[:, None]
    values = ((cube[face, y0, x0]*(1-fx) + cube[face, y0, x1]*fx)*(1-fy) +
              (cube[face, y1, x0]*(1-fx) + cube[face, y1, x1]*fx)*fy)
    return values.reshape(*shape, cube.shape[-1])


def hammersley(count):
    n = np.arange(count, dtype=np.uint32)
    bits = (n << 16) | (n >> 16)
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1)
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2)
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4)
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8)
    return np.stack((n.astype(float)/count, bits.astype(float)*2.3283064365386963e-10), -1)


def ggx_half(roughness, samples):
    xi = hammersley(samples)
    alpha2 = roughness**4
    cos_theta = np.sqrt((1-xi[:, 1]) / (1+(alpha2-1)*xi[:, 1]))
    sin_theta = np.sqrt(np.maximum(0, 1-cos_theta*cos_theta))
    phi = 2*math.pi*xi[:, 0]
    return np.stack((np.cos(phi)*sin_theta, np.sin(phi)*sin_theta, cos_theta), -1)


def prefilter(cube, size, roughness, samples=512):
    normal = directions(size).reshape(-1, 3)
    if roughness == 0:
        return sample_cube(cube, normal).reshape(6, size, size, 3)
    half_local = ggx_half(roughness, samples)
    output = np.empty_like(normal)
    for first in range(0, len(normal), 256):
        n = normal[first:first+256]
        up = np.zeros_like(n)
        up[:, 2] = 1
        up[np.abs(n[:, 2]) > .999] = (1, 0, 0)
        tangent = np.cross(up, n)
        tangent /= np.linalg.norm(tangent, axis=-1, keepdims=True)
        bitangent = np.cross(n, tangent)
        half = (tangent[:, None, :]*half_local[None, :, 0:1] +
                bitangent[:, None, :]*half_local[None, :, 1:2] +
                n[:, None, :]*half_local[None, :, 2:3])
        light = 2*np.sum(n[:, None, :]*half, -1, keepdims=True)*half - n[:, None, :]
        weights = np.maximum(0, np.sum(n[:, None, :]*light, -1))
        radiance = sample_cube(cube, light)
        output[first:first+len(n)] = np.sum(radiance*weights[..., None], axis=1) / weights.sum(axis=1)[:, None]
    return output.reshape(6, size, size, 3)


def brdf_lut(size, samples=1024):
    ndv = (np.arange(size)+.5)/size
    view = np.stack((np.sqrt(1-ndv*ndv), np.zeros(size), ndv), -1)
    output = np.empty((size, size, 2), dtype=np.float32)
    for row in range(size):
        roughness = (row+.5)/size
        half = ggx_half(roughness, samples)
        vdh = np.maximum(0, np.einsum('ic,jc->ij', view, half))
        light = 2*vdh[..., None]*half[None, :, :] - view[:, None, :]
        ndl = np.maximum(0, light[..., 2])
        ndh = half[None, :, 2]
        k = roughness*roughness/2
        geometry = ((ndv/(ndv*(1-k)+k))[:, None] * ndl/(ndl*(1-k)+k))
        visibility = geometry*vdh / np.maximum(1e-8, ndh*ndv[:, None])
        fresnel = (1-vdh)**5
        output[row, :, 0] = np.mean((1-fresnel)*visibility, axis=1)
        output[row, :, 1] = np.mean(fresnel*visibility, axis=1)
    return output


def sky_cube(size, hour):
    solar = math.sin((hour-6)*math.pi/12)
    day = np.clip(solar*2.3+.42, 0, 1)
    twilight = max(0, 1-abs(solar)*4)
    top = np.array((.015+.10*day+twilight*.10, .025+.32*day-twilight*.07, .09+.60*day))
    horizon = np.array((.055+.55*day+twilight*.34, .075+.68*day-twilight*.18, .15+.69*day-twilight*.27))
    blend = np.clip(.34+directions(size)[..., 1:2]*1.8, 0, 1)
    return np.maximum(0, horizon*(1-blend)+top*blend)


def read_face(path, probe, state, face):
    data = path.read_bytes()
    if data[:8] != b'MCENV1\0\0':
        raise ValueError(f'{path}: invalid capture magic')
    size, p, t, f = struct.unpack_from('<4I', data, 8)
    position = struct.unpack_from('<4f', data, 24)
    hour = struct.unpack_from('<f', data, 40)[0]
    if (size != 128 or (p, t, f) != (probe, state, face) or
            hour != HOURS[state] or len(data) != 44+size*size*8):
        raise ValueError(f'{path}: capture metadata/length mismatch')
    pixels = np.frombuffer(data, dtype='<f2', offset=44).astype(np.float64).reshape(size, size, 4)
    if not np.isfinite(pixels).all() or pixels.min() < 0 or not np.all(pixels[..., 3] == 1):
        raise ValueError(f'{path}: invalid HDR pixels')
    return pixels[..., :3], position, hashlib.sha256(data).hexdigest()


def cook(capture_directory, output):
    start = time.monotonic()
    capture_manifest = json.loads((capture_directory / 'capture.json').read_text(encoding='utf-8'))
    if (capture_manifest.get('schema') != 1 or capture_manifest.get('faces') != 36 or
            capture_manifest.get('size') != 128 or capture_manifest.get('dynamic_lights') is not False or
            capture_manifest.get('probe_feedback') is not False):
        raise ValueError('Incomplete/incompatible capture manifest')
    cubes, positions, source = [], [], []
    for probe in range(2):
        position = None
        for state in range(3):
            faces = []
            for face in range(6):
                path = capture_directory / f'probe-{probe}-time-{state}-face-{face}.hdrface'
                pixels, current, digest = read_face(path, probe, state, face)
                if position is not None and position != current:
                    raise ValueError('Capture probe positions differ')
                position = current
                faces.append(pixels)
                source.append({'file': path.name, 'sha256': digest})
            cubes.append(np.stack(faces))
        positions.append(position)
    cubes.extend(sky_cube(128, hour) for hour in HOURS)
    sh = np.stack([irradiance(cube) for cube in cubes]).astype('<f4')
    lut = brdf_lut(128)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix('.mcpb.tmp')
    with temporary.open('wb') as file:
        file.write(b'MCPB1\0\0\0')
        file.write(struct.pack('<6I', 1, 2, 3, 128, 8, 128))
        file.write(np.asarray(positions, dtype='<f4').tobytes())
        file.write(sh.tobytes())
        for index, cube in enumerate(cubes):
            print(f'Prefilter cube {index+1}/9', flush=True)
            mips = [prefilter(cube, 128 >> mip, mip/7) for mip in range(8)]
            for face in range(6):
                for mip in mips:
                    rgb = mip[face]
                    rgba = np.concatenate((rgb, np.ones((*rgb.shape[:2], 1))), -1)
                    if not np.isfinite(rgba).all() or rgba.max() > 65504:
                        raise ValueError('HDR radiance out of half-float range')
                    file.write(rgba.astype('<f2').tobytes())
        file.write(lut.astype('<f4').tobytes())
    temporary.replace(output)
    report = {'schema': 1, 'asset': 'Original MiniCity3D static district lighting',
              'source_captures': source, 'positions': positions, 'hours': HOURS,
              'capture_settings': capture_manifest,
              'capture_executable_sha256': hashlib.sha256((capture_directory.parent / 'MiniCity3D.exe').read_bytes()).hexdigest(),
              'cooker_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'numpy': np.__version__, 'tool': 'tools/cook_probes.py',
              'capture': 'DX11 linear HDR static-only geometry, 128px faces, no probe feedback, no tone mapping',
              'global_fallback': 'Original analytic game sky gradient; no external HDRI',
              'irradiance': 'SH9; solid-angle weighted; cosine convolution divided by pi',
              'specular': 'GGX importance sampling, 512 Hammersley samples per output texel, roughness=mip/7',
              'brdf': '128x128 RG32F split-sum DFG, 1024 Hammersley samples, Schlick-GGX visibility k=roughness^2/2',
              'format': 'MCPB1; RGBA16F complete cube mips, uncompressed linear HDR',
              'limitations': ['Single capture bounce from legacy ambient/direct lighting',
                              'No interior visibility volumes or box parallax correction',
                              'Time/weather blend is an approximation; moving objects excluded',
                              'Recapture when static geometry/materials or lights change'],
              'output_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
              'seconds': round(time.monotonic()-start, 2)}
    output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Cooked {output}: {output.stat().st_size} bytes in {report["seconds"]} seconds')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--captures', type=Path, default=Path('build-msvc-ninja/probe-captures'))
    parser.add_argument('--output', type=Path, default=Path('assets/lighting/showcase.mcpb'))
    args = parser.parse_args()
    cook(args.captures, args.output)
