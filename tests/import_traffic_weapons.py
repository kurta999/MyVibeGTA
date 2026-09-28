"""Focused regression checks for the indexed glTF weapon cooker."""

import pathlib
import hashlib
import json
import math
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import import_traffic_weapons as importer


class IndexedImportTest(unittest.TestCase):
    def test_multiple_gltf_buffers_fail_instead_of_discarding_geometry(self):
        with tempfile.TemporaryDirectory() as folder:
            source = pathlib.Path(folder) / "multi.gltf"
            source.write_text(json.dumps({"buffers": [
                {"uri": "one.bin"}, {"uri": "two.bin"}]}))
            with self.assertRaisesRegex(ValueError, "exactly one glTF buffer"):
                importer.read_scene(source)

    def test_report_discloses_discarded_skin_and_animation(self):
        with tempfile.TemporaryDirectory() as folder:
            source = pathlib.Path(folder) / "animated.glb"
            source.write_bytes(b"source fixture")
            document = {"skins": [{}], "animations": [{"name": "Pump"}],
                        "samplers": [{"wrapS": 10497}],
                        "materials": [{"doubleSided": True}]}
            report = importer.conversion_report(document, source,
                {"POSITION", "NORMAL", "JOINTS_0", "WEIGHTS_0"})
            self.assertEqual(report["source_sha256"],
                hashlib.sha256(b"source fixture").hexdigest())
            self.assertEqual(report["discarded_attributes"],
                ["JOINTS_0", "WEIGHTS_0"])
            self.assertEqual(report["discarded_skins"], 1)
            self.assertEqual(report["discarded_animations"], ["Pump"])
            self.assertEqual(report["double_sided_materials"], [0])

    def test_inverse_transpose_normal_under_nonuniform_scale(self):
        matrix = [2, 0, 0, 0, 0, 1, 0, 0,
                  0, 0, 0.5, 0, 0, 0, 0, 1]
        normal = importer.transformed_normal(matrix, (1, 0, 1))
        self.assertAlmostEqual(normal[0], 1 / math.sqrt(17), places=6)
        self.assertAlmostEqual(normal[2], 4 / math.sqrt(17), places=6)

    def test_source_indices_and_primitive_boundaries(self):
        triangle = [
            (0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1),
            (1, 0, 0, 0, 1, 0, 1, 0, 1, 1, 1, 1),
            (0, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1),
        ]
        # Two material primitives have byte-identical vertices. Their source
        # indices must remain distinct; a repeated face in one primitive must
        # reuse that primitive's three vertices.
        vertices = triangle * 3
        keys = [(0, 0, i) for i in range(3)] + [
            (0, 1, i) for i in range(3)] + [(0, 0, i) for i in range(3)]
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / "fixture.m3d"
            importer.write_mesh(path, vertices, keys)
            data = path.read_bytes()
        magic, vertex_count, index_count = struct.unpack_from("<4sII", data)
        self.assertEqual((magic, vertex_count, index_count), (b"M3D2", 6, 9))
        indices = struct.unpack_from("<9I", data, 12 + 6 * 48)
        self.assertEqual(indices, (0, 1, 2, 3, 4, 5, 0, 1, 2))

    def test_external_image_uses_source_directory(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            source = root / "source" / "model.gltf"
            source.parent.mkdir()
            (source.parent / "base.png").write_bytes(b"image fixture")
            output = root / "cooked" / "mesh.m3d"
            output.parent.mkdir()
            name = importer.embedded_image(
                {"images": [{"uri": "base.png"}]}, b"", 0, source, output)
            self.assertEqual((output.parent / name).read_bytes(), b"image fixture")


if __name__ == "__main__":
    unittest.main()
