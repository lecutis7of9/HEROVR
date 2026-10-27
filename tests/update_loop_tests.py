#!/usr/bin/env python3
"""Update-loop gate (0.0.62): after a launcher installs 0.0.62 and is restarted, it must not offer 0.0.62 again.

The report (26 Oct): "HERO VR Launcher 0.0.6.1" (launcher build 2) installed 0.0.62 into C:\\sm2-port, then said
"Update available: Version 0.0.62" on every start. Every launcher (builds 2-9) offers an update when the mod version
is newer, or when it is the same and version.json's launcher_build is above the RUNNING launcher's build. That exe ran
from outside the install, so the update could not replace it (only <install>\\tools\\launcher\\SM2VRLauncher.exe is
replaced) and launcher_build 9 > 2 offered 0.0.62 forever.
Fix: version.json for a new mod version has no launcher_build (the version bump already offers it to every older
launcher); build 9 counts the launcher installed in the mod folder and hands over to it when it is newer.
This file runs each shipped launcher's own ModVersion.cs (mono) with the update rule they ship, against the
manifest the release scripts produce, for "just installed 0.0.62, restarted"."""
import json, os, pathlib, re, shutil, subprocess, sys, tempfile, unittest, zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tests'))
from regression_sweep_tests import SHIPPED, SHIPPED_B9, SHIPPED_B10, SHIPPED_B11, CUR, src, need, CHECKS   # noqa: E402

# The release assets for this tree's version (0.0.62, then 0.0.63: the same rule, one version on).
ASSETS = pathlib.Path(os.environ.get('HEROVR_ASSETS', '/workspace/herovr-release/github/release-assets-' + CUR))

HARNESS = '''namespace Sm2VrLauncher { static class H { static int Main(string[] a) {
 // the rule every build 2-8 ships: newer mod version, or same version and launcher_build above the running build
 var have = ModVersion.Parse(a[0]); var got = ModVersion.Parse(a[1]); int lb = int.Parse(a[2]); int installed = int.Parse(a[3]);
 bool offer = got.CompareTo(have) > 0 || (got.CompareTo(have) == 0 && lb > RULE);
 System.Console.WriteLine("build=" + LauncherInfo.Build + " offer=" + offer); return offer ? 1 : 0; } } }'''


def offers(modversion_cs, have, got, lb, installed, rule):
    with tempfile.TemporaryDirectory() as d:
        pathlib.Path(d, 'mv.cs').write_text(modversion_cs)
        pathlib.Path(d, 'h.cs').write_text(HARNESS.replace('RULE', rule))
        r = subprocess.run(['mcs', '-nologo', '-out:' + d + '/h.exe', d + '/mv.cs', d + '/h.cs'], capture_output=True, text=True)
        assert r.returncode == 0, r.stdout + r.stderr
        o = subprocess.run(['mono', d + '/h.exe', have, got, str(lb), str(installed)], capture_output=True, text=True)
        return o.returncode == 1


def manifest_launcher_build(m):
    return int(m.get('launcher_build', 0) or 0)


