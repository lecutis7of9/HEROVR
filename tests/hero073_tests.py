"""HERO VR 0.0.73 private test builds (source gates): TEST <n> on the wrist, the CI never uploads a hero-0.0.73
build, the claws are the JACKET AND JEANS suit's own (extracted, credited), the mirror shows the props."""
import os, pathlib, re, unittest
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()

class Hero073(unittest.TestCase):
    def test_release_build_off_test_number_kept(self):  # the public 0.0.74 release: HERO_TEST_BUILD OFF (test builds: ON), the test number kept
        c = src('CMakeLists.txt')
        self.assertRegex(c, r'option\(HERO_TEST_BUILD "[^"]*" OFF\)')
        self.assertIn('set(HERO_TEST_NUMBER "10" CACHE', c)  # 0.0.74 test 10: the notice says TEST 10
        self.assertIn('set(HERO_TEST_VERSION "0.0.74" CACHE', c)
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
        self.assertIn("'shadow_match': 'on'", ls)  # test 3: every fix ON by default
        self.assertIn("'cull_margin': '10deg'", ls)

    def test_test3_light_hold(self):
        import sys
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        import live_settings as L
        self.assertEqual(L.UPDATE38_DEFAULTS['light_hold'], 'on')
        self.assertEqual(L.export_value('light_hold', 'on'), 0)
        self.assertEqual(L.export_value('light_hold', 'strong'), 1)
        self.assertEqual(L.export_value('light_hold', 'off'), 2)
        self.assertEqual(L.export_value('shadow_match', L.UPDATE38_DEFAULTS['shadow_match']), 0)
        self.assertEqual(L.export_value('shadow_match', 'off'), 1)
        self.assertEqual(L.SETTINGS['light_hold'][1], 'SpidyLightHold')
        g = src('src/game_xr.cpp')
        self.assertIn('combat38::shadowMatchOn(c38(combat38::shadowMatch))', g)
        self.assertIn('SpidyEyeLightHold(reinterpret_cast<void*>', g)
        self.assertIn('eye_history::jitterScale(', g)
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('saturate(row0.w)', n)
        self.assertIn('std::atomic<uint32_t> lightHoldPercent{spidy::eye_history::lightHoldOn};', n)
        h = src('include/spidy/combat38.hpp')
        self.assertIn('{"ON", "STRONG", "OFF", nullptr}', h)

    def test_test3_migration_old_defaults(self):
        # A test 2 live.txt holding shadow_match at test 2's default (OFF, written by the build, not picked)
        # gets ON once; old numbers keep test 2's meaning (0 OFF -> default -> ON, 1 was ON); a value set
        # after the marker stays, and a live.txt made by this build starts with the marker.
        import sys, tempfile, pathlib
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        import live_settings as L
        cases = {'shadow_match = off': 'on', 'shadow_match = 0': 'on', 'shadow_match = 1': 'on',
                 'shadow_match = experimental': 'on', 'shadow_match = on': 'on'}
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            for line, want in cases.items():
                p.write_text('# old\nsharpen = 140\n' + line + '  # kept\n', encoding='utf-8')
                first = L.migrate_073t3(p)
                kept, _ = L.persisted(p)
                self.assertEqual(kept['shadow_match'], want, line)
                self.assertEqual(L.export_value('shadow_match', kept['shadow_match']), 0, line)
                self.assertIn(L.MIGRATED_073T3, p.read_text(encoding='utf-8'))
                self.assertIn('sharpen = 140', p.read_text(encoding='utf-8'))
                if line.endswith(('= on',)):
                    self.assertIsNone(first)
                else:
                    self.assertIn('shadow_match = on', first)
                self.assertIsNone(L.migrate_073t3(p))  # once
            # chosen after the marker: stays, numbers in the new order (1 = OFF)
            for line, want in (('shadow_match = off', 'off'), ('shadow_match = 1', 1)):
                p.write_text(L.HEADER + line + '\n', encoding='utf-8')
                self.assertIsNone(L.migrate_073t3(p))
                kept, _ = L.persisted(p)
                self.assertEqual(L.export_value('shadow_match', kept['shadow_match']), 1, line)
            # no live.txt: nothing made; a new one from the hologram carries the marker
            q = pathlib.Path(d) / 'none.txt'
            self.assertIsNone(L.migrate_073t3(q))
            self.assertFalse(q.exists())
            f = L.LiveFile(q, {})
            self.assertTrue(f.adopt({'shadow_match': 'off'}))
            self.assertIn(L.MIGRATED_073T3, q.read_text(encoding='utf-8'))
            self.assertIsNone(L.migrate_073t3(q))
            self.assertEqual(L.persisted(q)[0]['shadow_match'], 'off')
            # unreadable values are left alone, nothing raises
            p.write_text('shadow_match = 7\nshadow_match_x = off\n', encoding='utf-8')
            self.assertIsNone(L.migrate_073t3(p))
            self.assertIn('shadow_match = 7', p.read_text(encoding='utf-8'))
        self.assertEqual(L.migrate_old_defaults('turn = 0\n', {'turn': (('snap', 'smooth'), 'snap', 'smooth')}),
                         {'turn': 'smooth'})  # the helper is generic (any key: old names, old and new default)
        r = src('tools/run_game_vr.py')
        self.assertLess(r.index('migrate_386('), r.index('migrate_073t3('))
        self.assertLess(r.index('migrate_073t3('), r.index("kept, _ = live_settings.persisted(ROOT/'live.txt')"))

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

    def test_test3_van_screen_and_van_on_by_default(self):
        import sys, tempfile, pathlib
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        import live_settings as ls
        # VAN SCREEN and PUNISHER VAN: ON by default (value 0), switchable off.
        t = src('tools/live_settings.py')
        self.assertIn("'punisher_van': 'on'", t)
        self.assertIn("'van_screen': 'on'", t)
        self.assertIn("'punisher_van': ('on', 'off')", t)
        c = src('include/spidy/combat38.hpp')
        self.assertIn('{"van_screen", "VAN SCREEN"', c.replace('{"van_screen","VAN SCREEN"', '{"van_screen", "VAN SCREEN"'))
        self.assertIn('bool punisherVan = true;', src('include/spidy/hero072_play.hpp'))
        # The once-only migration (MIGRATE_073T3): an old OFF default goes ON, an old 1 (was ON) stays ON,
        # an OFF picked after the marker stays OFF.
        self.assertEqual(ls.MIGRATE_073T3['punisher_van'], (('off', 'on'), 'off', 'on'))
        with tempfile.TemporaryDirectory() as d:
            live = pathlib.Path(d) / 'live.txt'
            for line in ('punisher_van = off', 'punisher_van = 0', 'punisher_van = 1', 'punisher_van = on'):
                live.write_text('fov = 90\n' + line + '\n', encoding='utf-8')
                ls.migrate_073t3(live)
                kept, _ = ls.persisted(live)
                self.assertEqual(ls.export_value('punisher_van', kept['punisher_van']), 0, line)
                self.assertIn('fov = 90', live.read_text(encoding='utf-8'))
            live.write_text(ls.HEADER + 'punisher_van = off\n', encoding='utf-8')
            self.assertIsNone(ls.migrate_073t3(live))
            self.assertEqual(ls.export_value('punisher_van', ls.persisted(live)[0]['punisher_van']), 1)
        # The game-side screen never draws behind or without an eye.
        v = src('src/van_screen.cpp')
        self.assertIn('if (!van072::fine(viewer) || dot(viewer - v.centre, n) <= 0)', v)


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


