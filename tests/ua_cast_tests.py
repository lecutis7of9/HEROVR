"""ULTIMATE ALLIANCE EXPANSION CHARACTERS (ua_cast.hpp, tools/ua/ua_cast.py): the Stark Tower hub's people from the
player's OWN MUA1 PS2 disc - their models posed in the map's idle performances (cast.txt + Cast/scene.glb), their
conversations (talk.txt) and recorded lines (Voice/) - drawn in the room and talked to (look or point, A). Synthetic
fixtures only: nothing of the game is in git. Made by Agent Mulder."""
import json, os, struct, sys, tempfile, unittest, wave, io
from pathlib import Path
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ua'))
import ua_cast  # noqa: E402
def src(p): return open(os.path.join(ROOT, p), encoding='utf-8').read()
try:
    import numpy  # noqa: F401
    from PIL import Image
    HAVE_NP = True
except ImportError:  # pragma: no cover
    HAVE_NP = False

CONV = """<conversation>
  <startCondition conditionscriptfile="common/conv_run_once/run_once07">
    <participant name="default"><line text="%NICKFURY%: Story only." /></participant>
  </startCondition>
  <startCondition conditionscriptfile="common/conv/wolverine" runonce="true">
    <participant name="default"><line soundtoplay="voice/fury/1_x_010_nf" text="%NICKFURY%: Logan." /></participant>
  </startCondition>
  <startCondition conditionscriptfile="act1/stark/xor_fury_convs" runonce="true">
    <participant name="default">
      <line soundtoplay="voice/fury/1_x_020_nf" text="%NICKFURY%: Welcome aboard.">
        <response text="%PLAYER%: Glad to be here." tagjump="menu" />
      </line>
    </participant>
  </startCondition>
  <startCondition>
    <participant name="default">
      <line tagindex="menu" text="%PLAYER%">
        <response text="What is this place?" chosenscriptfile="act1/stark/set_var" >
          <line text="%NICKFURY%: Stark Tower."><response text="%BLANK%" tagjump="menu" /></line>
        </response>
        <response text="Secret." scriptfile="act1/stark/needs_var" tagjump="menu" />
        <response text="Take us to the Omega Base.">
          <line text="%Menu%: Are you sure you wish to start the Omega Base mission?">
            <response text="%YES%" chosenscriptfile="act1/stark/load_omega1" conversationend="true" />
            <response text="%NO%" tagjump="menu" />
          </line>
        </response>
        <response text="Logan only." scriptfile="common/conv/wolverine" conversationend="true" />
        <response text="Goodbye." tagjump="nowhere" />
      </line>
    </participant>
  </startCondition>
</conversation>
"""


def pkg_fixture(d):
    pkg = Path(d)
    (pkg / 'conversations' / 'act1' / 'stark').mkdir(parents=True)
    (pkg / 'conversations' / 'act1' / 'stark' / '1_x.engb.xml').write_text(CONV)
    sc = pkg / 'scripts' / 'act1' / 'stark'
    sc.mkdir(parents=True)
    (sc / 'xor_fury_convs.py').write_text('run_default = isActorInList("_ACTIVATOR_", "wolverine ironman" )\n'
                                          'if run_default == 0\n     allowConversation("TRUE" )\nendif\n')
    (sc / 'set_var.py').write_text("setGameFlag('conv_vars', 2, 1)\n")
    (sc / 'needs_var.py').write_text("disallowResponseOnVar('conv_vars', 2)\n")
    (sc / 'load_omega1.py').write_text("loadZone('omega1')\n")
    return pkg


def vag_block(shift, pred, flags, nibbles):
    data = bytearray([(pred << 4) | shift, flags])
    for k in range(0, 28, 2):
        data.append((nibbles[k] & 15) | ((nibbles[k + 1] & 15) << 4))
    return bytes(data)