class UpdateLoop(unittest.TestCase):
    def setUp(self):
        need(shutil.which('mcs') and shutil.which('mono'), 'mono')

    def shipped(self):
        for build, z in SHIPPED:
            need(os.path.isfile(z), z)
            with zipfile.ZipFile(z) as zz:
                names = zz.namelist()
                mv = [n for n in names if n.endswith('tools/launcher/src/ModVersion.cs')][0]
                every = ''.join(zz.read(n).decode('utf-8', 'replace') for n in names if n.endswith('.cs'))
                self.assertIn('return got.CompareTo(have) == 0 && m.LauncherBuild > LauncherInfo.Build;', every, z)
                yield build, zz.read(mv).decode('utf-8')

    def test_no_build_offers_0_0_62_again_after_installing_it(self):
        need((ASSETS / 'version.json').is_file(), str(ASSETS / 'version.json'))
        m = json.load(open(ASSETS / 'version.json', encoding='utf-8'))
        lb = manifest_launcher_build(m)
        self.assertEqual(m['version_number'], CUR)
        cur_build = int(re.search(r'public const int Build = (\d+);', src('ModVersion.cs')).group(1))
        for build, mv in self.shipped():
            # before: this version is offered (the self-update path); after installing it and restarting: not again
            self.assertTrue(offers(mv, '0.0.6' if build >= 6 else '0.0.5', m['version'], lb, cur_build, 'LauncherInfo.Build'), build)
            if CUR != '0.0.62':   # 0.0.63: also from 0.0.62 (the outside 0.0.6.1 launcher of the report, with 0.0.62 installed)
                self.assertTrue(offers(mv, '0.0.62', m['version'], lb, cur_build, 'LauncherInfo.Build'), build)
            if CUR not in ('0.0.62', '0.0.63'):   # 0.0.70: also from 0.0.63
                self.assertTrue(offers(mv, '0.0.63', m['version'], lb, cur_build, 'LauncherInfo.Build'), build)
            if CUR not in ('0.0.62', '0.0.63', '0.0.70'):   # 0.0.71: also from 0.0.70
                self.assertTrue(offers(mv, '0.0.70', m['version'], lb, cur_build, 'LauncherInfo.Build'), build)
            self.assertFalse(offers(mv, CUR, m['version'], lb, cur_build, 'LauncherInfo.Build'),
                             f'launcher build {build} run from outside the install would offer {CUR} again (update loop)')
        # launcher build 9 (the 0.0.62 release's own) and, from 0.0.70, build 10 (0.0.63's), with their Math.Max rule
        later = [(SHIPPED_B9, '0.0.62')] if CUR != '0.0.62' else []
        if CUR not in ('0.0.62', '0.0.63'):
            later.append((SHIPPED_B10, '0.0.63'))
        if CUR not in ('0.0.62', '0.0.63', '0.0.70'):   # 0.0.71: build 11 (0.0.70's), with 0.0.70 or 0.0.63 installed
            later.append((SHIPPED_B11, '0.0.70'))
            later.append((SHIPPED_B11, '0.0.63'))
        for (b, z), have in later:
            need(os.path.isfile(z), z)
            with zipfile.ZipFile(z) as zz:
                names = zz.namelist()
                mv9 = zz.read([n for n in names if n.endswith('tools/launcher/src/ModVersion.cs')][0]).decode('utf-8')
                every = ''.join(zz.read(n).decode('utf-8', 'replace') for n in names if n.endswith('.cs'))
            self.assertIn('m.LauncherBuild > Math.Max(LauncherInfo.Build, SystemInfo.InstalledLauncherBuild(root))', every)
            self.assertIn('public const int Build = %d;' % b, mv9)
            self.assertTrue(offers(mv9, have, m['version'], lb, b, 'System.Math.Max(LauncherInfo.Build, installed)'),
                            f'launcher build {b} with {have} installed is not offered {CUR}')
            for installed in (b, cur_build):
                self.assertFalse(offers(mv9, CUR, m['version'], lb, installed, 'System.Math.Max(LauncherInfo.Build, installed)'),
                                 f'launcher build {b} would offer {CUR} again (update loop)')
        b9 = src('ModVersion.cs')
        self.assertFalse(offers(b9, CUR, m['version'], lb, cur_build, 'System.Math.Max(LauncherInfo.Build, installed)'))

    def test_the_old_manifest_reproduces_the_loop(self):
        # the published-at-6:2x manifest had launcher_build 9: the same rule offers 0.0.62 again for builds 2-8
        for build, mv in self.shipped():
            self.assertTrue(offers(mv, '0.0.62', 'Version 0.0.62', 9, 9, 'LauncherInfo.Build'), build)

    def test_the_release_manifest_gate_rejects_launcher_build(self):
        sys.path.insert(0, str(CHECKS))
        import check_release_manifest as c
        with tempfile.TemporaryDirectory() as d:
            for n in ('HERO-VR-Launcher-9.9.9.zip', 'SHA256SUMS.txt', 'HERO-VR-9.9.9-mod-win64.zip'):
                pathlib.Path(d, n).write_bytes(b'x')
            import hashlib
            f = dict(kind='prebuilt', name='HERO-VR-9.9.9-mod-win64.zip', url='https://x/releases/download/v9.9.9/HERO-VR-9.9.9-mod-win64.zip',
                     sha256=hashlib.sha256(b'x').hexdigest(), size=1)
            m = dict(tag='v9.9.9', version_number='9.9.9', files=[f], launcher_build_in_zip=9)
            pathlib.Path(d, 'version.json').write_text(json.dumps(m))
            self.assertEqual(c.problems(d, 'v9.9.9', 9), [])
            m['launcher_build'] = 9; pathlib.Path(d, 'version.json').write_text(json.dumps(m))
            self.assertTrue(any('update loop' in p for p in c.problems(d, 'v9.9.9', 9)))
            self.assertEqual(c.problems(d, 'v9.9.9', 9, True), [])
        for fin, tag in ((pathlib.Path('/workspace/herovr-release/finalize_00062.sh'), 'v0.0.62 --min-launcher-build 9'),
                         (pathlib.Path('/tmp/rel/finalize_%s.sh' % CUR.replace('.', '').zfill(5)), 'v%s --min-launcher-build ' % CUR)):
            if fin.is_file():
                t = fin.read_text()
                self.assertIn("m['launcher_build_in_zip']=lb", t)
                self.assertIn('check_release_manifest.py . --tag ' + tag, t)

    def test_build_9_hands_over_to_the_installed_launcher(self):
        u, f, i = src('MainForm.Updates.cs'), src('MainForm.cs'), src('SystemInfo.cs')
        self.assertIn('m.LauncherBuild > Math.Max(LauncherInfo.Build, SystemInfo.InstalledLauncherBuild(root))', u)
        self.assertIn('SystemInfo.NewerInstalledLauncher(root) : null', u)
        self.assertIn('RestartLauncher(msg, installedExe)', u)
        self.assertIn('RestartLauncher("", installedExe)', f)
        self.assertLess(f.index('NewerInstalledLauncher'), f.index('StartupUpdateCheck()'))
        self.assertIn('public static int InstalledLauncherBuild(string root)', i)
        self.assertIn('InstalledLauncherBuild(root) > LauncherInfo.Build ? exe : null', i)

    def test_revert_skips_the_key_older_launchers_compute(self):
        # e2e 26 Oct 08:15 (rv6): Revert to 0.0.6 wrote SkipUpdate=0.0.62#9 (the reverting launcher's own build), but the
        # 0.0.6 launcher computes 0.0.62#0 from a version.json without launcher_build, so it offered 0.0.62 again at once.
        need((ASSETS / 'version.json').is_file(), str(ASSETS / 'version.json'))
        m = json.load(open(ASSETS / 'version.json', encoding='utf-8'))
        want = m['version_number'] + '#' + str(manifest_launcher_build(m))
        for build, z in SHIPPED:
            with zipfile.ZipFile(z) as zz:
                every = ''.join(zz.read(n).decode('utf-8', 'replace') for n in zz.namelist() if n.endswith('.cs'))
            self.assertIn('static string UpdateKey(Manifest m) { return (m.VersionNumber ?? m.Version) + "#" + m.LauncherBuild; }', every, build)
            self.assertIn('if (settings.Get("SkipUpdate", "") == UpdateKey(m))', every, build)
        u = src('MainForm.Updates.cs')
        rv = u[u.index('void UsePreviousVersion'):u.index('void RunInstall')]
        self.assertNotIn('"#" + LauncherInfo.Build', rv, 'revert guesses the skip key from its own launcher build')
        self.assertIn('newer = UpdateKey(Manifest.Latest(repo, out rel, inst.Log))', rv)
        self.assertIn('newer = RevertSkipKey(ownNumber)', rv)
        self.assertIn('internal static string RevertSkipKey(string number) { return number + "#0"; }', u)
        self.assertEqual(want, m['version_number'] + '#0', 'the fallback key assumes version.json has no launcher_build')

    def test_the_e2e_has_the_outside_launcher_restart_mode(self):
        s = pathlib.Path('/workspace/herovr-release/e2e62/run.sh')
        need(s.is_file(), str(s))
        self.assertIn('outside)', s.read_text())


if __name__ == '__main__':
    unittest.main()
