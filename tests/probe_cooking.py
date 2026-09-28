import sys
import unittest
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import cook_probes as probes


class ProbeMath(unittest.TestCase):
    def test_cube_orientation_roundtrip(self):
        directions = probes.directions(32)
        axis_color = (directions+1)/2
        np.testing.assert_allclose(probes.sample_cube(axis_color, directions), axis_color, atol=1e-12)
        center = probes.directions(1).reshape(6, 3)
        np.testing.assert_equal(center, ((1, 0, 0), (-1, 0, 0), (0, 1, 0),
                                        (0, -1, 0), (0, 0, 1), (0, 0, -1)))

    def test_constant_diffuse_energy(self):
        color = np.array((.25, 2.0, 4.0))
        cube = np.broadcast_to(color, (6, 64, 64, 3))
        coefficients = probes.irradiance(cube)
        radiance = np.einsum('fhwi,ic->fhwc', probes.sh_basis(probes.directions(8)), coefficients[:, :3])
        np.testing.assert_allclose(radiance, np.broadcast_to(color, radiance.shape), atol=1e-6)

    def test_directional_diffuse_convolution(self):
        direction = probes.directions(64)
        cube = np.broadcast_to(1+.5*direction[..., 1:2], (6, 64, 64, 3))
        coefficients = probes.irradiance(cube)
        target = probes.directions(8)
        radiance = np.einsum('fhwi,ic->fhwc', probes.sh_basis(target), coefficients[:, :3])
        expected = np.broadcast_to(1+target[..., 1:2]/3, radiance.shape)
        np.testing.assert_allclose(radiance, expected, atol=1e-5)

    def test_prefilter_preserves_constant_hdr(self):
        cube = np.broadcast_to((.1, 2.0, 7.0), (6, 16, 16, 3))
        for roughness in (0, .35, 1):
            radiance = probes.prefilter(cube, 4, roughness, 128)
            np.testing.assert_allclose(radiance, np.broadcast_to((.1, 2., 7.), radiance.shape), atol=1e-12)

    def test_white_furnace_brdf_bound(self):
        lut = probes.brdf_lut(32)
        self.assertTrue(np.isfinite(lut).all())
        self.assertGreaterEqual(lut.min(), 0)
        self.assertLessEqual(np.max(lut.sum(axis=-1)), 1.02)
        self.assertGreater(lut[0, -1, 0], .99)
        self.assertLess(lut[0, -1, 1], .001)


if __name__ == '__main__':
    unittest.main()
