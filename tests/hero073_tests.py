"""HERO VR 0.0.73 private test builds (source gates): TEST <n> on the wrist, the CI never uploads a hero-0.0.73
build, the claws are the JACKET AND JEANS suit's own (extracted, credited), the mirror shows the props."""
import os, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

class Hero073(unittest.TestCase):
    def test_test_build_on_with_its_number(self):
        c = src('CMakeLists.txt')
        self.assertRegex(c, r'option\(HERO_TEST_BUILD "[^"]*" ON\)')
        self.assertRegex(c, r'set\(HERO_TEST_NUMBER "[0-9]+"')
    def test_ci_never_uploads_a_hero_0073_build(self):
        w = src('.github/workflows/build-prebuilt.yml')
        i = w.index('- name: Upload to the release')
        self.assertIn("!startsWith(github.ref_name, 'hero-0.0.73')", w[i:i + 200])
        self.assertIn('default: false', w[w.index('upload:'):w.index('upload:') + 200])
    def test_claws_extracted_and_credited(self):
        t = src('tools/claws073/extract_claws.py')
        self.assertIn("'Claws' / name / 'scene.glb'", t)
        self.assertIn('license.txt', t)
        self.assertIn('`Models/Claws/Left/`', src('THIRD-PARTY-MODELS.md'))
        m = src('include/spidy/mesh_model.hpp')
        self.assertIn('"Claws/Left", "Claws/Right"', m)
        self.assertIn('slot < slotSuitEnd', m)
        self.assertIn('Models/Claws/Left/scene.glb', src('tools/release072/required_assets.py'))
    def test_mirror_props(self):
        g = src('src/game_xr.cpp')
        self.assertIn('vr_mirror::unreflect(plane, viewer)', g)
        self.assertIn('vr_mirror::reflectVertices(plane, mirrorProps, 0)', g)
    def test_test2_cull_margin_and_shadow_match(self):
        s = src('src/stereo_probe.cpp')
        self.assertIn('recording_fov::widened(head, static_cast<float>(cullMarginDegrees.load(', s)
        i = s.index('uint64_t csmFit(')
        body = s[i:i + 2500]
        # SHADOW MATCH: the eye's own descriptor is put back after the fit, nothing is shared between the eyes.
        self.assertIn('std::memcpy(view, &lens, sizeof lens);', body)
        self.assertLess(body.index('std::memcpy(view, &lens, sizeof lens);'), body.index('std::memcpy(view, &own, sizeof own);'))
        self.assertIn('std::atomic<uint32_t> shadowMatchOn{};', s)
        self.assertNotIn('eyeShadows = 1', s)
        g = src('src/game_xr.cpp')
        self.assertIn('SpidyCullMarginDegrees(reinterpret_cast<void*>', g)
        self.assertIn('SpidyShadowMatchOn(reinterpret_cast<void*>', g)
        ls = src('tools/live_settings.py')
        self.assertIn("'shadow_match': 'off'", ls)
        self.assertIn("'cull_margin': '10deg'", ls)

    def test_exports_unique_in_the_probe(self):
        # CI caught 'SpidyShadowMatch': redefinition (game_xr.cpp's live setter and stereo_probe.cpp's
        # switch, both in spidy_stereo_probe): every exported name is defined once across the two.
        import re
        names = {}
        for f in ('src/game_xr.cpp', 'src/stereo_probe.cpp'):
            for n in set(re.findall(r'__declspec\(dllexport\)[^(;{]*?\b(Spidy\w+)\s*\(', src(f))):
                names.setdefault(n, []).append(f)
        twice = {n: fs for n, fs in names.items() if len(fs) > 1}
        self.assertEqual(twice, {})

if __name__ == '__main__':
    unittest.main()

class Hero073Test3SmoothTurn(unittest.TestCase):
    """Fix 26: smooth turning is the default everywhere; snap stays one setting away (SMOOTH TURN 0)."""
    def test_script_default_is_smooth(self):
        p = src('tools/launch-game-vr.ps1')
        self.assertIn('[ValidateRange(0,360)][int]$SmoothTurn=120,', p)
        self.assertIn('[switch]$SnapTurning,', p)
        self.assertIn('if($SmoothTurn -eq 0 -and -not $SnapTurning) { $SmoothTurn=120 }', p)
        self.assertLess(p.index('$SmoothTurn=120 }'), p.index("'--smooth-turn',$SmoothTurn"))
    @staticmethod
    def script(smooth, snap):
        # launch-game-vr.ps1's rule, as the session gets it (--smooth-turn).
        if smooth == 0 and not snap: smooth = 120
        return 0 if snap else smooth
    def test_every_launcher_gets_smooth_unless_snap_was_picked(self):
        self.assertEqual(self.script(120, False), 120)   # nothing passed (0.0.73 launcher, default)
        self.assertEqual(self.script(0, False), 120)     # a 0.0.72 launcher's default 0
        self.assertEqual(self.script(0, True), 0)        # SMOOTH TURN off in the 0.0.73 launcher
        self.assertEqual(self.script(90, False), 90)
    def test_launcher_and_session_defaults(self):
        l = src('tools/launcher/src/LaunchOptions.cs')
        self.assertRegex(l, r'Opt\.Num\("COMFORT", "SmoothTurn", "SMOOTH TURN", "[^".]*\.", 0, 360, 10, 120,')
        self.assertIn('args.Add("-SnapTurning");', l)
        self.assertIn('int smoothTurn = 120', src('include/spidy/vr_settings.hpp'))
        self.assertRegex(src('tools/run_game_vr.py'), r"'--smooth-turn'[^\n]*default=120")

class Hero073Test3FlightPose(unittest.TestCase):
    """Fix 27: the lean reaches the body the player sees - the worn suit too - and LEAN is the default."""
    def test_lean_worked_out_for_every_body(self):
        g = src('src/game_xr.cpp')
        i = g.index('Quat flyTiltNow{};')
        self.assertLess(i, g.index('if (bodyStarted) {', i))
        self.assertIn('fly_lean::wanted(c38(combat38::flyPose)', g[i:i + 800])
        self.assertIn('saved.flyLean = flyTiltNow;', g)
        j = g.index('fly_lean::leanSuit(saved->flyLean, t.eye, t.up, t.feet);')
        self.assertLess(j, g.index('suit_wear::estimate(*suitRig, t, live)', j))
    def test_lean_is_the_default(self):
        ls = src('tools/live_settings.py')
        self.assertIn("'fly_pose': ('lean', 'upright')", ls)
        self.assertIn("'fly_pose': 'lean'", ls)

