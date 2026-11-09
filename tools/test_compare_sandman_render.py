"""Offline regression tests for the passive Sandman render-command comparator."""
import json
import tempfile
import unittest
from pathlib import Path

from compare_sandman_render import load, compare


class RenderCompareTests(unittest.TestCase):
    def test_normalized_rates_and_eye_separation(self):
        clear = {"frames": 60, "commands": {
            "0x20 VisualEffects": [1.0, 2.0, 3.0, 3.0],
            "0x30 Other": [4.0, 0.0, 0.0, 0.0]}}
        cloud = {"frames": 120, "commands": {
            "0x20 VisualEffects": [1.0, 2.0, 6.0, 6.0],
            "0x40 Added": [0.0, 0.0, 1.0, 1.0]}}
        changes = compare(clear, cloud, threshold=0.5)
        self.assertEqual(changes[0]["command"], "0x30 Other")
        self.assertEqual(changes[0]["delta_per_frame"], [-4.0, 0.0, 0.0, 0.0])
        effects = next(x for x in changes if x["command"] == "0x20 VisualEffects")
        self.assertEqual(effects["delta_per_frame"], [0.0, 0.0, 3.0, 3.0])

    def test_small_changes_filtered(self):
        clear = {"frames": 3, "commands": {"fx": [0.0, 0.0, 0.0, 0.0]}}
        cloud = {"frames": 3, "commands": {"fx": [0.0, 0.0, 0.49, 0.49]}}
        self.assertEqual(compare(clear, cloud, 0.5), [])

    def test_invalid_input_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            f = Path(folder) / "capture.json"
            for bad in (
                {"frames": 0, "commands": {}},
                {"frames": 1, "commands": {"effect": [1, 2]}},
                {"frames": 1, "commands": {"effect": [1, 2, float("nan"), 4]}},
            ):
                f.write_text(json.dumps(bad), encoding="utf-8")
                with self.assertRaises(ValueError):
                    load(f)


if __name__ == "__main__":
    unittest.main()
