"""Build 8 (test 8 of 0.0.72) ROSTER PAGE (the user, 11:27 PM: "wheres the roster page with the profile
pictures and suit options?"): the page's picture list, the Roster GLB tool, the suit catalog and the zip's
required files agree; the wrist row and the game wiring are in place. The page's behaviour is in
core_tests.cpp ('Build 8: ROSTER page ...')."""
import importlib.util
import json
import os
import re
import struct
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))


def read(*p):
    with open(os.path.join(ROOT, *p), encoding='utf-8') as f:
        return f.read()


def load(name, *p):
    spec = importlib.util.spec_from_file_location(name, os.path.join(ROOT, *p))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def cpp_folders():
    s = read('include', 'spidy', 'roster_page.hpp')
    body = s[s.index('pictureFolders[pictureCount] = {'):]
    body = body[:body.index('};')]
    return re.findall(r'"([^"]+)"', body)


class RosterPage(unittest.TestCase):
    def test_picture_lists_agree(self):
        glb = load('make_roster_glb', 'tools', 'roster', 'make_roster_glb.py')
        self.assertEqual(cpp_folders(), glb.PICTURE_FOLDERS)
        self.assertEqual(len(glb.PICTURE_FOLDERS), 69)

    def test_coming_soon_portraits(self):
        # 0.0.73 COMING SOON: the 21 SOON tiles' pictures are in the repo, in holo_roster's order, credited,
        # none from Marvel's Avengers (2020); a missing one is a silhouette, never an error.
        glb = load('make_roster_glb', 'tools', 'roster', 'make_roster_glb.py')
        sp = load('soon_portraits', 'tools', 'roster', 'soon_portraits.py')
        self.assertEqual(glb.SOON_KEYS, sp.KEYS)
        self.assertEqual(glb.PICTURE_FOLDERS[48:], ['soon_' + k for k in glb.SOON_KEYS])
        h = read('include', 'spidy', 'holo_roster.hpp')
        body = h[h.index('soonHeroes[soonCount] = {'):]
        names = re.findall(r'\{"([^"]+)", [01]\}', body[:body.index('};')])
        self.assertEqual(len(names), 21)
        key = {'THE FLASH': 'flash'}
        self.assertEqual([key.get(n, re.sub(r'[^a-z]', '', n.lower())) for n in names], glb.SOON_KEYS)
        meta = json.load(open(os.path.join(glb.SOON_DIR, 'soon.json')))
        for k in glb.SOON_KEYS:
            self.assertTrue(os.path.exists(os.path.join(glb.SOON_DIR, k + '.jpg')), k)
            self.assertTrue(meta[k]['user'] and meta[k]['url'].startswith('https://sketchfab.com/'), k)
            self.assertNotIn('avengers', (meta[k]['name'] + meta[k]['url']).lower(), k)
        try:
            import PIL  # noqa: F401
        except ImportError:
            return
        ims, credits = glb.soon_pictures(os.path.join(ROOT, 'no-such-folder'))
        self.assertEqual(len(ims), 21)
        self.assertTrue(all('silhouette' in c[1] for c in credits))

    def test_every_catalog_suit_has_a_picture(self):
        s = read('include', 'spidy', 'hero072.hpp')
        suits = re.findall(r'"(Suit_[A-Za-z]+_\d|CapSuit|PunisherWarzone)"', s)
        self.assertTrue(suits)
        folders = cpp_folders()
        for f in suits:
            self.assertIn(f, folders)

    def test_roster_glb_in_the_zip(self):
        ra = load('required_assets', 'tools', 'release072', 'required_assets.py')
        self.assertIn('Models/Roster/scene.glb', ra.MODEL_FILES)
        self.assertIn('Models/Roster/license.txt', ra.LICENSE_FILES)

    def test_roster_model_not_decimated(self):
        s = read('include', 'spidy', 'mesh_model.hpp')
        self.assertIn('"Roster"', s)

    def test_wrist_row(self):
        h = read('include', 'spidy', 'wrist_hud.hpp')
        c = read('src', 'wrist_hud.cpp')
        self.assertIn('heroRoster,', h)
        self.assertIn('static_assert(static_cast<int>(Control::heroRoster) == controls - 1);', h)
        self.assertIn('return "ROSTER: HEROES AND SUITS";', c)
        self.assertIn('row(Control::heroRoster);', c)
        self.assertIn('No restart needed.', c[c.index('case Control::heroRoster:\n        return "Opens'):][:200])

    def test_game_wiring(self):
        s = read('src', 'game_xr.cpp')
        self.assertIn('rosterPage.open(frame.head, rosterClock)', s)
        self.assertIn('roster_page::append(translucent, overlay.modelReady() ? &modelVertices : nullptr', s)
        self.assertIn('roster_page::publish(roster_page::snapshotOf(hero072Play.suits, all72, in72.o.ownSuit));', s)
        self.assertIn('hero072Play.suits.choose(h, i, all72)', s)
        self.assertIn('frame.buttons &= ~static_cast<uint32_t>(buttonA | buttonB | buttonX | buttonY);', s)

    def test_built_glb_if_present(self):
        p = os.path.join(ROOT, 'build', 'h7out', 'Models', 'Roster', 'scene.glb')
        if not os.path.exists(p):
            self.skipTest('Roster not built here')
        b = open(p, 'rb').read()
        self.assertEqual(b[:4], b'glTF')
        n = struct.unpack_from('<I', b, 12)[0]
        j = json.loads(b[20:20 + n])
        self.assertEqual(len(j['images']), 8)
        tris = sum(j['accessors'][pr['indices']]['count'] // 3 if 'indices' in pr else
                   j['accessors'][pr['attributes']['POSITION']]['count'] // 3
                   for m in j['meshes'] for pr in m['primitives'])
        self.assertEqual(tris, 138)


if __name__ == '__main__':
    unittest.main()
