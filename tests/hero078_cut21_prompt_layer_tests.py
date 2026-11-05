"""HERO VR 0.0.78 cut21 PROMPT LAYER (CANDIDATE): the prompt_layer switch (5646), the UI layer's find and readback in
the GPU path, the small keyed cards in the renderer and their wiring in the VR loop - never the whole layer."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cut21(unittest.TestCase):
    def test_switch(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyPromptLayer(void* v)', g)
        self.assertEqual(len(re.findall(r'\b5646\b', g)), 1)
        key = 'prompt_layer'
        self.assertEqual(live_settings.SETTINGS[key][0], (0, 1))
        self.assertEqual(live_settings.SETTINGS[key][1], 'SpidyPromptLayer')
        self.assertTrue(live_settings.SETTINGS[key][2].endswith('- live, no restart needed'))
        self.assertEqual(live_settings.CHOICES[key]['on'], 0)
        self.assertEqual(live_settings.CHOICES[key]['off'], 1)
        self.assertIn('live.txt prompt_layer = off', src('apps/launcher/ui.cpp'))
        self.assertIn('promptLayerNow{prompt_layer::layerOn}', g)

    def test_gpu(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('prompt_layer::isLayer(', n)
        self.assertIn('promptLayerStep(', n)
        cap = n[n.index('void captureLayer('):]
        self.assertIn('D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE', cap[:cap.index('++layerSkipState')])
        h = src('include/spidy/native_eye_gpu.hpp')
        for d in ('void wantPromptLayer(bool', 'bool latestPromptLayer(', 'std::string promptLayerLine('):
            self.assertIn(d, h)

    def test_no_stall(self):
        # The layer's find, check, copy and alpha read-back never wait on the GPU, and never use the UI DUMP's read-back.
        n = src('src/native_eye_gpu.cpp')
        start = n.index('void dropLayer(uint64_t now)')
        layer = n[start:n.index('void promptLayerStep(', start)]
        self.assertNotIn('uiDump(', layer)
        self.assertNotIn('WaitForSingleObject', layer)
        self.assertNotIn('SetEventOnCompletion', layer)
        self.assertIn('prompt_layer::candidate(', layer)
        self.assertIn('void checkCandidate(', layer)
        self.assertIn('if (!layerVerified)', layer)
        self.assertIn('!layerVerified || !layerShot.texture', n)

    def test_ui_dump_off_by_default(self):
        self.assertIn('uiDumpNow{ui_dump::dumpOff}', src('src/game_xr.cpp'))
        self.assertIn('1 off, the default since cut21', live_settings.SETTINGS['ui_dump'][2])
        self.assertIn('Off by default, as each save can hitch the game up to 2 s', src('apps/launcher/ui.cpp'))

    def test_cards_small_and_keyed(self):
        r = src('src/d3d12_renderer.cpp')
        self.assertIn('unsigned D3D12Renderer::recordCards(', r)
        self.assertIn('keyed == 2', r.replace('keyed==2', 'keyed == 2'))
        self.assertIn('static constexpr unsigned maxCards = 8', src('include/spidy/d3d12_renderer.hpp'))
        p = src('include/spidy/prompt_layer.hpp')
        self.assertIn('Chebyshev', p)

    def test_vr_loop(self):
        g = src('src/game_xr.cpp')
        self.assertIn('native_gpu::wantPromptLayer(promptLayerFrame);', g)
        block = g[g.index('cut21 PROMPT LAYER (CANDIDATE, prompt_layer.hpp)'):]
        block = block[:block.index('const uint64_t promptNowMs')]
        self.assertIn('!saved->flatScreen && !saved->stereoScreen && !showScreen', block)
        self.assertIn('prompt_layer::regions(', block)
        self.assertIn('prompt_layer::cardRect(', block)
        self.assertIn('overlay.setCards(', block)
        self.assertIn('overlay.clearCards();', block)
        self.assertNotIn('setInset', block)  # never the whole layer
        self.assertEqual(g.count('overlay.clearCards();'), 2)

    def test_strings(self):
        for p in ('include/spidy/prompt_layer.hpp', 'tests/cut21_tests.cpp',
                  'tests/hero078_cut21_quiet_trim_tests.py'):
            t = src(p).lower()
            self.assertNotIn('jim' + 'jam', t)
            self.assertNotIn('mi' + 'les', t)


if __name__ == '__main__':
    unittest.main()
