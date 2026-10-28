"""Launcher build 10 (branch launcher-redesign): the new HERO VR design (the concept video) beside the classic one.
Asset loading and every fallback (damaged orb/sound/font, renderer error, crash, hang, Shift/--classic, corrupt
setting), the window at 720p/1080p/4K and 100-200% DPI on several monitors, the seamless sound loop, and the rules
the past player issues taught (see regression_sweep_tests.py for those gates on the whole tree)."""
import os, pathlib, re, shutil, subprocess, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
LAUNCHER = ROOT/'tools/launcher'
SRC = LAUNCHER/'src'
API = os.environ.get('NETFX_API', '/usr/lib/mono/4.8-api')
def src(n): return (SRC/n).read_text(encoding='utf-8')
def all_src(): return {p.name: p.read_text(encoding='utf-8') for p in SRC.glob('*.cs')}
HAVE_MONO = shutil.which('mcs') and shutil.which('mono') and os.path.isfile(API + '/mscorlib.dll')
RES = ['orb.bin', 'intro.wav', 'loop.wav', 'Barlow-Regular.ttf', 'Barlow-Medium.ttf']

class Sources(unittest.TestCase):
    def test_assets_embedded_by_both_builds(self):
        for f in RES: self.assertTrue((LAUNCHER/'assets/hero'/f).is_file(), f)
        sh = (LAUNCHER/'build.sh').read_text(); ps = (LAUNCHER/'build.ps1').read_text()
        for f in RES:
            self.assertIn('-resource:assets/hero/%s,hero.%s' % (f, f), sh)
            self.assertIn('/resource:assets\\hero\\%s,hero.%s' % (f, f), ps)
        self.assertTrue((LAUNCHER/'assets/hero/OFL.txt').is_file() or any((LAUNCHER/'assets/hero').glob('*OFL*')), 'Barlow licence')

    def test_sound_is_waveout_not_soundplayer(self):
        s = all_src()
        self.assertFalse(any(re.search(r'\bnew\s+(System\.Media\.)?SoundPlayer\b|using System\.Media', t) for t in s.values()), 'SoundPlayer is not allowed')
        w = s['WaveAudio.cs']
        for fn in ('waveOutOpen', 'waveOutWrite', 'waveOutReset', 'waveOutClose', 'waveOutUnprepareHeader'): self.assertIn(fn, w)
        h = s['MainForm.Hero.cs']
        self.assertIn('HeroGameStarting', s['MainForm.cs'])   # stops when a game starts (both PLAY VR and FLATSCREEN)
        self.assertEqual(s['MainForm.cs'].count('HeroGameStarting();'), 2)
        # a failed sound device: the launcher runs silently (no fallback, no message box)
        i = h.index('void HeroStartSound'); body = h[i:h.index('\n        }', i)]
        self.assertIn('Start(', body); self.assertNotIn('MessageBox', body); self.assertNotIn('FallBack', body)

    def test_settings_have_subtexts(self):
        h = src('MainForm.Hero.cs')
        for title in ('New launcher design', 'Launcher sound'):
            m = re.search(r'"%s",\s*"([^"]+)"' % re.escape(title), h)
            self.assertIsNotNone(m, title)
            self.assertEqual(len(re.findall(r'[.!?](\s|$)', m.group(1))), 1, 'one sentence: ' + m.group(1))
        self.assertIn('Switch to classic launcher', h)
        self.assertIn('(restarts the launcher)', h)

    def test_opt_in_prompt_text(self):
        h = src('MainForm.Hero.cs')
        self.assertIn('New launcher design available. Would you like to restart now with the new design? You can safely '
                      'switch back to the original anytime if you prefer it or run into issues.', h)
        self.assertIn('"Restart with new design"', h); self.assertIn('"Not now"', h)

    def test_classic_is_the_safe_default(self):
        m = src('LauncherMode.cs'); p = src('Program.cs')
        self.assertIn('if (raw == null) return Design.Classic;', m)
        self.assertIn('"--classic"', m)
        self.assertIn('ShiftHeld()', p)
        # every failure while building the new design's window opens the classic one
        i = p.index('CreateWindow('); body = p[i:i + 2500]
        self.assertIn('HeroAssets.Load()', body); self.assertIn('WriteMarker()', body)
        self.assertIn('Theme.ApplyClassic()', body); self.assertIn('FallBack(', body)

    def test_native_controls_never_get_the_private_font(self):
        # Windows' own controls (GDI text) cannot use a PrivateFontCollection font: text boxes and dialogs use Theme.Native.
        s = all_src()
        self.assertIn('Font = Theme.NUi(13)', s['Widgets.cs'])
        for f in ('ReportDialogs.cs', 'RevertDialog.cs'):
            self.assertNotRegex(s[f], r'Theme\.(Ui|UiBold|F)\(', f)
        nd = s['MainForm.Hero.cs'][s['MainForm.Hero.cs'].index('class NoticeDialog'):]
        self.assertNotRegex(nd, r'Theme\.(Ui|UiBold|F)\(')

    def test_clock_resumes_after_a_sheet_opened_at_start(self):
        # 0.0.63 e2e: the Updates sheet opened by the startup mod install before the first tick stopped the clock
        # with heroStarted still false, so closing the sheet left the home black for good.
        s = (LAUNCHER/'src/MainForm.Hero.cs').read_text()
        i = s.index('void HeroPauseCheck()'); body = s[i:s.index('void HeroSheetChanged()')]
        self.assertIn('(heroStarted || shownWatch.IsRunning)', body)

    def test_animation_pauses(self):
        h = src('MainForm.Hero.cs')
        i = h.index('void HeroPauseCheck'); body = h[i:h.index('\n        }', i)]
        for cond in ('FormWindowState.Minimized', 'SheetOpen', 'gameOn'): self.assertIn(cond, body)
        self.assertIn('DoubleBuffered', src('MainForm.cs') + h)

    def test_no_net48_only_apis(self):
        r = subprocess.run(['python3', str(LAUNCHER/'tools'/'check-netfx.py'), str(SRC)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

BASE = '9ebe3a4'   # launcher-redesign baseline: the live launcher 0.0.62.2 (build 9, revert fix) unchanged
def base(name):
    r = subprocess.run(['git', 'show', '%s:tools/launcher/src/%s' % (BASE, name)], cwd=str(ROOT), capture_output=True, text=True)
    return r.stdout if r.returncode == 0 else None
def method(text, sig):
    i = text.index(sig); j = text.index('\n        {', i); depth = 0
    for k in range(j, len(text)):
        if text[k] == '{': depth += 1
        elif text[k] == '}':
            depth -= 1
            if depth == 0: return text[i:k + 1]
    raise AssertionError(sig)

class PastIssues(unittest.TestCase):
    """Every issue players hit before, checked against the new design: it shares the classic window's code for all of
    them (regression_sweep_tests.py and the build 8/9 tests run on this tree too and pin the fixes themselves)."""
    BACKEND = ('Installer.cs', 'GitHub.cs', 'Manifest.cs', 'Backups.cs', 'Settings.cs', 'SystemInfo.cs', 'Telemetry.cs',
               'MainForm.Updates.cs', 'ModVersion.cs', 'Clixml.cs', 'Screenshot.cs')

    def test_backend_is_unchanged(self):
        # self-update (releases/latest/download/version.json), the bundled Python check and repair, stale module
        # downloads, the placeholder-repo fallback, Revert to 0.0.6 and 0.0.5, the logs: the same code as build 9.
        changed = []
        for f in self.BACKEND:
            old = base(f)
            if old is None or not (SRC/f).exists(): continue
            new = (SRC/f).read_text(encoding='utf-8')
            if f == 'ModVersion.cs':   # only the launcher version (0.0.63.1), the build number and its note, and (0.0.63) Revert's tags
                new = re.sub(r'public const string Version = "[0-9.]+";', '', new); old = re.sub(r'public const string Version = "[0-9.]+";', '', old)
                old = re.sub(r'public const int Build = \d+;', '', re.sub(r'/// <summary>The launcher.s own build number.*?</summary>', '', old, flags=re.S))
                new = re.sub(r'public const int Build = \d+;', '', re.sub(r'/// <summary>The launcher.s own build number.*?</summary>', '', new, flags=re.S))
                # 0.0.63: Revert offers 0.0.62 too (launcher_build9_tests pins the list); the rest of the file stays build 9's
                tag_rx = r'public const string PreviousTag = "v[0-9.]+";|/// <summary>Build 9 \(Version 0.0.62\): every release REVERT.*?public static readonly string\[\] PreviousTags = \{[^}]*\};'
                new = re.sub(tag_rx, '', new, flags=re.S); old = re.sub(tag_rx, '', old, flags=re.S)
            if f == 'MainForm.Updates.cs':   # 0.0.63: only the log header's positions (clear of the new design's BACK  ESC)
                hdr = r'var logCard = card;.*?btnDevBuild\.Visible'
                nh = re.search(hdr, new, re.S).group(0)
                for name in ('"Build & update log"', 'new LogBox(), 556, 48, 532, 488', '"Copy"', '"Logs"', 'Build from source (dev)'):
                    self.assertIn(name, nh)
                new = re.sub(hdr, '', new, flags=re.S); old = re.sub(hdr, '', old, flags=re.S)
            if f == 'MainForm.Updates.cs':   # 0.0.71 (build 12): the one call to the EXPERIMENTAL / STABLE confirmation (experimental_confirm_tests.py)
                line = '\n            if (!ConfirmExperimental(m)) return;   // build 12: the EXPERIMENTAL / STABLE notice, confirmed before any download'
                self.assertEqual(new.count(line), 1); new = new.replace(line, '')
            if f == 'Manifest.cs':   # 0.0.71 (build 12): "experimental" and "stable_version" for that notice; nothing else
                for line in [l for l in new.split('\n') if '// build 12' in l]:
                    self.assertTrue('Experimental' in line or 'StableVersion' in line, line); new = new.replace(line + '\n', '')
                self.assertNotIn('Experimental', new)
            if old != new: changed.append(f)
        self.assertEqual(changed, [])
        self.assertIn('releases/latest/download/version.json', ''.join(all_src().values()))

    def test_swapchain_flat_fallback_and_play_paths_shared(self):
        # issue: swapchain XrResult -2 sent players to flat screen. PLAY VR in the new design is the classic PlayVr(),
        # changed only by the one line that stops the orb and the sound.
        h = src('MainForm.Hero.cs'); m = src('MainForm.cs'); old = base('MainForm.cs')
        self.assertIn('case 0: if (job == Job.Vr) StopVr(); else PlayVr(); break;', h)
        self.assertIn('case 1: PlayFlat(); break;', h)
        self.assertIn('case 4: Revert(); break;', h)
        if old is None: self.skipTest('baseline commit not in this tree (unpacked zip, no git history)')
        for sig in ('void PlayVr()', 'void PlayFlat()', 'void StopVr('):
            self.assertEqual(method(m, sig).replace('\n            HeroGameStarting();', '').replace('\n                HeroGameStarting();', ''), method(old, sig), sig)

    def test_update_python_and_stale_modules_untouched_by_the_new_design(self):
        h = '\n'.join(all_src()[f] for f in ('MainForm.Hero.cs', 'HeroScene.cs', 'HeroAssets.cs', 'LauncherMode.cs', 'OrbPlayer.cs', 'WaveAudio.cs'))
        for word in ('python', 'Installer.', 'Directory.Delete', 'GitHub.'):
            self.assertNotIn(word, h, 'the new design must not touch ' + word)
        for f in ('HeroAssets.cs', 'LauncherMode.cs'):   # files it writes: only its own, in the settings folder or %TEMP%
            for m_ in re.finditer(r'File\.(Delete|WriteAll\w+|Create)\(([^;]*)', all_src()[f]):
                self.assertRegex(m_.group(2), r'^(p|f|tmp|MarkerPath|probe|path|target|dir)\b|write-test|Log|fonts', m_.group(0))

    def test_trim_char_crash(self):
        r = subprocess.run(['python3', str(LAUNCHER/'tools'/'check-netfx.py'), str(SRC)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        for f in ('MainForm.Hero.cs', 'HeroScene.cs', 'HeroAssets.cs', 'LauncherMode.cs', 'OrbPlayer.cs', 'WaveAudio.cs'):
            self.assertNotRegex(all_src()[f], r"\.(Trim|TrimStart|TrimEnd|Split)\('", f)

    def test_bracketed_and_special_paths(self):
        # the restart passes the exe path quoted; nothing in the new design builds a wildcard or a cmd line by hand
        mode = src('LauncherMode.cs')
        i = mode.index('public static bool Restart'); body = mode[i:i + 1500]
        self.assertIn('GetExecutingAssembly().Location', body)
        self.assertIn('new System.Diagnostics.ProcessStartInfo(exe, ', body)   # the path is the file name, never parsed
        self.assertIn('"\\"" + exe + "\\" --after-update"', body)            # quoted for mono
        self.assertNotIn('cmd.exe', body); self.assertNotIn('powershell', body.lower())
        h = '\n'.join(all_src()[f] for f in ('MainForm.Hero.cs', 'HeroAssets.cs', 'LauncherMode.cs'))
        self.assertNotRegex(h, r'GetFiles\([^)]*,\s*"[^"]*[\[*?]')
        # SelfTest and RealExe below run from "HERO-VR [0.0.62] (1) Ünïcode" and "HERO VR [test] (1)"

    def test_placeholder_repo_and_self_update_from_builds_6_to_9(self):
        v = src('ModVersion.cs')
        self.assertGreaterEqual(int(re.search(r'public const int Build = (\d+);', v).group(1)), 10)   # 10, 11 (0.0.70)
        # builds 6-9 update when version.json's launcher_build is above theirs; the exe keeps its name and is one file
        self.assertIn('-out:', (LAUNCHER/'build.sh').read_text())
        self.assertIn('m.LauncherBuild > LauncherInfo.Build', src('MainForm.Updates.cs'))
        self.assertIn('placeholder', (base('Settings.cs') or '') + (base('GitHub.cs') or '') + (base('MainForm.Updates.cs') or '') + ''.join(all_src().values()))

    def test_nexus_layout_one_exe(self):
        # The Nexus and release zips carry the launcher as one exe: everything the new design needs is embedded.
        for f in ('HeroAssets.cs',):
            t = all_src()[f]
            self.assertIn('GetManifestResourceStream', t)
            self.assertNotRegex(t, r'BaseDirectory[^;]*(orb|wav|ttf)')

    def test_runs_without_admin(self):
        self.assertIn('level="asInvoker"', (LAUNCHER/'app.manifest').read_text())
        a = src('HeroAssets.cs')
        self.assertIn('GetTempPath', a)       # fonts fall back to %TEMP% when the settings folder is read-only
        self.assertIn('ProbeWrite', a)
        self.assertIn('Settings.Folder', src('LauncherMode.cs'))   # the marker lives in the per-user settings folder
        self.assertNotIn('Registry', '\n'.join(all_src()[f] for f in ('MainForm.Hero.cs', 'HeroAssets.cs', 'LauncherMode.cs', 'WaveAudio.cs')))

    def test_locales_dpi_screens_covered_by_self_test(self):
        t = (ROOT/'tests/launcher_hero/HeroSelfTest.cs').read_text(encoding='utf-8')
        for c in ('de-DE', 'tr-TR', 'ar-SA', 'ja-JP'): self.assertIn(c, t)
        for size in ('1280, 720', '1920, 1080', '3840, 2160'): self.assertIn(size, t)
        self.assertIn('100, 125, 150, 175, 200', t)

@unittest.skipUnless(HAVE_MONO, 'needs mono (mcs) and the .NET 4.8 reference assemblies')
class SelfTest(unittest.TestCase):
    """Compiles HeroSelfTest.cs with the launcher sources and runs it in a portable folder whose path has brackets,
    spaces and non-ASCII letters (the Nexus folder names), in five cultures."""
    def test_self_test(self):
        d = pathlib.Path(tempfile.mkdtemp()) / 'HERO-VR [0.0.62] (1) Ünïcode'
        d.mkdir(parents=True)
        (d/'launcher.ini').write_text('Portable=1\n')
        exe = d/'herotest.exe'
        res = []
        for f in RES: res.append('-resource:%s,hero.%s' % (LAUNCHER/'assets/hero'/f, f))
        refs = ['-r:%s/%s.dll' % (API, n) for n in ('mscorlib', 'System', 'System.Core', 'System.Drawing', 'System.Windows.Forms',
                'System.IO.Compression', 'System.IO.Compression.FileSystem', 'System.Web.Extensions', 'System.Management')]
        cmd = ['mcs', '-nostdlib', '-noconfig', '-lib:' + API] + refs + ['-langversion:5', '-nowarn:414,169,649', '-target:exe',
               '-main:Sm2VrLauncher.HeroSelfTest', '-out:' + str(exe)] + res + [str(p) for p in sorted(SRC.glob('*.cs'))] + [str(ROOT/'tests/launcher_hero/HeroSelfTest.cs')]
        r = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        r = subprocess.run(['mono', str(exe)], capture_output=True, text=True, cwd=str(d), timeout=300)
        print(r.stdout[-3000:])
        self.assertEqual(r.returncode, 0, '\n'.join(l for l in r.stdout.splitlines() if l.startswith('FAIL')) + r.stderr[-2000:])
        self.assertIn('ALL PASS', r.stdout)
        self.assertGreater(r.stdout.count('PASS '), 300)

def xvfb():
    return os.environ.get('HERO_TEST_DISPLAY') or (os.environ.get('DISPLAY') if os.environ.get('DISPLAY') else None)

@unittest.skipUnless(HAVE_MONO and xvfb() and (LAUNCHER/'out/SM2VRLauncher.exe').is_file(), 'needs a built launcher, mono and an X display (HERO_TEST_DISPLAY)')
class RealExe(unittest.TestCase):
    """The real launcher under Mono: each injected failure opens the classic design and says why (once)."""
    def run_exe(self, d, fail=None, args=(), wait=14):
        env = dict(os.environ, DISPLAY=xvfb(), SM2VR_LAUNCHER_PREVIEW='1', SM2VR_LAUNCHER_SCREENSHOT='wait')
        env.pop('SM2VR_LAUNCHER_TEST_HERO_FAIL', None)
        if fail: env['SM2VR_LAUNCHER_TEST_HERO_FAIL'] = fail
        p = subprocess.run(['timeout', str(wait), 'mono', str(d/'SM2VRLauncher.exe')] + list(args), env=env, cwd=str(d), capture_output=True, text=True)
        log = d/'launcher-data/logs/launcher-design.log'
        return p.returncode, (log.read_text() if log.exists() else '')

    def fresh(self, design='hero'):
        d = pathlib.Path(tempfile.mkdtemp()) / 'HERO VR [test] (1)'
        (d/'launcher-data').mkdir(parents=True)
        shutil.copy(LAUNCHER/'out/SM2VRLauncher.exe', d)
        (d/'launcher.ini').write_text('Portable=1\n')
        (d/'launcher-data/settings.ini').write_text('LauncherDesign=%s\n' % design)
        return d

    def settings(self, d):
        return dict(l.split('=', 1) for l in (d/'launcher-data/settings.ini').read_text().splitlines() if '=' in l and not l.startswith('#'))

    def test_starts_and_clears_marker(self):
        d = self.fresh(); rc, log = self.run_exe(d)
        self.assertIn('started', log); self.assertNotIn('fallback', log)
        self.assertFalse((d/'launcher-data/hero-starting.marker').exists())
        self.assertEqual(self.settings(d).get('LauncherDesign'), 'hero')

    def test_injected_failures_fall_back(self):
        for fail, words in (('orb', 'orb'), ('audio', 'launcher sound'), ('fonts', 'launcher font'),
                            ('paint', 'could not draw'), ('crash', 'failed while running')):
            d = self.fresh(); rc, log = self.run_exe(d, fail)
            self.assertIn('fallback to classic', log, fail)
            s = self.settings(d)
            self.assertEqual(s.get('LauncherDesign'), 'classic', fail)
            self.assertIn(words, s.get('HeroFallbackReason', '') + log, fail)
            # the reason stays saved until the player presses OK on the notice (preview mode skips the startup
            # notices; HeroSelfTest checks NoticeShown), and the next start stays classic
            rc, log2 = self.run_exe(d)
            self.assertNotIn('opening the new design', log2[len(log):], fail)
            self.assertIn('opening the classic design', log2[len(log):], fail)

    def test_hang_before_first_frame(self):
        d = self.fresh(); rc, log = self.run_exe(d, 'hang')
        self.assertTrue((d/'launcher-data/hero-starting.marker').exists())
        rc, log = self.run_exe(d)
        self.assertIn('did not finish starting', log)
        self.assertEqual(self.settings(d).get('LauncherDesign'), 'classic')

    def test_classic_switch(self):
        d = self.fresh(); rc, log = self.run_exe(d, args=['--classic'])
        self.assertNotIn('opening the new design', log)
        self.assertEqual(self.settings(d).get('LauncherDesign'), 'hero')

if __name__ == '__main__':
    unittest.main()