class Hero073Test3SmoothTurnMigration(unittest.TestCase):
    """0.0.73 test 3: an older launcher's settings.ini kept opt.SmoothTurn=0 (its default, snap), which the 0.0.73
    launcher reads as the player picking snap (-SnapTurning); once, on load, it moves to smooth (opt.migrated073t3)."""
    HARNESS = r'''
using System; using System.IO;
namespace Sm2VrLauncher { static class MigrationTest {
    static int fails;
    static void Check(bool ok, string what) { Console.WriteLine((ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; }
    static Settings Fresh(string text) { if (text == null) { if (File.Exists(Settings.FilePath)) File.Delete(Settings.FilePath); }
        else { Directory.CreateDirectory(Settings.Folder); File.WriteAllText(Settings.FilePath, text); } return Settings.Load(); }
    static int Main() {
        var s = Fresh("opt.SmoothTurn=0\nopt.SnapTurn=45\n");
        Check(s.Get("opt.SmoothTurn", null) == "120", "an older launcher's 0 becomes smooth 120");
        Check(s.Get("opt.SnapTurn", null) == "45", "other options stay");
        Check(File.ReadAllText(Settings.FilePath).Contains("opt.migrated073t3=1"), "marker saved");
        Check(File.ReadAllText(Settings.FilePath).Contains("opt.SmoothTurn=120"), "smooth saved");
        s.Set("opt.SmoothTurn", "0"); s.Save();
        Check(Settings.Load().Get("opt.SmoothTurn", null) == "0", "snap picked after the marker stays");
        s = Fresh("opt.SmoothTurn=90\n");
        Check(s.Get("opt.SmoothTurn", null) == "90", "a chosen speed stays");
        s = Fresh(null);
        Check(s.Get("opt.SmoothTurn", "120") == "120" && File.ReadAllText(Settings.FilePath).Contains("opt.migrated073t3=1"), "new install starts migrated");
        Check(!s.Migrate073t3(), "once only");
        s = Fresh("LauncherDesign=hero\nopt.SmoothTurn=0\n");
        Check(s.Get("LauncherDesign", null) == "classic" && s.Get("opt.SmoothTurn", null) == "120", "new design moves to classic (and smooth) once");
        Check(File.ReadAllText(Settings.FilePath).Contains("opt.migrated073classic=1") && File.ReadAllText(Settings.FilePath).Contains("HeroOfferShown=1"), "classic marker saved, no offer");
        s.Set("LauncherDesign", "hero"); s.Save();
        Check(Settings.Load().Get("LauncherDesign", null) == "hero", "the new design turned on after the marker stays (opt-in)");
        s = Fresh("LauncherDesign=classic\n");
        Check(s.Get("LauncherDesign", null) == "classic" && !s.MigrateClassic073(), "classic stays, once only");
        Console.WriteLine(fails == 0 ? "ALL PASS" : "FAILED"); return fails == 0 ? 0 : 1; } } }
'''
    def test_source(self):
        s = src('tools/launcher/src/Settings.cs')
        self.assertIn('public const string Migrated073t3Key = "opt.migrated073t3";', s)
        i = s.index('s.ReadFile(FilePath);')
        self.assertLess(i, s.index('bool moved = s.Migrate073t3();'))
        self.assertLess(s.index('bool moved = s.Migrate073t3();'), s.index('if (s.MigrateClassic073()) moved = true;'))
        self.assertIn('if (moved) s.Save();', s)
        d = re.search(r'SmoothTurnDefault = "(\d+)"', s).group(1)
        self.assertRegex(src('tools/launcher/src/LaunchOptions.cs'), r'"SmoothTurn", "SMOOTH TURN", "[^"]*", 0, 360, 10, %s,' % d)
        self.assertIn('if($SmoothTurn -eq 0 -and -not $SnapTurning) { $SmoothTurn=%s }' % d, src('tools/launch-game-vr.ps1'))
        self.assertIn('int smoothTurn = %s;' % d, src('include/spidy/vr_settings.hpp'))
    @unittest.skipUnless(__import__('shutil').which('mcs') and __import__('shutil').which('mono'), 'needs mono (mcs)')
    def test_migration_runs(self):
        import subprocess, tempfile
        d = tempfile.mkdtemp()
        with open(os.path.join(d, 'launcher.ini'), 'w') as f: f.write('Portable=1\n')
        with open(os.path.join(d, 't.cs'), 'w') as f: f.write(self.HARNESS)
        exe = os.path.join(d, 't.exe')
        r = subprocess.run(['mcs', '-langversion:5', '-target:exe', '-main:Sm2VrLauncher.MigrationTest', '-out:' + exe,
                            os.path.join(ROOT, 'tools/launcher/src/Settings.cs'), os.path.join(d, 't.cs')], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        r = subprocess.run(['mono', exe], capture_output=True, text=True, cwd=d, timeout=120)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('ALL PASS', r.stdout)
        self.assertEqual(r.stdout.count('PASS '), 12)
        self.assertTrue(os.path.isfile(os.path.join(d, 'launcher-data', 'settings.ini')))   # portable: never the user's own

class Hero073Test3ClassicLauncher(unittest.TestCase):
    """0.0.73 test 3: players found the new launcher design laggy - CLASSIC is the default, the new design is opt-in only."""
    def test_classic_default_and_no_offer(self):
        m = src('tools/launcher/src/LauncherMode.cs'); h = src('tools/launcher/src/MainForm.Hero.cs')
        self.assertIn('if (raw == null) return Design.Classic;', m)
        i = h.index('void DesignStartupNotices()'); body = h[i:h.index('void HeroClosed()', i)]
        self.assertNotIn('Restart with new design', body); self.assertNotIn('LauncherMode.Restart', body)
        self.assertNotIn('KeyDesign, LauncherMode.Hero', body)
        self.assertIn('designToggle = new ToggleRow("New launcher design"', h)   # still reachable, opt-in
        self.assertIn('MigratedClassic073Key = "opt.migrated073classic"', src('tools/launcher/src/Settings.cs'))

class Hero073Test3ExportCodes(unittest.TestCase):
    """0.0.73 test 3: every setting export refuses with its own code (VAN SCREEN shared WEAPON COLLISION's 5442)."""
    def test_every_export_code_is_unique(self):
        s = src('src/game_xr.cpp')
        starts = list(re.finditer(r'extern "C" __declspec\(dllexport\) DWORD WINAPI (\w+)\(', s))
        self.assertGreater(len(starts), 200)
        uses = {}
        for i, m in enumerate(starts):
            body = s[m.end():starts[i + 1].start() if i + 1 < len(starts) else len(s)]
            first = body.split('\n')[0]
            if '{ return' in first: body = first
            else:
                k = re.search(r'\n\}', body)
                if k: body = body[:k.start()]
            for c in set(re.findall(r'\b(5\d{3})\b', body)): uses.setdefault(c, []).append(m.group(1))
        self.assertEqual({c: f for c, f in uses.items() if len(f) > 1}, {})
        self.assertEqual(uses.get('5442'), ['SpidyWeaponCollision'])
        self.assertEqual(uses.get('5445'), ['SpidyVanScreen'])
        self.assertEqual(uses.get('5448'), ['SpidyVanPedals'])  # 0.0.73 test 3 VAN PEDALS
        self.assertEqual(uses.get('5453'), ['SpidyDdBatonTip'])  # 0.0.73 DD BATON
        self.assertEqual(uses.get('5462'), ['SpidyIronHands'])  # 0.0.73 test 5 HERO HANDS (5461: MP SOLO TEST, below)
        self.assertEqual(uses.get('5463'), ['SpidyDdGrappleAim'])
        self.assertEqual(uses.get('5464'), ['SpidyWolverineClaws'])
        self.assertEqual(uses.get('5470'), ['SpidyLightChecks'])  # 0.0.73 LIGHT CHECKS
        self.assertEqual(uses.get('5461'), ['SpidyMpSoloTest'])  # 0.0.73 test 6: MP SOLO TEST (mp-solo-test's 5449 is IMAGE's)
        self.assertEqual(uses.get('5474'), ['SpidyClawLevel'])  # 0.0.73 test 6 CLAW LEVEL
        self.assertEqual(uses.get('5477'), ['SpidyMpPlayer2Body'])  # 0.0.73 MPSUIT PLAYER 2 BODY
        self.assertEqual(uses.get('5490'), ['SpidyComingSoon'])  # 0.0.73 COMING SOON
        self.assertEqual(uses.get('5491'), ['SpidyHideouts'])  # 0.0.74 test 1 HIDEOUTS
        self.assertEqual(uses.get('5492'), ['SpidyCarMusic'])  # 0.0.74 test 1 CAR MUSIC
        self.assertEqual(uses.get('5493'), ['SpidySeatedDriving'])  # 0.0.74 test 1 SEATED DRIVING
        self.assertEqual(uses.get('5494'), ['SpidyBatmobile'])  # 0.0.74 test 1 BATMOBILE
        self.assertEqual(uses.get('5495'), ['SpidyBatmobileChoice'])  # 0.0.74 test 1 BATMOBILE CHOICE
        self.assertEqual(uses.get('5498'), ['SpidyCabinClear'])  # 0.0.74 test 2 CABIN CLEAR
        self.assertEqual(uses.get('5499'), ['SpidyCarPaint'])  # 0.0.74 test 2 CAR PAINT
        self.assertEqual(uses.get('5500'), ['SpidySeatedTools'])  # 0.0.74 test 2 SEATED TOOLS
        self.assertEqual(uses.get('5501'), ['SpidyDashPointer'])  # 0.0.74 test 2 DASH POINTER
        self.assertEqual(uses.get('5503'), ['SpidyCrimeGps'])  # 0.0.74 test 2b CRIME GPS
        self.assertEqual(uses.get('5507'), ['SpidyWheelFirst'])  # 0.0.74 test 3 WHEEL FIRST (van072.hpp)
        self.assertEqual(uses.get('5508'), ['SpidyCaveQuiet'])  # 0.0.74 test 3 CAVE QUIET (cave_quiet074.hpp)
        self.assertEqual(uses.get('5509'), ['SpidyFingerRay'])  # 0.0.74 test 3 FINGER RAY (finger_ray074.hpp)
        self.assertEqual(uses.get('5510'), ['SpidyDashInside'])  # 0.0.74 test 3 DASH INSIDE (cabin_look074.hpp)
        self.assertEqual(uses.get('5511'), ['SpidyCockpitGlow'])  # 0.0.74 test 3 COCKPIT GLOW (cabin_look074.hpp)
        self.assertEqual(uses.get('5530'), ['SpidyPunisherQuickHook'])  # 0.0.74 test 10 QUICK HOOK (hero072.hpp)
        self.assertEqual(uses.get('5531'), ['SpidyGrappleYank'])  # 0.0.74 test 10 GRAPPLE YANK (grapple074.hpp)
        self.assertEqual(uses.get('5532'), ['SpidySureGrapple'])  # 0.0.74 test 10 SURE GRAPPLE (grapple074.hpp)
        self.assertEqual(uses.get('5533'), ['SpidyBeltZones'])  # 0.0.74 test 10 BELT ZONES (hero007.hpp)
        self.assertEqual(uses.get('5524'), ['SpidyEntryFacing'])  # 0.0.74 test 10 ENTRY FACING (van072.hpp)
        self.assertEqual(uses.get('5525'), ['SpidyHideoutHold'])  # 0.0.74 test 10 HIDEOUT HOLD (van_screen.hpp)
        self.assertEqual(uses.get('5534'), ['SpidyThrowSwing'])  # 0.0.74 test 10 THROW BY SWING (throw_swing.hpp)
        self.assertEqual(uses.get('5535'), ['SpidyGlideTilt'])  # 0.0.74 test 10 GLIDE TILT (cape_glide.hpp)
        # 0.0.74 test 10: the tests' picture / AA codes are retired with their options (never reused)
        # 0.0.74 test 10 (72's picture): IMAGE 5449, STREAMER MODE/SMOOTH/EYE 5450-5452, GAME TAA 5460, GLOW CHECK
        # 5471, SL WATCH 5472, SHARPEN MATCH 5473 and WINDOW POP FIX 5475 are retired with their options too
        for code in ('5449', '5450', '5451', '5452', '5460', '5471', '5472', '5473', '5475', '5496', '5497', '5502',
                     '5504', '5505', '5506') + tuple(str(c) for c in range(5512, 5524)):
            self.assertIsNone(uses.get(code), code)


class Hero073Test6(unittest.TestCase):
    """0.0.73 test 6: CLAW LEVEL and WINDOW POP FIX (both ON by default, live, switchable off)."""

    def test_claw_level_wired(self):
        g = src('src/game_xr.cpp')
        self.assertIn('c38(combat38::clawLevel) == 0); // 0.0.73 test 6 CLAW LEVEL', g)
        self.assertIn('setHero007(v, combat38::clawLevel, 5474)', g)
        s = src('src/suit_wear.cpp')
        self.assertIn('rig.clawWayLevel[side] = lw', s)
        self.assertIn('useLevel ? rig.clawWayLevel[side] : rig.clawWay[side]', s)
        self.assertIn('inline constexpr float clawLevelDeg = 0.f;', src('include/spidy/suit_wear.hpp'))

    def test_live_settings_defaults_on(self):
        import sys
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        import live_settings as L
        self.assertEqual(L.UPDATE38_DEFAULTS['claw_level'], 'on')
        self.assertEqual(L.export_value('claw_level', 'off'), 1)
        self.assertNotIn('window_pop', L.UPDATE38_DEFAULTS)  # 0.0.74 test 10: WINDOW POP FIX retired (72's picture)


class Hero074T10PictureGone(unittest.TestCase):
    """0.0.74 test 10: 0.0.72's picture. IMAGE, GAME TAA, GAME AA TAA / KEEP MY GAME AA, SHARPEN MATCH, STREAMER
    MODE's switches, WINDOW POP FIX, SL WATCH and GLOW CHECK are gone (switch and code), not only off. STREAMER MODE's
    copy path (streamer_view.hpp) is back, always on with no switch (hero074t10_picture_tests.Monitor)."""

    def test_switches_and_code_gone(self):
        g = src('src/game_xr.cpp')
        for gone in ('SpidyImage(', 'SpidyImageStyleOption(', 'SpidyGlowCheck(', 'SpidySlWatch(', 'SpidySharpenMatch(',
                     'SpidyWindowPop(', 'SpidyStreamerMode(', 'SpidyStreamerSmooth(', 'SpidyStreamerEye(', 'image_style::',
                     'combat38::streamer', 'window_pop::',
                     'sl_watch::', 'eye_sharpen::matchOn'):
            self.assertNotIn(gone, g, gone)
        for header in ('image_style.hpp', 'window_pop.hpp', 'sl_watch.hpp'):
            self.assertFalse(os.path.exists(os.path.join(ROOT, 'include', 'spidy', header)), header)
        n = src('src/native_eye_gpu.cpp')
        self.assertNotIn('eye_sharpen::matchedPercent(', n)
        self.assertNotIn('streamerSmoothing', n)
        self.assertNotIn('cleanImage', n)
        self.assertNotIn('keep-game-aa', src('tools/run_game_vr.py'))
        self.assertNotIn('KeepGameAa', src('tools/launch-game-vr.ps1'))



class Hero073MpSuit(unittest.TestCase):
    """0.0.73 MPSUIT: PLAYER 2 in a game suit (no mod model) is a hero body, not the stick figure (SUIT LOOK, ON)."""

    def test_player2_body_wired(self):
        g = src('src/game_xr.cpp')
        self.assertIn('spidy::mp::bodyOn(c38(combat38::mpPlayer2Body))', g)
        self.assertIn('spidy::mp::appendHeroBody(vertices, gt, spidy::mp::lookOf(v.state), viewer', g)
        self.assertIn('if (!drawnSuit && !drawnBody)', g)  # the figure only as the last resort
        self.assertIn('spidy::mp::setLook(mine, suitPalette.primary, suitPalette.secondary, suitPalette.emblem', g)
        self.assertIn('setHero007(v, combat38::mpPlayer2Body, 5477)', g)
        self.assertIn('src/mp_body.cpp', src('CMakeLists.txt'))
        self.assertIn('{"SUIT LOOK", "FIGURE", nullptr, nullptr}', src('include/spidy/combat38.hpp'))
        self.assertIn('combatMpPlayer2Body', src('src/wrist_hud.cpp'))

    def test_live_settings_default_suit_look(self):
        import live_settings as L
        self.assertEqual(L.UPDATE38_DEFAULTS['mp_player2_body'], 'suit_look')
        self.assertEqual(L.export_value('mp_player2_body', 'suit_look'), 0)
        self.assertEqual(L.export_value('mp_player2_body', 'figure'), 1)
        self.assertEqual(L.SETTINGS['mp_player2_body'][1], 'SpidyMpPlayer2Body')

    def test_buddy_pose_still_decodes(self):
        # tools/mp_test_buddy.py sends test 6's 112-byte pose; the game reads it without a LOOK tail.
        n = src('src/mp_net.cpp')
        self.assertIn('poseSentBytes', n)
        self.assertIn("'L'", n)


if __name__ == '__main__':
    unittest.main()
