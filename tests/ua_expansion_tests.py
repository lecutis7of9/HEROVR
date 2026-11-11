"""ULTIMATE ALLIANCE EXPANSION (M1): Marvel: Ultimate Alliance's Stark Tower hub walked in VR on foot from the
wrist's own ULTIMATE ALLIANCE EXPANSION page (ua_expansion.hpp), built by the player from his OWN MUA1 PS2 disc
(tools/ua/convert_ua.py - bring your own game: nothing of the game in git). live.txt ua_locations, ON by default
(new features ship on, switchable off), needs a restart to turn on. The hideouts' pattern (hideout074).
Made by Agent Mulder."""
import os, re, sys, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import live_settings  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()


class UaLocationsSetting(unittest.TestCase):
    def test_live_txt_option(self):
        (lo, hi), export, text = live_settings.SETTINGS['ua_locations']
        self.assertEqual((lo, hi, export), (0, 1, 'SpidyUaLocations'))
        self.assertIn('restart', text)
        self.assertEqual(live_settings.CHOICE_NAMES['ua_locations'], ('on', 'off'))
        self.assertEqual(live_settings.UPDATE38_DEFAULTS['ua_locations'], 'on')  # new features ship on
        self.assertEqual(live_settings.export_value('ua_locations', 'on'), 0)
        self.assertEqual(live_settings.export_value('ua_locations', 'off'), 1)
        self.assertEqual(live_settings.parse('ua_locations = off\n'), ({'ua_locations': 'off'}, []))
        self.assertIn('ua_locations', live_settings.UPDATE38_KEYS)
        self.assertIn('ua_locations', live_settings.START_ONLY)

    def test_export_5541_is_ours_alone(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyUaLocations(void* v) { return setHero007(v, combat38::uaLocations, 5541); }', g)
        self.assertEqual(len(re.findall(r'\b5541\b', g)), 1)
        c = src('include/spidy/combat38.hpp')
        self.assertIn('{"ua_locations", "UA LOCATIONS", "UA",', c)
        self.assertIn('inline bool uaLocationsOn(uint32_t value) { return value != 1; }', c)


class Wiring(unittest.TestCase):
    def setUp(self):
        self.g = src('src/game_xr.cpp')

    def test_loaded_only_when_on_and_converted(self):
        self.assertIn('if (mesh_model::slotIsUa(slot)) {', self.g)
        self.assertIn('modelsPath(mesh_model::slotFolders[slot], L"scene.glb")', self.g)
        self.assertIn('readUaPlace(at);', self.g)
        self.assertIn('"UA/StarkTower", "UA/StarkTower/Cast"};', src('include/spidy/mesh_model.hpp'))

    def test_the_wrist_page_starts_and_ends_it(self):
        self.assertIn('if (result.uaStarkTower) // ULTIMATE ALLIANCE EXPANSION', self.g)
        self.assertIn('uaVisit.begin(ua_expansion::which[ua_expansion::starkTower]', self.g)
        self.assertIn('const char* why = ua_expansion::refuse(gate);', self.g)
        self.assertIn('uaFloorY = head.y - ua_expansion::eyeHeight;', self.g)
        w = src('src/wrist_hud.cpp')
        self.assertIn('case uaPage: {', w)
        self.assertIn('return ua_expansion::pageTitle;', w)
        self.assertIn('constexpr int uaPage = 8;', src('include/spidy/wrist_hud.hpp'))

    def test_the_hero_stays_where_he_was_left(self):
        # the game gets a pad at rest, no hero tool answers a hand, the van is not drawn in the room
        self.assertIn('if (uaVisit.active()) {\n                            const uint16_t keep', self.g)
        self.assertIn('const bool seatedLock = carLock.tools || uaVisit.active(), grappleLock = carLock.grapple || uaVisit.active();', self.g)
        self.assertIn('} else if (!uaVisit.active() && body && van.state != van072::none', self.g)
        self.assertIn('spidy::cave_quiet::quiet(caveQuietOption, hideoutVisit.active() || uaVisit.active())', self.g)

    def test_drawn_as_a_hideout_room(self):
        self.assertIn('saved.hideoutModel = mesh_model::library().model(mesh_model::slotUaStarkTower);', self.g)
        self.assertIn('HV.ua = true;', self.g)


class BringYourOwnGame(unittest.TestCase):
    def test_converter_and_docs(self):
        c = src('tools/ua/convert_ua.py')
        for want in ("'--iso'", "'--wad'", "'--blender'", "'--igb-blender'", "'--out'", 'make_hideout.build(',
                     "'mesh'", 'strip_lights', 'npcs.txt'):
            self.assertIn(want, c)
        d = src('docs/UA-EXPANSION.md')
        for want in ('convert_ua.py', 'Models/UA/StarkTower', 'ua_locations', 'ULTIMATE ALLIANCE EXPANSION', 'own'):
            self.assertIn(want, d)

    def test_never_committed(self):
        gi = src('.gitignore')
        self.assertIn('/Models/UA/', gi)
        self.assertIn('*.iso', gi)
        import subprocess
        try:
            tracked = subprocess.run(['git', 'ls-files'], cwd=ROOT, capture_output=True, text=True, timeout=60).stdout.split('\n')
        except (OSError, subprocess.SubprocessError):
            self.skipTest('no git')
        bad = [p for p in tracked if p.lower().endswith(('.igb', '.fb', '.wad', '.iso', '.engb')) or p.startswith('Models/UA/')]
        self.assertEqual(bad, [])


if __name__ == '__main__':
    unittest.main()
