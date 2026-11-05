"""HERO VR 0.0.78 cut18 UI DUMP (diagnostic): the ui_dump switch (5645), its hooks in the GPU path and the VR loop, the
PNG writer's folder and the collector."""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class Cut18(unittest.TestCase):
    def test_switch(self):
        g = src('src/game_xr.cpp')
        self.assertIn('WINAPI SpidyUiDump(void* v)', g)
        self.assertEqual(len(re.findall(r'\b5645\b', g)), 1)
        key = 'ui_dump'
        self.assertEqual(live_settings.SETTINGS[key][0], (0, 1))
        self.assertEqual(live_settings.SETTINGS[key][1], 'SpidyUiDump')
        self.assertTrue(live_settings.SETTINGS[key][2].endswith('- live, no restart needed'))
        self.assertEqual(live_settings.CHOICES[key]['on'], 0)
        self.assertEqual(live_settings.CHOICES[key]['off'], 1)
        self.assertIn(key, live_settings.START_ONLY)
        self.assertIn('live.txt ui_dump = off', src('apps/launcher/ui.cpp'))

    def test_gpu_hunt(self):
        n = src('src/native_eye_gpu.cpp')
        self.assertIn('watchUi(count, bs); // cut18 UI DUMP: read-only', n)
        self.assertIn('uiDumpStep(swapchain); // cut18 UI DUMP diagnostic', n)
        # only readable states are copied, the back buffer from PRESENT, and the wait is outside the locks
        self.assertIn('ui_dump::readableState(t.state)', n)
        body = n[n.index('void uiDump(IDXGISwapChain*'):n.index('void uiDumpStep(')]
        self.assertLess(body.index('mine = slot->fence;'), body.index('WaitForSingleObject(done, 2000)'))
        h = src('include/spidy/native_eye_gpu.hpp')
        self.assertIn('void requestUiDump(uint32_t reason);', h)
        self.assertIn('bool takeUiDump(', h)

    def test_triggers_and_writer(self):
        g = src('src/game_xr.cpp')
        self.assertEqual(g.count('uiDumpAsk.fetch_or(1u << ui_dump::reasonBossGate)'), 2)
        self.assertEqual(g.count('uiDumpAsk.fetch_or(1u << ui_dump::reasonPrompt)'), 1)
        self.assertIn('native_gpu::requestUiDump(r);', g)
        self.assertIn('L"\\\\Spidy\\\\ui-dump"', g)
        self.assertIn('GUID_ContainerFormatPng', g)

    def test_collector(self):
        c = src('tools/collect-reports.ps1')
        self.assertIn("Join-Path $env:APPDATA 'Spidy\\ui-dump'", c)
        self.assertIn("$_.Name -like 'ui-*'", c)


if __name__ == '__main__':
    unittest.main()
