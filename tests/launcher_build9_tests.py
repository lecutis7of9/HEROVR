"""Version 0.0.62, launcher build 9: Revert offers both earlier releases (0.0.6 and 0.0.5), and the Nexus
"crashed right away" fixes (/workspace/nexus-crash/REPORT.txt): the first-run HERO VR Launcher.cmd and build.ps1 cope
with a folder name in brackets, running from inside the zip and an antivirus removing the built launcher, and keep
tools\\launcher\\build.log; the launcher's crash log falls back to %TEMP% when its settings folder is what failed."""
import pathlib, re, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
# The public repo carries the launcher in tools/launcher and the .cmd at its root; the dev tree keeps them beside.
LAUNCHER = next((p for p in (ROOT/'tools/launcher', ROOT.parent/'herovr-launcher-0062') if (p/'src').is_dir()),
                ROOT/'tools/launcher')
CMD = next((p for p in (ROOT/'HERO VR Launcher.cmd', ROOT.parent/'herovr-release/text-0.0.63/HERO VR Launcher.cmd', ROOT.parent/'herovr-release/text-0.0.62/HERO VR Launcher.cmd')
            if p.is_file()), ROOT/'HERO VR Launcher.cmd')
def src(n): return (LAUNCHER/'src'/n).read_text(encoding='utf-8')

