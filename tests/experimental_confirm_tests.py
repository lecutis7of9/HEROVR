"""0.0.71 gate (0.0.72: the same, for the 0.0.72 release): the EXPERIMENTAL / STABLE notice before every update (user, Oct 27).

"When a player clicks update to this build, they must click a confirmation acknowledging that the newest live build is
EXPERIMENTAL, may crash or have issues, and that 0.0.63 is the most recent STABLE build (and they can revert to it)."

Launcher build 12 (this tree):
  - UpdateNow, the one path every update takes (the start-up prompt calls UpdateNow(true), the Update now button
    UpdateNow()), calls ConfirmExperimental after its own question and before anything is downloaded or installed;
  - ConfirmExperimental shows a Yes/No warning with No as the default and returns false (update stops) unless Yes;
  - version.json "experimental" (default true, "false" turns it off) and "stable_version" (default 0.0.63) drive it,
    and the text names the new version, EXPERIMENTAL, "may crash or have issues", the STABLE version and Revert.
Builds 2-11 (already on players' PCs) cannot show a new window. Their own update windows show version.json's text:
the start-up prompt (Yes/No/Cancel) shows "version" and "whats_new", Update now (OK/Cancel) shows "version". So the
release's version.json must carry the warning in both, while "version_number" (what every build compares) stays the
plain 3-part version. Checked on the release's version.json (HERO_RELEASE_MANIFEST) and, when given, on the shipped
launcher sources (HERO_OLD_LAUNCHER_SRC, os.pathsep-separated src folders).
Needs mcs (mono) or csc for the compiled checks; skipped with a message when neither is there."""
import json
import os
import pathlib
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT/'tools'/'launcher'/'src'
STABLE = '0.0.63'   # the 0.0.74 release: 0.0.63 (test 10 used 0.0.73)
REL = (ROOT/'VERSION').read_text().strip()   # the release being checked (0.0.72)


def src(n, d=SRC): return (pathlib.Path(d)/n).read_text(encoding='utf-8')


def method(text, sig):
    i = text.index(sig); j = text.index('{', i); depth = 0
    for k in range(j, len(text)):
        depth += {'{': 1, '}': -1}.get(text[k], 0)
        if depth == 0: return text[i:k + 1]
    raise AssertionError('unbalanced ' + sig)


def compiler():
    for c in ('mcs', 'csc'):
        if shutil.which(c): return c
    return None


HARNESS = r'''
using System; using System.IO;
namespace Sm2VrLauncher {
  static class SystemInfo { public static bool IsUpdateZipName(string n) { return false; } }
  static class H { static int Main(string[] a) {
    var m = Manifest.Parse(File.ReadAllText(a[0]));
    Console.WriteLine("EXPERIMENTAL=" + m.Experimental);
    Console.WriteLine("STABLE=" + ExperimentalNotice.StableVersionOf(m));
    Console.WriteLine("NEW=" + ExperimentalNotice.NewVersionOf(m));
    Console.WriteLine("VER=" + m.Ver.Number);
    Console.WriteLine("TEXT=" + ExperimentalNotice.Text(m, "0.0.70").Replace("\n", "|"));
    return 0; } } }
'''


