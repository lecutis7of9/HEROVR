# HERO VR (GPLv3). Made by Agent Mulder.
"""0.0.75 SM1 (owner: "the launcher can also launch iilyn's VR mod with my add-ons ... just don't touch his
performance stuff ... a Spider-Man 1 and Spider-Man 2 option clearly separated and easy to pick"): the game picker,
the Spider-Man 1 page and play path, his release download (his GitHub, unchanged), the per-game server list, the
setting subtexts, and the names rules."""
import json, os, pathlib, re, shutil, subprocess, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT/'tools/launcher/src'
API = os.environ.get('NETFX_API', '/usr/lib/mono/4.8-api')
HAVE_MONO = shutil.which('mcs') and shutil.which('mono') and os.path.isfile(API + '/mscorlib.dll')
def src(n): return (SRC/n).read_text(encoding='utf-8')
NEW = [SRC/'Sm1.cs', SRC/'MainForm.Sm1.cs', ROOT/'tests/sm1_launcher/Sm1SelfTest.cs', ROOT/'include/spidy/sm1_addon.hpp',
       ROOT/'src/sm1/sm1_core.cpp', ROOT/'src/sm1/sm1_addon_dll.cpp', ROOT/'src/sm1/CMakeLists.txt', ROOT/'tests/sm1_addon_tests.cpp',
       ROOT/'docs/SM1-ADDON-0.0.75.md', ROOT/'src/sm1/sm1_layer.cpp', ROOT/'src/sm1/herovr_sm1_layer.json']


class Picker(unittest.TestCase):
    def test_two_games_clearly_apart(self):
        s = src('MainForm.Sm1.cs')
        self.assertIn('"SPIDER-MAN 2", "SPIDER-MAN 1"', s)
        self.assertIn('{ "HERO VR", Sm1.Title }', s)
        self.assertIn('"iilyn\'s Spider-Man VR mod, with Mulder\'s Heroes add-on"', src('Sm1.cs'))
        self.assertIn('Pick a game: switches at once, no restart needed.', s)   # the picker's own subtext

    def test_picker_in_both_designs(self):
        m, h = src('MainForm.cs'), src('MainForm.Hero.cs')
        self.assertIn('BuildGamePicker();', m)        # classic
        self.assertIn('BuildGamePicker();', h)        # new design, after HeroFinishBuild parks the classic chrome
        self.assertLess(h.index('classicChrome.Controls.Add(c)'), h.index('BuildGamePicker();'))
        self.assertIn('Sm1HeroTick();', h)
        self.assertIn('Sm1LayoutPicker();', h)

    def test_sheets_line_up(self):
        m, h = src('MainForm.cs'), src('MainForm.Hero.cs')
        self.assertIn('const int SheetSm1 = 6', src('MainForm.Sm1.cs'))
        self.assertIn('Theme.R(36, 196, 1108, 436),   // 0.0.75 SM1', m)
        home = re.search(r'var home = new\[\] \{(.*?)\};', h).group(1)
        self.assertEqual(home.count('Theme.R(') + home.count('MultiplayerBounds()'), 6)
        self.assertIn('Array.Resize(ref home, 7); home[6] = Theme.R(36, 196, 1108, 436);', h)   # the 7th: SM1

    def test_spider_man_2_unchanged_when_picked(self):
        m = src('MainForm.cs')
        self.assertIn('if (Sm1Selected && job == Job.None) { Sm1Play(); return; }', m)
        self.assertIn('if (Sm1Selected) { Sm1PlayFlat(); return; }', m)
        self.assertIn('if (Sm1Selected) { Sm1State(); return; }', m)
        self.assertIn('if (Sm1Selected) tabs.Selected = SheetSm1;', m)
        self.assertIn('Sm1.KeyGame, LobbyGame.Sm2)', src('MainForm.Sm1.cs'))   # default: Spider-Man 2


class HisMod(unittest.TestCase):
    def test_his_github_only(self):
        s = src('Sm1.cs')
        self.assertIn('https://github.com/IlyaMez/SpidyVR', s)
        self.assertIn('https://api.github.com/repos/IlyaMez/SpidyVR/releases/latest', s)
        self.assertIn('browser_download_url', s)   # the download counts on his release
        self.assertIn('Sha256', s)

    def test_never_changes_his_mod(self):
        s, f = src('Sm1.cs'), src('MainForm.Sm1.cs')
        self.assertNotIn('%APPDATA%\\\\Spidy', s)
        self.assertNotRegex(s + f, r'Path\.Combine\(\s*(sm1Mod|target|DefaultModFolder)\s*,\s*"[^"]*\.(ini|json|cfg)"')
        self.assertIn('WorkingDirectory = sm1Mod, UseShellExecute = true', f)   # his launcher, his settings
        self.assertIn('Path.Combine(DataFolder, "addon")', s)                   # our add-on is staged in our folder

    def test_credit(self):
        c = src('MainForm.Credits.cs')
        self.assertIn('iilyn (Ilya Mezerowsky)', c)
        for k in ('Sm1.RepoUrl', 'Sm1.KofiUrl', 'Sm1.DiscordUrl'): self.assertIn(k, c)

    def test_popups_no_or_autosetup(self):
        f = src('MainForm.Sm1.cs'); d = src('AutosetupDialog.cs')
        self.assertIn('"Yes, Autosetup"', d); self.assertIn('Text = "No"', d)
        self.assertIn('new AutosetupDialog("Spider-Man 1 setup", text)', f)
        for t in ('GameMissingText', 'ModMissingText', 'AddonMissingText'): self.assertIn('Sm1.' + t, f)

    def test_every_setting_has_restart_subtext(self):
        f = src('MainForm.Sm1.cs')
        rows = re.findall(r'new ToggleRow\("([^"]+)", "([^"]+)"', f)
        self.assertEqual(len(rows), 4)
        for title, sub in rows:
            self.assertRegex(sub, r'takes effect the next time (you press Play|the launcher starts)\.$', title)


