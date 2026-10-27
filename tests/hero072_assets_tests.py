# Version 0.0.72: the missing-asset gate. The player's own sounds (sha256 as sent), their unchanged
# decodes, the CMake embed mappings, and the test zip's required list (models + Sounds/).
import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'release072'))
import required_assets as ra


class UserSounds(unittest.TestCase):
    def test_each_mp3_is_the_players_file(self):
        for name, (digest, _id, _wav) in ra.USER_SOUNDS.items():
            p = ra.source_mp3(name)
            self.assertTrue(os.path.isfile(p), 'MISSING: ' + p)
            self.assertEqual(hashlib.sha256(open(p, 'rb').read()).hexdigest(), digest, 'CHANGED: ' + p)

    def test_each_wav_is_embedded_and_mapped(self):
        cm = (ROOT / 'CMakeLists.txt').read_text()
        names = re.search(r'foreach\(SPIDY_SFX_NAME([^)]*)\)', cm).group(1).split()
        for name, (_d, sid, wav) in ra.USER_SOUNDS.items():
            self.assertIn(sid, names, sid + ' not embedded')
            self.assertTrue((ROOT / 'assets' / 'sfx' / (wav + '.wav')).is_file(), wav + '.wav missing')
            if wav != sid:
                self.assertRegex(cm, r'set\(SPIDY_SFX_FILE_%s "%s"\)' % (sid, wav), sid + ' not mapped to the player\'s sound')

    def test_user_guns_lists_the_same_sounds(self):
        text = (ROOT / 'tools' / 'sfx' / 'user_guns.py').read_text()
        for name, (digest, _id, wav) in ra.USER_SOUNDS.items():
            self.assertIn(digest, text)
            self.assertIn(wav + '.wav', text)

    @unittest.skipIf(not shutil.which('ffmpeg'), 'needs ffmpeg')
    def test_wavs_are_the_unchanged_decode(self):
        r = subprocess.run([sys.executable, str(ROOT / 'tools' / 'sfx' / 'user_guns.py'), '--check'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)


class ZipList(unittest.TestCase):
    def test_required_list(self):
        need = ra.required_zip_files()
        # test build 5: the mod files under payload/, the installer and the notes at the top
        for s in ('payload/Sounds/user-shotgun.mp3', 'payload/Sounds/user-pistol.mp3', 'payload/Sounds/user-pistol-reload.mp3',
                  'payload/Sounds/user-lightning-1.mp3', 'payload/Sounds/user-lightning-2.mp3', 'READ-ME-FIRST.txt',
                  'payload/SHA256SUMS.txt', 'payload/HERO-VR-TEST-BUILD.txt', 'INSTALL-TEST-BUILD.bat', 'INSTALL-TEST-BUILD.ps1'):
            self.assertIn(s, need)
        self.assertEqual(len(need), len(set(need)))

    @unittest.skipIf(os.environ.get('CI') or os.environ.get('GITHUB_ACTIONS') or not (ROOT / 'build' / 'h7out' / 'Models').is_dir(),
                     'the converted models are built locally')
    def test_models_present(self):
        for f in ra.MODEL_FILES:
            self.assertTrue((ROOT / 'build' / 'h7out' / f).is_file(), 'MISSING: ' + f)


if __name__ == '__main__':
    unittest.main()
