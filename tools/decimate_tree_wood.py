"""Blender worker: collapse edges instead of deleting random bark faces."""
import pathlib
import sys

import bpy
import bmesh
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from tree_geometry import leaf_components, retain_branches

source, destination = [pathlib.Path(value) for value in sys.argv[sys.argv.index('--') + 1:]]
data = np.load(source)
mesh = bpy.data.meshes.new('connected wood')
# glTF duplicates vertices along UV/normal seams. Weld their positions, while
# keeping the original UV per face corner, so collapse sees connected tubes.
positions, remap = np.unique(data['pos'], axis=0, return_inverse=True)
triangles = remap[data['tri']]
# Some source trunks contain coincident front/back faces. They become
# non-manifold after welding and would prevent collapse from meeting its budget.
_, retained = np.unique(np.sort(triangles, axis=1), axis=0, return_index=True)
retained.sort()
mesh.from_pydata(positions.tolist(), [], triangles[retained].tolist())
mesh.update()
uv = mesh.uv_layers.new(name='source UV')
uv.data.foreach_set('uv', data['uv'][data['tri'][retained].ravel()].astype(np.float32).ravel())
mesh.polygons.foreach_set('use_smooth', np.ones(len(mesh.polygons), dtype=bool))
# Removing coincident back faces can leave inconsistent winding. Collapse
# needs coherent face orientation along each connected bark surface.
topology = bmesh.new()
topology.from_mesh(mesh)
bmesh.ops.recalc_face_normals(topology, faces=list(topology.faces))
topology.to_mesh(mesh)
topology.free()
mesh.update()
obj = bpy.data.objects.new('connected wood', mesh)
bpy.context.collection.objects.link(obj)
bpy.context.view_layer.objects.active = obj
obj.select_set(True)
outputs = {}
for name, target in [('near', int(data['near'])), ('lod', int(data['lod']))]:
    modifier = obj.modifiers.new(name, 'DECIMATE')
    modifier.ratio = min(1.0, target / len(mesh.polygons))
    modifier.use_collapse_triangulate = True
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    result = evaluated.to_mesh()
    result.calc_loop_triangles()
    points = np.empty(len(result.vertices) * 3, dtype=np.float32)
    result.vertices.foreach_get('co', points)
    normals = np.empty(len(result.vertices) * 3, dtype=np.float32)
    result.vertices.foreach_get('normal', normals)
    loop_vertices = np.empty(len(result.loops), dtype=np.int32)
    result.loops.foreach_get('vertex_index', loop_vertices)
    texcoords = np.empty(len(result.loops) * 2, dtype=np.float32)
    result.uv_layers.active.data.foreach_get('uv', texcoords)
    triangle_loops = np.empty(len(result.loop_triangles) * 3, dtype=np.int32)
    result.loop_triangles.foreach_get('loops', triangle_loops)
    ids = loop_vertices[triangle_loops]
    geometry = np.concatenate((points.reshape(-1, 3)[ids],
        normals.reshape(-1, 3)[ids], texcoords.reshape(-1, 2)[triangle_loops]), axis=1)
    if len(geometry) // 3 > target * 1.2:
        geometry = retain_branches(geometry, leaf_components(ids.reshape(-1, 3), len(result.vertices)), target)
    outputs[name] = geometry
    evaluated.to_mesh_clear()
    obj.modifiers.remove(modifier)
np.savez_compressed(destination, **outputs)