class Layer(unittest.TestCase):
    def test_features_match_the_addon(self):
        hpp = (ROOT/'include/spidy/sm1_addon.hpp').read_text(encoding='utf-8')
        native = re.findall(r'\{"([a-z -]+)", Evidence::\w+, State::(\w+)', hpp)
        shown = re.findall(r'new\[\] \{ "([a-z -]+)", "(\w+)"', src('Sm1.cs'))
        want = [(n, s) for n, s in native if n not in ('host check', 'add-on settings')]
        self.assertEqual(shown, want)
        self.assertIn('Sm1.FeatureSummary()', src('MainForm.Sm1.cs'))

    def test_layer_registered_only_while_playing(self):
        s, f = src('Sm1.cs'), src('MainForm.Sm1.cs')
        self.assertIn(r'@"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"', s)
        self.assertIn('Registry.CurrentUser', s)
        self.assertNotIn('Registry.LocalMachine', s)
        self.assertIn('Sm1RegisterLayer();', f)
        self.assertIn('Sm1.RemoveLayersAfterGame(', f)
        self.assertGreaterEqual(f.count('Sm1.RemoveLayers()'), 3)   # failure, add-on off, closing / startup
        m = json.loads((ROOT/'src/sm1/herovr_sm1_layer.json').read_text(encoding='utf-8'))
        self.assertEqual(m['api_layer']['name'], 'XR_APILAYER_HEROVR_sm1_heroes')
        self.assertEqual(m['api_layer']['library_path'], '.\\herovr_sm1_layer.dll')
        self.assertEqual(m['api_layer']['disable_environment'], 'HEROVR_SM1_LAYER_DISABLE')
        pkg = (ROOT/'tools/package.ps1').read_text(encoding='utf-8')
        self.assertIn("'herovr_sm1_layer.dll', 'herovr_sm1_layer.json'", pkg)


class Multiplayer(unittest.TestCase):
    def test_per_game_list(self):
        mp = src('MainForm.Multiplayer.cs')
        self.assertIn('InternetLobby.Merge(hosts, inet, mineIds, MpGame)', mp)
        self.assertIn('if (Sm1MpGuard()) return;', mp)
        self.assertIn('"game", LobbyGame.Sm2', src('InternetLobby.cs'))


class Names(unittest.TestCase):
    def test_no_banned_names(self):
        files = NEW + [SRC/n for n in ('MainForm.cs', 'MainForm.Hero.cs', 'MainForm.Multiplayer.cs', 'MainForm.Credits.cs',
                                        'Lobby.cs', 'InternetLobby.cs', 'AutosetupDialog.cs')] + [ROOT/'tools/package.ps1', ROOT/'tests/sm1_launcher_tests.py']
        for p in files:
            if not p.is_file(): continue
            t = p.read_text(encoding='utf-8')
            self.assertNotIn('jim' + 'jam', t.lower(), p.name)
            self.assertNotIn('CLOUDFLARE_API_' + 'TOKEN', t, p.name)
            # 'miles' only as the game's title in player text, never as a name in code
            self.assertIsNone(re.search(r'\b[A-Za-z_]*[Mm]iles[A-Za-z_0-9]*\s*(=|\(|;)', t), p.name)


@unittest.skipUnless(HAVE_MONO, 'needs mono (mcs) and the .NET 4.8 reference assemblies')
class SelfTest(unittest.TestCase):
    def test_logic(self):
        d = pathlib.Path(tempfile.mkdtemp()) / 'HERO VR [sm1] (1)'
        d.mkdir(parents=True)
        (d/'launcher.ini').write_text('Portable=1\n')
        exe = d/'sm1test.exe'
        refs = ['-r:%s/%s.dll' % (API, n) for n in ('mscorlib', 'System', 'System.Core', 'System.Drawing', 'System.Windows.Forms',
                'System.IO.Compression', 'System.IO.Compression.FileSystem', 'System.Web.Extensions', 'System.Management')]
        res = ['-resource:%s,hero.%s' % (ROOT/'tools/launcher/assets/hero'/f, f) for f in ('orb.bin', 'intro.wav', 'loop.wav', 'Barlow-Regular.ttf', 'Barlow-Medium.ttf')]
        cmd = ['mcs', '-nostdlib', '-noconfig', '-lib:' + API] + refs + ['-langversion:5', '-nowarn:414,169,649,219', '-target:exe',
               '-main:Sm2VrLauncher.Sm1SelfTest', '-out:' + str(exe)] + res + [str(p) for p in sorted(SRC.glob('*.cs'))] + [str(ROOT/'tests/sm1_launcher/Sm1SelfTest.cs')]
        r = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        env = dict(os.environ, XDG_CONFIG_HOME=str(d/'appdata'), MONO_REGISTRY_PATH=str(d/'registry'))
        r = subprocess.run(['mono', str(exe)], capture_output=True, text=True, cwd=str(d), timeout=120, env=env)
        print(r.stdout[-2500:])
        self.assertEqual(r.returncode, 0, r.stdout[-3000:] + r.stderr[-2000:])
        self.assertIn('ALL PASS', r.stdout)
        self.assertFalse((d/'launcher-data'/'SM1'/'outside.txt').exists())


if __name__ == '__main__':
    unittest.main()