class Conversations(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.pkg = pkg_fixture(self.tmp.name)
        # the response scriptfile is read as written in the file (as the game's): the needs test reads the attribute
        self.talk = ua_cast.read_talk(self.pkg / 'conversations' / 'act1' / 'stark' / '1_x.engb.xml', self.pkg)

    def tearDown(self):
        self.tmp.cleanup()

    def test_starts_their_conditions_and_run_once(self):
        starts = self.talk['starts']
        self.assertEqual([c for _, _, c in starts], ['never', 'hero:wolverine', 'not:wolverine,ironman', 'always'])
        self.assertEqual([o for _, o, _ in starts], [False, True, True, False])

    def test_lines_responses_and_jumps(self):
        lines, resps = self.talk['lines'], self.talk['resps']
        welcome = next(i for i, l in enumerate(lines) if l['text'] == 'Welcome aboard.')
        menu = next(i for i, l in enumerate(lines) if l['speaker'] == 'PLAYER' and not l['text'])
        self.assertEqual(lines[welcome]['sound'], 'VOICE/FURY/1_X_020_NF')
        glad = next(r for r in resps if r['line'] == welcome)
        self.assertEqual((glad['to'], glad['text']), (menu, '%PLAYER%: Glad to be here.'))
        bye = next(r for r in resps if r['text'] == 'Goodbye.')
        self.assertTrue(bye['end'] and bye['to'] == -1, 'a jump to no tag ends the talk')
        yes = next(r for r in resps if r['text'] == '%YES%')
        self.assertEqual((yes['action'], yes['end']), ('missions', True), 'load_* is a mission: COMING SOON')
        q = next(r for r in resps if r['text'] == 'What is this place?')
        self.assertEqual(q['action'], 'flag:2', "the chosen script's setGameFlag('conv_vars', 2, 1)")
        secret = next(r for r in resps if r['text'] == 'Secret.')
        self.assertEqual((secret['needs'], secret['cond']), (2, 'always'), 'a follow-up: shown once flag 2 is set (disallowResponseOnVar)')
        logan = next(r for r in resps if r['text'] == 'Logan only.')
        self.assertEqual(logan['cond'], 'hero:wolverine', "a hero's own reply")
        confirm = next(i for i, l in enumerate(lines) if l['text'].startswith('Are you sure'))
        self.assertEqual(lines[confirm]['speaker'], 'MENU')

    def test_talk_txt_round_trip_and_opening(self):
        out = Path(self.tmp.name) / 'talk.txt'
        talks = {'1_x': self.talk}
        ua_cast.write_talk(out, talks, ua_cast.speaker_names(talks, {}), {'VOICE/FURY/1_X_020_NF': 0}, {0: 2.5})
        text = out.read_text()
        self.assertTrue(text.startswith(ua_cast.TALK_HEADER + '\n'))
        speakers, convs = ua_cast.read_talk_txt(out)
        self.assertEqual(speakers['PLAYER'], ua_cast.PLAYER_NAME)
        conv = convs['1_x']
        _, line = ua_cast.opening(conv)
        self.assertEqual(conv['lines'][line][1:], (0, 2.5, 'Welcome aboard.'), 'Spider-Man: the welcome, voiced')
        _, line = ua_cast.opening(conv, 'wolverine')
        self.assertEqual(conv['lines'][line][3], 'Logan.')
        tok, voice, secs, words = conv['lines'][line]
        self.assertEqual(voice, -1, 'a clip not in the bank: subtitles only')
        self.assertAlmostEqual(secs, min(9, 1.2 + .055 * len('Logan.')), places=2)
        self.assertIn('resp 2 3 0 none 0 always %PLAYER%: Glad to be here.', text)
        self.assertIn(' hero:wolverine Logan only.', text)
        resps = conv['resps']
        self.assertTrue(all(len(r) == 7 for r in resps))
        self.assertNotIn('\r', text)


class Voices(unittest.TestCase):
    @unittest.skipUnless(HAVE_NP, 'numpy')
    def test_ps_adpcm(self):
        nib = [1, 2, -1, 0] * 7
        body = vag_block(12, 0, 0, nib) + vag_block(12, 1, 0, [0] * 28) + vag_block(0, 0, 7, [0] * 28)
        rate, pcm = ua_cast.decode_vag(body)
        s = list(struct.unpack(f'<{len(pcm) // 2}h', pcm))
        self.assertEqual(rate, 22050)
        self.assertEqual(s[:4], [1, 2, -1, 0], 'shift 12, no prediction: the nibbles')
        self.assertEqual(len(s), 56, 'stops at the end flag')
        self.assertEqual(s[28], (0 + (s[27] * 60) >> 6) if s[27] >= 0 else s[28], 'filter 1 predicts from the last')
        head = b'VAGp' + struct.pack('>III', 32, 0, len(body)) + struct.pack('>I', 11025) + bytes(28)
        rate, pcm2 = ua_cast.decode_vag(head + body)
        self.assertEqual((rate, pcm2), (11025, pcm), "the VAGp header's rate and size")

    @unittest.skipUnless(HAVE_NP, 'numpy')
    def test_write_voices(self):
        with tempfile.TemporaryDirectory() as d:
            body = vag_block(12, 0, 0, [3] * 28) + vag_block(0, 0, 7, [0] * 28)
            talks = {'c': {'lines': [{'sound': 'B'}, {'sound': 'A'}, {'sound': 'B'}, {'sound': 'MISSING'}]}}
            index, dur = ua_cast.write_voices(Path(d) / 'Voice', talks, {'A': body, 'B': body})
            self.assertEqual(index, {'B': 0, 'A': 1}, 'each clip once, in first-said order')
            self.assertEqual(sorted(os.listdir(Path(d) / 'Voice')), ['v0000.wav', 'v0001.wav'])
            with wave.open(str(Path(d) / 'Voice' / 'v0000.wav')) as w:
                self.assertEqual((w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()), (1, 2, 22050, 28))
            self.assertAlmostEqual(dur[0], 28 / 22050)


ENGB = """<world>
 <entity name="fury_ent" classname="monsterspawnerent" character="nickfurynpc" monster_spawnscript="act1/stark/spawn_fury"
   monster_actonuse="true" monster_actscript="act1/stark/talk_fury" />
 <entity name="worker_ent" classname="monsterspawnerent" character="officeworkerfemale" />
 <entity name="spot" classname="pathent" />
 <entinst type="fury_ent"><inst name="sp_nickfurynpc01" pos="100 0 0" orient="0 0 0" /></entinst>
 <entinst type="worker_ent"><inst name="sp_talker01" pos="0 100 0" orient="0 0 1.5708" />
   <inst name="sp_talker02" pos="0 200 0" orient="0 0 0" /><inst name="sp_talker03" pos="0 300 0" orient="0 0 0" /></entinst>
 <entinst type="spot"><inst name="fury_standingspot01" pos="50 -50 10" orient="0 0 3.14159" />
   <inst name="player_start_all01" pos="10 10 10" orient="0 0 0" /></entinst>
</world>
"""
NPCSTAT = """<stats_root><stats name="nickfurynpc" skin="2101" charactername="Nick Fury" />
<stats name="officeworkerfemale" skin="15902" charactername="Office Worker" /></stats_root>"""


class Cast(unittest.TestCase):
    def test_plan_cast(self):
        with tempfile.TemporaryDirectory() as d:
            pkg = Path(d)
            (pkg / 'maps').mkdir()
            (pkg / 'maps' / 'm.engb.xml').write_text(ENGB)
            (pkg / 'data').mkdir()
            (pkg / 'data' / 'npcstat.engb.xml').write_text(NPCSTAT)
            sc = pkg / 'scripts' / 'act1' / 'stark'
            sc.mkdir(parents=True)
            (sc / 'spawn_fury.py').write_text('copyOriginAndAngles("_OWNER_", "fury_standingspot01")\nplayanim("EA_ZONE9", "_OWNER_")\n')
            (sc / 'talk_fury.py').write_text('startConversation("act1/stark/1_stark1_031")\nstartConversation("act1/stark/1_stark1_030")\n')
            cast = ua_cast.plan_cast(pkg, 'maps/m', .0254)
            fury = next(c for c in cast if c['id'] == 'nickfurynpc01')
            self.assertEqual((fury['skin'], fury['anim'], fury['talk'], fury['label']), ('2101', 'zone9', '1_stark1_030', 'NICK FURY'))
            self.assertEqual(fury['at'], [round(40 * .0254, 3), 0.0, round(60 * .0254, 3)], 'at its spot, room space')
            self.assertAlmostEqual(fury['yaw'], 180.0, places=0)
            self.assertTrue(fury['turns'])
            talkers = [c for c in cast if c['id'].startswith('talker')]
            self.assertEqual([c['group'] for c in sorted(cast, key=lambda c: c['id']) if c['id'].startswith('talker')],
                             ['pair01', 'pair01', 'pair02'], 'rand_npcs.py: talkers 2k-1 and 2k a pair')
            self.assertTrue(all(c['anim'] == 'zone10' and c['talk'] is None for c in talkers), 'office workers idle, mute')
            looks = ua_cast.looks_of(cast)
            self.assertEqual(sorted(l['name'] for l in looks), ['15902_zone10', '2101_zone9'])
            self.assertTrue(next(l for l in looks if l['name'] == '2101_zone9')['principal'])

    @unittest.skipUnless(HAVE_NP, 'numpy')
    def test_pack_cast_ranges_frames_and_atlas(self):
        import numpy as np
        tri = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1], [1, 0, 1], [0, 1, 1]], dtype=np.float32)
        red = Image.new('RGBA', (8, 8), (255, 0, 0, 255))
        cut = Image.new('RGBA', (8, 8), (0, 0, 255, 0))

        def look(name, frames, principal):
            return {'name': name, 'frames': [tri + k for k in range(frames)], 'uv': np.zeros((6, 2)), 'image': np.array([0, 1]),
                    'images': [red, cut], 'seconds': 2.0, 'turns': True, 'principal': principal}
        looks = [look('a', 4, True), look('b', 4, False)]
        with tempfile.TemporaryDirectory() as d:
            lines, total, sheets = ua_cast.pack_cast(looks, d, cap=12, page=64)
            self.assertEqual(lines, ['look a 0 2 4 2.00 1', 'look b 8 2 2 2.00 1'], 'the extras lose frames first')
            self.assertEqual((total, sheets), (12, 1))
            data = Path(d, 'scene.glb').read_bytes()
            magic, version, length = struct.unpack_from('<III', data)
            self.assertEqual((magic, version, length), (0x46546C67, 2, len(data)))
            jl = struct.unpack_from('<I', data, 12)[0]
            g = json.loads(data[20:20 + jl])
            self.assertNotIn('extensionsRequired', g, 'mesh_model refuses a required extension')
            self.assertEqual(g['materials'][0]['alphaMode'], 'MASK', 'a cut-out page')
            counts = [g['accessors'][p['attributes']['POSITION']]['count'] for p in g['meshes'][0]['primitives']]
            self.assertEqual(sum(counts), 3 * total, 'every frame whole, 3 corners a triangle')
            for p in g['meshes'][0]['primitives']:
                self.assertEqual(p['mode'], 4)
            # the cast.txt the game reads
            cast = [{'id': 'nickfurynpc01', 'look': 'a', 'at': [1, 0, 2], 'yaw': -50.0, 'group': '-', 'talk': '1_stark1_030',
                     'label': 'NICK FURY'},
                    {'id': 'talker01', 'look': 'b', 'at': [0, -3, 0], 'yaw': 0.0, 'group': 'pair01', 'talk': None, 'label': 'OFFICE WORKER'}]
            ua_cast.write_cast(Path(d, 'cast.txt'), lines, cast)
            text = Path(d, 'cast.txt').read_text()
            self.assertTrue(text.startswith(ua_cast.CAST_HEADER + '\n'))
            self.assertIn('npc nickfurynpc01 a 1.000 0.000 2.000 -50.0 - 1_stark1_030 NICK FURY', text)
            self.assertIn('npc talker01 b 0.000 -3.000 0.000 0.0 pair01 - OFFICE WORKER', text)

    @unittest.skipUnless(HAVE_NP, 'numpy')
    def test_subtitle_mock(self):
        with tempfile.TemporaryDirectory() as d:
            Image.new('RGB', (320, 180), (90, 90, 90)).save(Path(d, 'raw.png'))
            ua_cast.subtitle_mock(Path(d, 'raw.png'), Path(d, 'out.png'), 'Nick Fury', 'Welcome aboard, ' * 8)
            im = Image.open(Path(d, 'out.png'))
            self.assertEqual(im.size, (320, 180))
            self.assertNotEqual(im.getpixel((160, 170)), (90, 90, 90), 'the panel drawn at the bottom')


