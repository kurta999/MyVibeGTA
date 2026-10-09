"""Verify the failure mode: sampled leaf fragments and holes in closed bark."""
import pathlib
import sys
import unittest

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from tree_geometry import leaf_components, select_leaves, expand_leaves, decimate_wood, retain_branches


class TreeGeometryTest(unittest.TestCase):
    def test_gltf_leaf_mask_orientation(self):
        import build_city_models as bake
        path = bake.tree_source('tree_detailed')[0]
        doc, binary = bake.read_scene(path)
        primitive = next(primitive for mesh in doc['meshes'] for primitive in mesh['primitives']
            if 'leaves' in doc['materials'][primitive['material']]['name'])
        material = doc['materials'][primitive['material']]
        image = np.asarray(bake.image_from_material(doc, binary, path, material))
        triangles = bake.accessor(doc, binary, primitive['indices']).reshape(-1, 3)[::100]
        uv = bake.accessor(doc, binary, primitive['attributes']['TEXCOORD_0'])[triangles].mean(axis=1)
        def coverage(v):
            x = np.clip((uv[:, 0] * image.shape[1]).astype(int), 0, image.shape[1] - 1)
            y = np.clip((v * image.shape[0]).astype(int), 0, image.shape[0] - 1)
            return np.mean(image[y, x, 3] >= 128)
        self.assertGreater(coverage(uv[:, 1]), .8)
        self.assertLess(coverage(1 - uv[:, 1]), .4)
        # Check the cooked atlas coordinates too, so reverting the baker's V
        # orientation fails this regression even if the source remains correct.
        packed = np.fromfile(bake.BAKED / 'nature/tree_detailed.m3d', '<f4', offset=8).reshape(-1, 3, 12)
        atlas = packed[:, :, 6:8].mean(axis=1)
        leaf_uv = atlas[(atlas[:, 0] >= .5) & (atlas[:, 1] < .5)]
        x = np.clip((((leaf_uv[:, 0] * 2 - 1 - .001) / .998) * image.shape[1]).astype(int), 0, image.shape[1] - 1)
        y = np.clip((((leaf_uv[:, 1] * 2 - .001) / .998) * image.shape[0]).astype(int), 0, image.shape[0] - 1)
        self.assertGreater(np.mean(image[y, x, 3] >= 128), .75)

    def test_twig_budget_retains_whole_largest_pieces(self):
        faces = []
        for index in range(10):
            width = 1 + index
            positions = np.array([[0, 0, 0], [width, 0, 0], [0, width, 0],
                [width, 0, 0], [width, width, 0], [0, width, 0]], float)
            faces.append(np.column_stack((positions, np.zeros((6, 5)))))
        labels = np.repeat(np.arange(10), 2)
        result = retain_branches(np.concatenate(faces), labels, 6).reshape(-1, 3, 8)
        self.assertEqual(len(result), 6)
        widths = np.max(result[:, :, 0], axis=1)
        self.assertEqual(sorted(widths.tolist()), [8, 8, 9, 9, 10, 10])

    def test_whole_multi_quad_leaves_and_nested_lod(self):
        # Each leaf has four connected triangles; two triangles per leaf
        # would leave visible holes. Interleave faces to prohibit pair assumptions.
        leaf = np.array([[0, 1, 2], [1, 3, 2], [2, 3, 4], [3, 5, 4]], np.int32)
        tri = np.concatenate([leaf + n * 6 for n in range(100)])
        tri = tri[np.random.default_rng(7).permutation(len(tri))]
        labels = leaf_components(tri, 600)
        near, near_labels, scale = select_leaves(tri, labels, 100, np.random.default_rng(4))
        self.assertEqual(len(near), 100)
        self.assertTrue(np.all(np.unique(near_labels, return_counts=True)[1] == 4))
        lod, lod_labels, reduced = select_leaves(near, near_labels, 20, np.random.default_rng(8))
        self.assertTrue(set(lod_labels).issubset(set(near_labels)))
        self.assertTrue(np.all(np.unique(lod_labels, return_counts=True)[1] == 4))
        self.assertAlmostEqual(scale ** 2 * len(near), len(tri))
        self.assertAlmostEqual((scale * reduced) ** 2 * len(lod), len(tri))
        points = np.column_stack((np.arange(600), np.zeros(600), np.ones(600))).astype(float)[near.ravel()]
        expanded = expand_leaves(points, near_labels, scale)
        # Shared corners remain identical after expanding an entire connected leaf.
        for index in np.unique(near):
            vertices = expanded[near.ravel() == index]
            self.assertTrue(np.allclose(vertices, vertices[0]))

    def test_decimation_preserves_closed_wood(self):
        # A closed subdivided cube must stay closed after edge collapse.
        positions = []
        triangles = []
        for axis in range(3):
            for sign in [-1, 1]:
                start = len(positions)
                for y in range(9):
                    for x in range(9):
                        p = [0., 0., 0.]
                        p[axis] = sign
                        p[(axis + 1) % 3] = x / 4 - 1
                        p[(axis + 2) % 3] = y / 4 - 1
                        positions.append(p)
                for y in range(8):
                    for x in range(8):
                        a = start + y * 9 + x
                        faces = [[a, a + 1, a + 10], [a, a + 10, a + 9]]
                        triangles.extend(faces if sign > 0 else [face[::-1] for face in faces])
        original = np.asarray(positions, np.float32)
        pos, remap = np.unique(original, axis=0, return_inverse=True)
        tri = remap[np.asarray(triangles)].astype(np.int32)
        normals = pos / np.linalg.norm(pos, axis=1, keepdims=True)
        uv = (pos[:, :2] + 1) / 2
        # Model the glTF's per-corner seam vertices and coincident back faces.
        duplicate_faces = np.concatenate((tri, tri[:, ::-1]))
        duplicate_faces = duplicate_faces[np.random.default_rng(13).permutation(len(duplicate_faces))]
        corners = duplicate_faces.ravel()
        seam_pos, seam_normals, seam_uv = pos[corners], normals[corners], uv[corners]
        seam_tri = np.arange(len(corners), dtype=np.int32).reshape(-1, 3)
        for geometry, budget in zip(decimate_wood(seam_pos, seam_normals, seam_uv, seam_tri, 100, 30, ROOT), [100, 30]):
            self.assertLess(len(geometry) // 3, len(tri))
            self.assertLessEqual(len(geometry) // 3, budget + 2)
            _, ids = np.unique(np.round(geometry[:, :3], 5), axis=0, return_inverse=True)
            faces = ids.reshape(-1, 3)
            edges = np.sort(np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1)
            self.assertTrue(np.all(np.unique(edges, axis=0, return_counts=True)[1] == 2))
            self.assertTrue(np.isfinite(geometry).all())


if __name__ == '__main__':
    unittest.main()
