"""Version 0.0.70: no hidden stereo pair at a scene's start (spidyvr-perf-compare #1).

A scene's two eye views are shown only with the HUD's panel on their screen, which needs the HUD panel's placement
hook. The SM2 port does not hook it (SM2_STEREO_HUD 0), so asking for them only made the game render two full-size
eye views nobody saw for about 1-1.25 s at every cutscene start or resume. The ask is now gated on the hook being
installed; the flat screen stays as it was. GAME SCREEN 3D's subtexts say that cutscenes stay flat in SM2 for now.
The behaviour itself is tested in core_tests.cpp (sceneStereoAsked, ScenePanel)."""
import pathlib, re, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
read = lambda p: (ROOT / p).read_text(encoding='utf-8')


class SceneStereoGate(unittest.TestCase):
    def test_pure_gate(self):
        s = read('include/spidy/presentation_gate.hpp')
        m = re.search(r'inline bool sceneStereoAsked\(bool scene, bool depthOn, bool hudPanelHooked\) \{\s*'
                      r'return scene && depthOn && hudPanelHooked;\s*\}', s)
        self.assertTrue(m, 'sceneStereoAsked must need the scene, the depth and the hook')

    def test_game_xr_asks_only_with_the_hook(self):
        s = read('src/game_xr.cpp')
        calls = re.findall(r'scenePanel\.wanted\((.*?)GetTickCount64\(\)\);', s, re.S)
        self.assertEqual(len(calls), 1, 'one scene request')
        c = re.sub(r'\s+', ' ', calls[0])
        self.assertIn('sceneStereoAsked(', c)
        self.assertIn('native_hud::panelHooked()', c)
        self.assertIn('values.screenDepth > 0', c)
        # the stereo command is sent only inside the scene branch (!vrPlay after `if (!vrPlay && !scene) return`)
        self.assertIn('if (!vrPlay && !scene)', s)

    def test_hook_flag_follows_the_real_hook(self):
        p = read('src/stereo_probe.cpp')
        sets = re.findall(r'native_hud::panelHooked\(([^;]*)\);', p)
        self.assertIn('hudPlaceHook != nullptr', sets, 'set from the hook when the hooks are enabled')
        self.assertIn('false', sets, 'cleared when they stop')
        self.assertEqual([x for x in sets if x not in ('hudPlaceHook != nullptr', 'false')], [])
        # SM2: never hooked, at compile time (and it refuses to build with the Remastered values)
        self.assertRegex(p, r'#define SM2_STEREO_HUD 0')
        self.assertIn('hudPlaceHook = SM2_STEREO_HUD ?', p)
        h = read('src/native_hud.cpp')
        self.assertRegex(h, r'std::atomic<bool> hudPanelHooked\{\};')

    def test_subtexts_are_honest(self):
        # no switch that silently does nothing: every place GAME SCREEN 3D is offered says cutscenes stay flat in SM2
        for path, needle in (('src/vr_settings.cpp', 'GAME SCREEN 3D'),
                             ('tools/launcher/src/LaunchOptions.cs', '"ScreenDepth"'),
                             ('apps/launcher/ui.cpp', 'option("Game screen 3D"'),
                             ('tools/run_game_vr.py', "'--screen-depth'")):
            s = read(path)
            i = s.index(needle)
            self.assertIn('stay flat for now', s[i:i + 400], path)


if __name__ == '__main__':
    unittest.main()
