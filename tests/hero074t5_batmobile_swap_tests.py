"""0.0.74 test 5 BATMOBILE SWAP (owner: "switching models doesn't change the car"): a parked Batmobile becomes
the picked car where it stands; the picker lines say so. Made by Agent Mulder."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def src(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class Swap(unittest.TestCase):
    def setUp(self):
        self.g = src('src/game_xr.cpp')

    def test_parked_car_swaps(self):
        i = self.g.index('0.0.74 test 5 BATMOBILE SWAP')
        block = self.g[i:i + 2000]
        self.assertIn('van.state == van072::parked', block)
        self.assertIn('pickNow != batCar && batReady(pickNow)', block)
        self.assertIn('batCar = pickNow;', block)
        # ahead of the kind/tune of this frame, so the new car's layout and tune are used the same step
        self.assertLess(i, self.g.index('vehicleTune = kindNow == kindBatmobile ? batmobile074::tune(batCar)'))

    def test_picker_lines(self):
        self.assertNotIn('comes on the next summon', self.g)
        self.assertIn('" - it swaps in when you get out"', self.g)
        self.assertIn('" - swapped in where it is parked"', self.g)


if __name__ == '__main__':
    unittest.main()
