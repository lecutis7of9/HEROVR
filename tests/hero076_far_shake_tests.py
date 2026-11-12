# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
"""0.0.76 FAR SHAKE: the far background shaking while the head turns (run 001842's video).

live.txt, both live (no restart):
  vr_game_jitter = off (default) | keep   0.0.76 JITTER FIX: the game's own head views (never shown) give the lens
                                          0, not the stock camera's stored jitter, which the lens doubled and
                                          flipped; keep: c71a59c's doubled values (hero076_jitter_fix_tests.py)
  vr_jitter = on (default) | off          the eyes' Halton walk, or one fixed tiny offset (TemporalAa still runs)
"""
import pathlib
import re
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as L  # noqa: E402


def src(p):
    return (ROOT / p).read_text(encoding='utf-8')


class FarShake(unittest.TestCase):
    def test_settings_live(self):
        # 79.4 (t2188): vr_jitter is a level (off, low, med, full, e5d2746), SpidyVrJitterLevel, 0 to 4.
        self.assertEqual(L.SETTINGS['vr_jitter'][:2], ((0, 4), 'SpidyVrJitterLevel'))
        self.assertEqual(L.SETTINGS['vr_game_jitter'][:2], ((0, 1), 'SpidyVrGameJitter'))
        for name in ('vr_jitter', 'vr_game_jitter'):
            self.assertNotIn(name, L.RESTART)
            self.assertIn('no restart', L.SETTINGS[name][2])
        # 79.2b (t2169): vr_jitter is the hologram's EYE JITTER now, kept in live.txt; vr_game_jitter is not kept.
        self.assertIn('vr_jitter', L.PERSISTED)
        self.assertNotIn('vr_game_jitter', L.START_ONLY + L.PERSISTED)

    def test_words(self):
        values, errors = L.parse('vr_jitter = off\nvr_game_jitter = keep\n')
        self.assertEqual(errors, [])
        self.assertEqual((L.export_value('vr_jitter', values['vr_jitter']),
                          L.export_value('vr_game_jitter', values['vr_game_jitter'])), (0, 0))
        # 'on' keeps its old meaning (the sample walk: full); the levels and the hidden e5d2746
        for word, number in (('off', 0), ('low', 1), ('med', 2), ('full', 3), ('on', 3), ('e5d2746', 4)):
            values, errors = L.parse(f'vr_jitter = {word}\n')
            self.assertEqual(errors, [], word)
            self.assertEqual(L.export_value('vr_jitter', values['vr_jitter']), number, word)
        self.assertEqual(L.CHOICE_NAMES['vr_jitter'], ('off', 'low', 'med', 'full', 'e5d2746'))
        # 0.0.76 JITTER FIX: off (1) is the default (the DLL's atomic; the line is never persisted).
        self.assertEqual(L.CHOICE_NAMES['vr_game_jitter'], ('keep', 'off'))
        self.assertIn('1 off, the default', L.SETTINGS['vr_game_jitter'][2])

    def test_migration_794(self):
        """A live.txt from 79.2b / 79.3 (on / off, the test default) is set to med once; a later choice stays."""
        import pathlib
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / 'live.txt'
            p.write_text('hud = 1\nvr_jitter = off\n', encoding='utf-8')
            self.assertIn('vr_jitter off set once to med', L.migrate_794_jitter(p))
            self.assertEqual(L.parse(p.read_text())[0]['vr_jitter'], 'med')
            p.write_text(p.read_text().replace('vr_jitter = med', 'vr_jitter = low'), encoding='utf-8')
            self.assertIsNone(L.migrate_794_jitter(p))
            self.assertEqual(L.parse(p.read_text())[0]['vr_jitter'], 'low')
            self.assertIsNone(L.migrate_794_jitter(pathlib.Path(d) / 'none.txt'))
        self.assertIn(L.MIGRATED_794_JITTER, L.HEADER)

    def test_levels_and_check_in_source(self):
        h = src('include/spidy/jitter_check.hpp')
        self.assertIn('inline constexpr float scales[levels] = {0.f, 0.115f, 0.25f, 0.5f, 1.f};', h)
        self.assertIn('inline constexpr uint32_t defaultLevel = med;', h)
        s = src('src/stereo_probe.cpp')
        self.assertIn('std::atomic<uint32_t> eyeJitterLevel{spidy::jitter_check::defaultLevel};', s)
        self.assertIn('walk[0] *= spidy::jitter_check::scales[level];', s)
        self.assertIn('SpidyVrJitterLevel(void* value)', s)
        self.assertIn('jitterCheckRun(static_cast<unsigned>(eye), jitter);', s)
        self.assertIn('jitterCheckOther(job, other, otherAa);', s)
        self.assertEqual(s.count('jitterCheckLens(index, jitter.data(), descriptor.values);'), 2)
        g = src('src/game_xr.cpp')
        self.assertIn('79.4 JITTER CHECK', g)
        self.assertIn('jitter_check::nextLevel(SpidyVrJitterLevelNow())', g)
        w = src('src/wrist_hud.cpp')
        self.assertIn('jitter_check::rowText[', w)

    def test_jitter_check_record(self):
        import struct
        import jitter_check_record as J
        raw = bytearray(1024)
        struct.pack_into('<8I2Q', raw, 0, J.MAGIC, 1, 1024, 3, 2, 1, 0, 0, 1000, 61000)
        for eye in range(2):
            struct.pack_into('<24Q9fI', raw, 48 + 232 * eye, 10, 10, 9, 0, 9, 0, 0, 0, *([0] * 8), 3, *([0] * 7),
                             1e-4, -1e-4, 2e-4, 2e-4, 2e-4, -2e-4, 1e-4, 2e-4, 0, 0)
        struct.pack_into('<2Q2f2I', raw, 48 + 464, 0x1234, 50, 5e-4, -1e-4, 1, 0)
        struct.pack_into('<Q2I', raw, 48 + 464 + 256, 1000, 2, 0)
        struct.pack_into('<Q2I', raw, 48 + 464 + 256 + 16, 31000, 3, 2)
        r = J.jitter_check_parse(bytes(raw), 100.0)
        self.assertEqual((r['level'], r['max_stored_px']), ('full', 0.44))
        self.assertEqual(r['left']['share']['job_equals_lens_stored'], 100.0)
        self.assertEqual(r['left']['match_lag_frames'], {8: 3})
        self.assertEqual([c['level'] for c in r['changes']], ['med', 'full'])
        self.assertEqual(r['changes'][1]['session_seconds'], 70.0)
        self.assertEqual(r['other_jobs'][0]['runs'], 50)
        self.assertIn('79.4 JITTER CHECK', J.jitter_check_line(r))
        self.assertIsNone(J.jitter_check_parse(bytes(1023)))
        rr = src('tools/run_game_vr.py')
        self.assertIn('jitter_check=jitter_check_record.jitter_check_snapshot', rr)
        self.assertIn('live_settings.migrate_794_jitter(ROOT/\'live.txt\')', rr)

    def test_native(self):
        s = src('src/stereo_probe.cpp')
        for export in ('SpidyVrJitter', 'SpidyVrGameJitter'):
            self.assertIn(f'DWORD WINAPI {export}(void* value)', s)
        # The stock jitter reaches the head views only through gameViewJitter (activeView, placeEyes onHead).
        self.assertEqual(len(re.findall(r'gameViewJitter\(noGameJitter, (?:stock|main)'
                                        r'\.values\[0x42[8c] / 4\]\)', s)), 4)
        self.assertNotIn('stock.values[0x428 / 4], stock.values[0x42c / 4]', s)
        # (The game-screen 3D mode's eyes still take the stock jitter; that mode is not the headset's.)
        self.assertNotIn('main.values[0x428 / 4], main.values[0x42c / 4], false', s)
        self.assertEqual(s.count('eyeJitterFor(eyeJitterOff.load'), 2)
        # 79.2b (t2168/t2169): vr_jitter OFF is the default (the halved walk did not stop the far shake).
        self.assertIn('std::atomic<uint32_t> eyeJitterOff{0}, gameViewJitterOff{1};', s)  # 79.4: MED by default
        h = src('include/spidy/sm1style.hpp')
        self.assertIn('inline constexpr float steadyJitter = 1e-6f;', h)
        self.assertIn('return off ? 0.f : stock;', h)

    def test_eye_history_check(self):
        """The outside advice (each eye its own TAA history) is measured each run and reaches the report."""
        s = src('src/stereo_probe.cpp')
        self.assertIn('mt::ownHistory(eyeSet, previous, mainSet, otherSet)', s)
        self.assertIn('++dd.sharedHistory[eye];', s)
        r = src('tools/run_game_vr.py')
        self.assertIn("'SpidySm1StyleData'", r)
        self.assertIn("sm1style_eyes=sm1style_snapshot(game, xr['SpidySm1StyleData']) or sm1style_last", r)
        self.assertIn('from sm1style_record import sm1style_snapshot, sm1style_line', r)
        import sm1style_record as P

        class Fake:
            def __init__(self, raw):
                self.raw = raw

            def read(self, address, size):
                return self.raw[:size]
        import struct
        raw = bytearray(256)
        struct.pack_into('<4I', raw, 0, 0x53314d53, 2, 256, 0)
        struct.pack_into('<2Q', raw, 32, 10, 11)    # calls
        struct.pack_into('<2Q', raw, 48, 9, 10)     # ran
        struct.pack_into('<2Q', raw, 64, 8, 9)      # history
        struct.pack_into('<6Q', raw, 200, 9, 10, 0, 0, 0, 1)
        rec = P.sm1style_snapshot(Fake(bytes(raw)), 0)
        self.assertEqual(rec['left'], dict(calls=10, ran=9, history=8, own_history=9, shared_history=0, job_changed=0, aa_writes=0, queued=0))
        self.assertEqual(rec['right']['job_changed'], 1)
        self.assertIn('the eyes never read each other', P.sm1style_line(rec))
        struct.pack_into('<Q', raw, 216, 3)
        self.assertIn('SHARED', P.sm1style_line(P.sm1style_snapshot(Fake(bytes(raw)), 0)))
        self.assertIsNone(P.sm1style_snapshot(Fake(bytes(255)), 0))
        # 79.3 (t2172): version 3 - every TemporalAa call and the eye runs found by the eye job table.
        struct.pack_into('<4I', raw, 0, 0x53314d53, 3, 256, 0)
        struct.pack_into('<2I', raw, 248, 30, 21)
        rec = P.sm1style_snapshot(Fake(bytes(raw)), 0)
        self.assertEqual((rec['all_calls'], rec['eye_by_table']), (30, 21))
        self.assertIn('TemporalAa calls in all 30', P.sm1style_line(rec))
        self.assertIn('jobEye(reinterpret_cast<void*>(job), nullptr, nullptr)', s)

    def test_docs(self):
        for doc in ('CHANGELOG.md', 'README-HERO-VR.md', 'BUILD-SM2.md', 'docs/VISUAL-0.0.76.md'):
            text = src(doc)
            self.assertIn('vr_game_jitter', text, doc)


if __name__ == '__main__':
    unittest.main()
