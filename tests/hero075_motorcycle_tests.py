"""HERO VR 0.0.75 MOTORCYCLE: CAPTAIN AMERICA's and WOLVERINE's summonable bike (Mert Tetik's "Low-Poly Sci-Fi
Motorcycle", CC BY 4.0) on the van's system - the X tap, the MODEL MISSING fallback, handlebar steering, the bike's
own slots, its draw and its credit. Driver only. Made by Agent Mulder."""
import os, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

class Hero075Motorcycle(unittest.TestCase):
    def test_x_tap_summons_or_says_model_missing(self):
        g = src('src/game_xr.cpp')
        self.assertIn('enum : uint8_t { kindNone, kindVan, kindBatmobile, kindBike };', g)
        self.assertIn('return h == hero_modes::captainAmerica || h == hero_modes::wolverine;', g)
        j = g.index('} else if (bikeHero(heroNow()) && mapping == game_pad::Mapping::gameplay) {')
        b = g[j:j + 2200]
        self.assertIn('buttons.buttons &= ~static_cast<uint32_t>(buttonX);', b)
        self.assertIn('bikeTap.step(bikeX, frame.seconds)', b)
        self.assertIn('"MOTORCYCLE: MODEL MISSING: Vehicle/Motorcycle, no bike"', b)
        self.assertIn('vanWanted = true;', b)
        self.assertLess(g.index('c38(combat38::punisherVan) == 0 && heroNow() == hero_modes::punisher'), j)
    def test_the_bike_is_chosen_tuned_and_seated(self):
        g = src('src/game_xr.cpp')
        self.assertIn('else if (bikeHero(heroWas) && bikeReady()) // 0.0.75 MOTORCYCLE (ON, no switch)', g)
        self.assertIn(': kindNow == kindBike    ? bike075::tune()', g)
        self.assertIn('vehicleKind == kindBatmobile || vehicleKind == kindBike); // 0.0.75', g)
        self.assertIn('seatAt.y = van.frame.world(batNow || bikeNow ? vanTune.eye : vehicle074::seatEye).y;', g)
        self.assertIn('why = "the motorcycle has no hideout trip";', g)
        self.assertIn('!bikeNow; // 0.0.75: the bike has no hideout trip', g)
    def test_the_bike_is_drawn_with_its_bars(self):
        g = src('src/game_xr.cpp')
        self.assertIn('const bool bikeDraw = vehicleKind == kindBike && bikeHero(hero007In.hero);', g)
        self.assertIn('slotModel(mesh_model::slotBikeBody, bike075::name)', g)
        self.assertIn('slotModel(mesh_model::slotBikeBars, bike075::name)', g)
        self.assertIn('cabin_look::optionOn(c38(combat38::dashInside)) && !bikeDraw;', g)
        self.assertIn('V.carCredit = bike075::creditLine;', g)
    def test_handlebar_steering_and_slots(self):
        v = src('include/spidy/van072.hpp')
        self.assertIn('bool handlebar = false;', v)
        self.assertIn('a = std::atan2(d.z, d.x);', v)
        m = src('include/spidy/mesh_model.hpp')
        self.assertIn('"Vehicle/Motorcycle"', m)
        self.assertIn('"Vehicle/Motorcycle/Handlebars"', m)
        self.assertIn('slotIsBike(slot)', m)
        b = src('src/bike075.cpp')
        self.assertIn('t.handlebar = true;', b)
    def test_credit_everywhere(self):
        for p in ('THIRD-PARTY-MODELS.md', 'CREDITS.md', 'RELEASE-NOTES-v0.0.75.md', 'include/spidy/bike075.hpp'):
            t = src(p)
            self.assertIn('Mert Tetik', t, p)
            self.assertIn('CC BY 4.0', t, p)
        self.assertIn('50c38758e5134a0b8ffa49780e7fe228', src('THIRD-PARTY-MODELS.md'))
        self.assertIn('driver only', src('RELEASE-NOTES-v0.0.75.md'))
    def test_no_addresses_and_no_switch(self):
        for p in ('include/spidy/bike075.hpp', 'src/bike075.cpp'):
            self.assertNotIn('0x', src(p), p)
        self.assertNotIn('motorcycle', src('include/spidy/combat38.hpp').lower())

if __name__ == '__main__':
    unittest.main()