class Wiring(unittest.TestCase):
    def setUp(self):
        self.g = src('src/game_xr.cpp')

    def test_slot_and_budget(self):
        m = src('include/spidy/mesh_model.hpp')
        self.assertIn('"UA/StarkTower", "UA/StarkTower/Cast"};', m)
        self.assertIn('uaRoomTriangles, uaCastTriangles};', m)
        self.assertIn('slotUaStarkTower == 74 && slotUaStarkCast == 75 && slots == 76', m)
        self.assertIn('size_t{mesh_model::uaCastDrawnTriangles}', src('include/spidy/hero007_visual.hpp'))
        self.assertIn('src/ua_cast.cpp', src('CMakeLists.txt'))

    def test_read_at_load_and_drawn_with_the_room(self):
        g = self.g
        self.assertIn('readUaCast(at);', g)
        self.assertIn('ua_cast::parseCast(cast, uaCasts[at], error, model->triangles())', g)
        self.assertIn('ua_cast::parseTalk(talk, uaTalks[at], error)', g)
        self.assertIn('saved.uaPoses.push_back({i, uaYaw[i], uaIdleClock + 1.37f * static_cast<float>(i)});', g)
        self.assertIn('if (drew && saved->uaCastModel && saved->uaCast && !saved->uaPoses.empty()) {', g)
        self.assertIn('ua_cast::append(modelVertices, *saved->uaCastModel, *saved->uaCast, saved->uaPoses, place,', g)

    def test_talking(self):
        g = self.g
        self.assertIn('uaLookNpc = ua_cast::pick(cast, uaShown, uaVisit.roomFeet(), eyeR, onRing ? Vec3{} : lookR, handOn,', g)
        self.assertIn('if (uaChat.begin(talks, talks.find(cast.npcs[static_cast<size_t>(uaLookNpc)].talk))) {', g)
        self.assertIn('ci.point = handOn ? batcave074::talkChoiceAt(uaPanelView, hi.head, handW, dirW) : -1;', g)
        self.assertIn('uaExitDone = true; // B in a talk ends the talk, not the trip', g)
        self.assertIn('message(1, 0, "MISSIONS: COMING SOON");', g)
        self.assertIn('uaShown = ua_cast::shownFor(cast, pair);', g)

    def test_voices_play_alone(self):
        self.assertIn('uaVoice->setOnce(true);', self.g)
        self.assertIn('modelsPath(ua_expansion::folders[ua_expansion::starkTower], L"Voice")', self.g)
        w = src('src/music_audio_win.cpp')
        self.assertIn('if (count && !once.load())', w)
        self.assertIn('void setOnce(bool on);', src('include/spidy/music_audio.hpp'))

    def test_panel_drawn(self):
        b = src('src/batcave074.cpp')
        self.assertIn('m.textCentre("MISSIONS: COMING SOON", 0, .02f, .075f, gold);', b)
        self.assertIn('t.textCentre("A: TALK", 0, -.06f, .045f, gold, .002f, .7f + .3f * pulse);', b)

    def test_converter(self):
        c = src('tools/ua/convert_ua.py')
        for s in ("build_cast(a, pkg / MAP, work, out, env)", "ua_extract.iso_extract_file(a.iso, HUB_BANK, work / 'sounds')",
                  "ua_cast.write_talk(out / 'talk.txt'", "ua_cast.pack_cast(looks, out / 'Cast', CAST_CAP)", "'--no-cast'"):
            self.assertIn(s, c)
        self.assertIn('CAST_CAP = 140000', c)
        self.assertIn('inline constexpr uint32_t uaCastTriangles = 140000', src('include/spidy/mesh_model.hpp'))

    def test_nothing_of_the_game_committed(self):
        gi = src('.gitignore')
        self.assertIn('/Models/UA/', gi)
        for p in Path(ROOT).rglob('*'):
            if '.git' in p.parts or 'build' in p.parts[-3:]:
                continue
            self.assertFalse(p.suffix.lower() in ('.igb', '.ens', '.vag', '.fb'), p)


if __name__ == '__main__':
    unittest.main()
