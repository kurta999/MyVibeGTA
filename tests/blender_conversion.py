"""Integration checks for canonical conversion; run after bootstrap_blender.ps1."""
import json
import pathlib
import os
import struct
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
BLENDER = ROOT / "build-tools/blender-4.5.14-windows-x64/blender.exe"
FIXTURES = ROOT / "build-tools/conversion-fixtures"


def run_blender(script, *arguments):
    environment = os.environ.copy()
    environment["BLENDER_USER_RESOURCES"] = str(ROOT / "build-tools/blender-user")
    return subprocess.run([str(BLENDER), "--background", "--factory-startup",
                           "--python-exit-code", "1", "--python", str(script),
                           "--", *map(str, arguments)], capture_output=True, text=True, env=environment)


def read_glb(path):
    data = path.read_bytes()
    magic, version, total, length, kind = struct.unpack_from("<5I", data)
    assert magic == 0x46546C67 and version == 2 and total == len(data) and kind == 0x4E4F534A
    return json.loads(data[20:20 + length])


class ConversionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        result = run_blender(ROOT / "tests/blender_conversion_fixture.py", FIXTURES)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def convert(self, suffix, name="scene"):
        output = FIXTURES / f"canonical-{name}-{suffix}.glb"
        result = run_blender(ROOT / "tools/blender_convert.py", "--input", FIXTURES / f"{name}.{suffix}",
                             "--output", output, "--source-url", "original-test-fixture",
                             "--license", "CC0-1.0", "--credit", "MiniCity project")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        report = json.loads(output.with_suffix(".conversion.json").read_text(encoding="utf-8"))
        self.assertEqual(report["unsupported"], [])
        self.assertTrue(report["output"]["all_primitives_indexed"])
        self.assertEqual(report["blender_version"], "4.5.14 LTS")
        self.assertEqual(len(report["source_sha256"]), 64)
        return read_glb(output)

    def check_geometry_and_rig(self, document):
        self.assertEqual(len(document["skins"]), 1)
        self.assertEqual(len(document["skins"][0]["joints"]), 2)
        self.assertGreater(len(document["animations"]), 0)
        paths = {channel["target"]["path"] for animation in document["animations"]
                 for channel in animation["channels"]}
        self.assertIn("rotation", paths)
        primitives = [primitive for mesh in document["meshes"] for primitive in mesh["primitives"]]
        for primitive in primitives:
            attributes = primitive["attributes"]
            for attribute in ("POSITION", "NORMAL", "TANGENT", "TEXCOORD_0", "TEXCOORD_1", "COLOR_0", "JOINTS_0", "WEIGHTS_0"):
                self.assertIn(attribute, attributes)
            self.assertIn("indices", primitive)
        self.assertTrue(any(node.get("name") == "FixtureRoot" and node.get("children")
                            for node in document["nodes"]))

    def test_fbx_preserves_rig_hierarchy_uv_color_and_animation(self):
        self.check_geometry_and_rig(self.convert("fbx"))

    def test_authored_blend_exports_clearcoat(self):
        document = self.convert("blend")
        self.check_geometry_and_rig(document)
        self.assertIn("KHR_materials_clearcoat", document["extensionsUsed"])

    def test_unsupported_shader_fails_with_report(self):
        output = FIXTURES / "rejected.glb"
        result = run_blender(ROOT / "tools/blender_convert.py", "--input", FIXTURES / "unsupported.blend",
                             "--output", output, "--source-url", "original-test-fixture",
                             "--license", "CC0-1.0", "--credit", "MiniCity project")
        self.assertNotEqual(result.returncode, 0)
        report = json.loads(output.with_suffix(".conversion.json").read_text(encoding="utf-8"))
        self.assertTrue(any("TEX_NOISE" in issue for issue in report["unsupported"]))
        self.assertFalse(output.exists())

    def test_centimetres_normalize_once_through_parent_scale(self):
        document = self.convert("blend", "centimetres")
        roots = [node for node in document["nodes"] if node.get("name") == "CanonicalMetreScale"]
        self.assertEqual(len(roots), 1)
        for scale in roots[0]["scale"]:
            self.assertAlmostEqual(scale, 0.01)
        self.check_geometry_and_rig(document)


if __name__ == "__main__":
    unittest.main()
