# Version 0.0.72: REAL MODELS gate. Every shipped model rendered by the game's own reader beside its
# source (model-check/*.png); fails on too few triangles, missing UVs / textures or a changed shape.
# Runs where the converted models, the sources and the dump tool are (skips in CI).
import os
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'models072'))
sys.path.insert(0, str(ROOT / 'tools' / 'models007'))

try:
    import model_check as mc
except Exception:  # numpy / PIL missing
    mc = None

SKIP = (os.environ.get('CI') or os.environ.get('GITHUB_ACTIONS') or mc is None or not os.path.isdir(mc.DEFAULT_MODELS)
        or not os.path.isdir(mc.DEFAULT_SOURCES) or not os.path.isfile(mc.DUMP))


@unittest.skipIf(SKIP, 'needs build/h7out/Models, the sources and build/gate/spidy_model_dump (local)')
class RealModels(unittest.TestCase):
    def test_every_model_passes(self):
        with tempfile.TemporaryDirectory(prefix='sm2072_mct_') as tmp:
            failures, rows = mc.run(out=os.environ.get('HERO072_MODEL_CHECK_OUT') or tmp)
        self.assertFalse(failures, '\n'.join(failures))
        slots = {r['slot'] for r in rows}
        for s in mc.SOURCES:
            self.assertIn(s, slots)
        self.assertIn('SawedOff/Assembled', slots)


class Floors(unittest.TestCase):
    @unittest.skipIf(mc is None, 'needs numpy')
    def test_floors(self):
        self.assertGreaterEqual(mc.FLOORS['weapon'], 3000)
        self.assertGreaterEqual(mc.FLOORS['vehicle'], 20000)


if __name__ == '__main__':
    unittest.main()