class ExperimentalConfirm(unittest.TestCase):
    def test_every_update_path_confirms_before_download(self):
        u = src('MainForm.Updates.cs')
        now = method(u, 'void UpdateNow(bool noConfirm = false)')
        c = now.index('if (!ConfirmExperimental(m)) return;')
        self.assertLess(now.index('MessageBoxButtons.OKCancel'), c)   # after the player's own "Update X to Y?"
        for later in ('AskUnverified()', 'RunInstall(', 'Download'):
            if later in now: self.assertLess(c, now.index(later), later)
        self.assertEqual(now.count('ConfirmExperimental'), 1)
        offer = method(u, 'void OfferUpdate(Manifest m)')
        self.assertIn('UpdateNow(true)', offer)   # the start-up prompt goes through UpdateNow too
        self.assertNotIn('RunInstall(', offer)
        self.assertEqual(u.count('btnUpdateNow.Click += (s, e) => UpdateNow();'), 1)

    def test_confirmation_dialog(self):
        f = method(src('MainForm.Experimental.cs'), 'bool ConfirmExperimental(Manifest m)')
        self.assertIn('MessageBoxButtons.YesNo', f)
        self.assertIn('MessageBoxDefaultButton.Button2', f)   # No is the default: Enter never updates
        self.assertIn('== DialogResult.Yes', f)
        self.assertIn('return ok;', f)
        self.assertIn('if (m == null || !m.Experimental) return true;', f)
        self.assertIn('Update cancelled: the experimental notice', f)

    def test_manifest_fields_and_text(self):
        c = compiler()
        if not c: self.skipTest('no mcs/csc: compiled checks run in CI (windows) and the release gates (mono)')
        with tempfile.TemporaryDirectory() as t:
            t = pathlib.Path(t); (t/'h.cs').write_text(HARNESS)
            exe = t/'h.exe'
            files = [str(SRC/n) for n in ('Manifest.cs', 'ModVersion.cs', 'ExperimentalNotice.cs', 'GitHub.cs')]
            r = subprocess.run([c, '-nologo', '-r:System.Web.Extensions.dll', '-out:' + str(exe), str(t/'h.cs')] + files,
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            run = ['mono', str(exe)] if c == 'mcs' else [str(exe)]

            def parse(d):
                p = t/'v.json'; p.write_text(json.dumps(d))
                o = subprocess.run(run + [str(p)], capture_output=True, text=True, check=True).stdout
                return dict(l.split('=', 1) for l in o.splitlines())
            base = dict(name='HERO VR', version='Version 0.0.72', version_number='0.0.72', tag='v0.0.72', files=[])
            d = parse(base)
            self.assertEqual((d['EXPERIMENTAL'], d['STABLE'], d['NEW']), ('True', STABLE, '0.0.72'))   # default: on
            for w in ('0.0.72 is an EXPERIMENTAL build', 'may crash or have issues', STABLE + ' is the most recent STABLE build',
                      'open Revert and press USE ' + STABLE, 'No: stay on 0.0.70'):
                self.assertIn(w, d['TEXT'])
            self.assertEqual(parse(dict(base, experimental=False))['EXPERIMENTAL'], 'False')
            self.assertEqual(parse(dict(base, experimental=True, stable_version='0.0.72'))['STABLE'], '0.0.72')
            rel = os.environ.get('HERO_RELEASE_MANIFEST')
            if rel:
                d = parse(json.loads(pathlib.Path(rel).read_text(encoding='utf-8')))
                self.assertEqual((d['EXPERIMENTAL'], d['STABLE'], d['NEW'], d['VER']), ('True', STABLE, REL, REL))

    def test_release_manifest_carries_the_warning_for_old_launchers(self):
        rel = os.environ.get('HERO_RELEASE_MANIFEST')
        if not rel: self.skipTest('HERO_RELEASE_MANIFEST not given (the release gates give it)')
        m = json.loads(pathlib.Path(rel).read_text(encoding='utf-8'))
        self.assertEqual(m['version_number'], REL)   # what every build compares: unchanged
        self.assertIs(m.get('experimental'), True); self.assertEqual(m.get('stable_version'), STABLE)
        for field in ('version', 'whats_new'):
            v = m[field]
            for w in ('EXPERIMENTAL', 'may crash or have issues', STABLE, 'STABLE', 'Revert'):
                self.assertIn(w, v, field)
        self.assertTrue(m['version'].startswith('Version ' + REL + ' '))
        # every build's ShortVersion / ModVersion.Parse takes the FIRST 3-part version: the release's, never 0.0.63
        self.assertEqual(re.search(r'\d+\.\d+\.\d+', m['version']).group(0), REL)
        self.assertIn('THOR', m['whats_new'])

    def test_old_launchers_show_the_text(self):
        dirs = [d for d in os.environ.get('HERO_OLD_LAUNCHER_SRC', '').split(os.pathsep) if d]
        if not dirs: self.skipTest('HERO_OLD_LAUNCHER_SRC not given (the release gates give the shipped sources)')
        for d in dirs:
            u = src('MainForm.Updates.cs', d)
            offer = method(u, 'void OfferUpdate(Manifest m)')
            self.assertIn('m.Version', offer, d); self.assertIn('m.WhatsNew', offer, d)
            self.assertIn('MessageBoxButtons.YesNoCancel', offer, d)
            now = method(u, 'void UpdateNow(bool noConfirm = false)')
            self.assertIn('" to " + m.Version + "?', now, d)
            self.assertIn('MessageBoxButtons.OKCancel', now, d)
            self.assertIn('ModVersion.Parse(VersionNumber) ?? ModVersion.Parse(Version)', src('Manifest.cs', d), d)


if __name__ == '__main__':
    unittest.main(verbosity=2)