@unittest.skipUnless((LAUNCHER/'src').is_dir(), 'launcher sources not beside this tree')
class LauncherBuild9(unittest.TestCase):
    def test_version_and_build(self):
        mv = src('ModVersion.cs')
        self.assertGreaterEqual(int(re.search(r'public const int Build = (\d+);', mv).group(1)), 9)
        v = re.search(r'public const string Version = "([0-9.]+)";', mv).group(1)
        # the launcher version is the mod version plus its own number (0.0.62.2 for 0.0.62, 0.0.63.1 for 0.0.63)
        self.assertTrue(v.startswith((ROOT/'VERSION').read_text(encoding='utf-8').strip() + '.'), v)
        ai = src('AssemblyInfo.cs')
        self.assertIn('AssemblyVersion("%s")' % v, ai)
        self.assertIn('(launcher build %d)' % int(re.search(r'public const int Build = (\d+);', mv).group(1)), ai)

    def test_revert_offers_both_earlier_releases(self):
        mv = src('ModVersion.cs')
        # build 9 (0.0.62): 0.0.6 and 0.0.5; build 10 (0.0.63): 0.0.62 first, then 0.0.6 and 0.0.5 (newest first)
        tags = re.search(r'public static readonly string\[\] PreviousTags = \{ ([^}]*) \};', mv).group(1)
        tags = [t.strip().strip('"') for t in tags.split(',')]
        self.assertEqual(tags[-2:], ['v0.0.6', 'v0.0.5'])
        cmk = re.search(r'set\(HERO_TEST_VERSION "([0-9.]+)"', (ROOT/'CMakeLists.txt').read_text(encoding='utf-8'))
        test_ver = tuple(int(x) for x in cmk.group(1).split('.')) if cmk else ()
        if test_ver >= (0, 0, 74):   # 0.0.74 test 10 (the user: "go to 73 ... not 6whatever"): 0.0.73 first, the rest kept
            self.assertEqual(tags, ['v0.0.73', 'v0.0.63', 'v0.0.62', 'v0.0.6', 'v0.0.5'])
            self.assertIn('public const string PreviousTag = "v0.0.73";', mv)
        elif int(re.search(r'public const int Build = (\d+);', mv).group(1)) >= 11:   # build 11 (0.0.70): 0.0.63 first
            self.assertEqual(tags, ['v0.0.63', 'v0.0.62', 'v0.0.6', 'v0.0.5'])
        elif int(re.search(r'public const int Build = (\d+);', mv).group(1)) >= 10:
            self.assertEqual(tags, ['v0.0.62', 'v0.0.6', 'v0.0.5'])
            self.assertIn('public const string PreviousTag = "v0.0.62";', mv)
        cur = tuple(int(x) for x in (ROOT/'VERSION').read_text(encoding='utf-8').strip().split('.'))
        cur = max(cur, test_ver)   # a test build (0.0.74) still carries the release's VERSION (0.0.73)
        self.assertTrue(all(tuple(int(x) for x in t[1:].split('.')) < cur for t in tags), tags)
        rd = src('RevertDialog.cs')
        self.assertIn('foreach (string tag in LauncherInfo.PreviousTags)', rd)
        self.assertIn('PreviousTag = t;', rd)
        self.assertIn('OfferPrevious(root, t)', rd)
        # numbers compared as numbers: 0.0.62 is newer than both
        self.assertIn('return prev != null && (have == null || have > prev);', rd)
        # 0.0.74 test 10: a test build (VERSION still 0.0.73) is compared by its marker's version, so USE 0.0.73 is offered
        self.assertIn('"HERO-VR-TEST-BUILD.txt"', rd); self.assertIn('var test = TestBuildVersion(root);', rd)
        up = src('MainForm.Updates.cs')
        self.assertIn('UsePreviousVersion(rs, d.PreviousTag)', up)
        self.assertIn('"/releases/download/" + tag + "/version.json"', up)
        self.assertIn('settings.Get("PreviousManifestUrl." + tag, "")', up)
        self.assertIn('SM2VR_LAUNCHER_TEST_PREVIOUS_TAG', up)

    def test_semver_is_numeric(self):
        mv = src('ModVersion.cs')
        self.assertRegex(mv, r'public (readonly )?int Major, Minor, Patch|int Patch')

    def test_crash_log_falls_back_to_temp(self):
        p = src('Program.cs')
        i = p.index('public static string LogError(Exception ex)')
        body = p[i:p.index('static void Crash(', i)]
        self.assertIn('Path.GetTempPath()', body)
        self.assertLess(body.index('Settings.Folder'), body.index('Path.GetTempPath()'))
        self.assertIn('try { Telemetry.NoteLauncherCrash(ex); } catch { }', p)

    def test_build_ps1_literal_paths_and_log(self):
        b = (LAUNCHER/'build.ps1').read_text(encoding='utf-8')
        self.assertIn('Set-Location -LiteralPath $PSScriptRoot', b)
        self.assertIn('Start-Transcript -LiteralPath (Join-Path $PSScriptRoot "build.log")', b)
        self.assertIn('Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot "src") -Filter *.cs', b)
        self.assertNotIn('Get-ChildItem src\\*.cs', b)
        self.assertNotIn('Set-Location $PSScriptRoot', b)

    def test_real_anchors_option(self):
        # 0.0.62 REAL ANCHORS: a launcher switch, default ON, passed as -RealAnchors (the script's -NoAirWebs).
        lo = src('LaunchOptions.cs')
        m = re.search(r'Opt\.Sw\("WEBS & MOVEMENT", "RealAnchors", "REAL ANCHORS", "([^"]+)", (true|false)\)', lo)
        self.assertIsNotNone(m)
        self.assertEqual(m.group(2), 'true')
        self.assertLessEqual(len(m.group(1)), 100)
        ps = (ROOT/'tools/launch-game-vr.ps1').read_text(encoding='utf-8')
        self.assertIn('[switch]$RealAnchors', ps)
        self.assertIn('if($NoAirWebs -or $RealAnchors)', ps)

@unittest.skipUnless(CMD.is_file(), 'HERO VR Launcher.cmd not beside this tree')
class FirstRunCmd(unittest.TestCase):
    def test_cmd(self):
        raw = CMD.read_bytes()
        self.assertEqual(raw.count(b'\n'), raw.count(b'\r\n'), 'CRLF line ends (cmd.exe labels)')
        c = raw.decode('utf-8')
        for label in ('notunzipped', 'buildfailed', 'blocked'):
            self.assertIn('goto %s' % label, c); self.assertIn('\r\n:%s\r\n' % label, c)
        self.assertIn('if not exist "tools\\launcher\\build.ps1" goto notunzipped', c)
        self.assertIn('if not exist "%EXE%" goto blocked', c)
        self.assertIn('build.log', c)
        self.assertNotIn('HERO-VR-0.0.6 folder', c)   # no version in the advice
        self.assertTrue(c.isascii())

if __name__ == '__main__':
    unittest.main()
