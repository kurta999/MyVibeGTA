import json
import pathlib
import struct
import sys

for path in map(pathlib.Path, sys.argv[1:]):
    content = path.read_bytes()
    magic, version, total_length = struct.unpack_from("<III", content)
    assert magic == 0x46546C67 and version == 2 and total_length == len(content)
    chunk_length, chunk_type = struct.unpack_from("<II", content, 12)
    assert chunk_type == 0x4E4F534A
    gltf = json.loads(content[20:20 + chunk_length])
    print(path.name)
    for name in ("meshes", "nodes", "skins", "materials", "images", "animations", "accessors"):
        print(f"  {name}: {len(gltf.get(name, []))}")
    print("  scene:", gltf.get("scenes", []))
    print("  first primitive:", gltf.get("meshes", [{}])[0].get("primitives", [{}])[0])
    print("  images:", gltf.get("images", []))
    print("  animation names:", [entry.get("name") for entry in gltf.get("animations", [])])
    print("  mesh nodes:", [(i, node.get("name"), node.get("mesh"), node.get("skin")) for i, node in enumerate(gltf.get("nodes", [])) if "mesh" in node])
    if gltf.get("animations"):
        animation = gltf["animations"][4]
        print("  idle samplers:", len(animation["samplers"]), [entry.get("interpolation", "LINEAR") for entry in animation["samplers"][:5]])
