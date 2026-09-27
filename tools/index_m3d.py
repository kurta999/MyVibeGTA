"""Convert a legacy triangle-list M3D1 mesh to indexed M3D2 in place.

The element order stays unchanged, so adjacent .pbr material ranges and
.glass triangle lists still address the same triangles. The original file
remains recoverable from source control.
"""

import argparse
import pathlib
import struct


VERTEX_SIZE = 48


def convert(path: pathlib.Path) -> tuple[int, int]:
    source = path.read_bytes()
    if len(source) < 8 or source[:4] != b"M3D1":
        raise ValueError(f"Expected M3D1: {path}")
    count = struct.unpack_from("<I", source, 4)[0]
    if not count or count % 3 or count > 3_000_000 or len(source) != 8 + count * VERTEX_SIZE:
        raise ValueError(f"Invalid legacy mesh: {path}")
    unique, indices, lookup = [], [], {}
    for offset in range(8, len(source), VERTEX_SIZE):
        vertex = source[offset:offset + VERTEX_SIZE]
        index = lookup.get(vertex)
        if index is None:
            index = len(unique)
            lookup[vertex] = index
            unique.append(vertex)
        indices.append(index)
    output = bytearray(struct.pack("<4sII", b"M3D2", len(unique), len(indices)))
    output.extend(b"".join(unique))
    output.extend(struct.pack(f"<{len(indices)}I", *indices))
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(output)
    temporary.replace(path)
    return count, len(unique)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("meshes", type=pathlib.Path, nargs="+")
    args = parser.parse_args()
    for mesh in args.meshes:
        elements, vertices = convert(mesh)
        print(f"{mesh}: {elements // 3} triangles, {vertices}/{elements} unique vertices")
