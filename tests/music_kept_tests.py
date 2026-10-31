# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR.
# 0.0.74 test 10 MUSIC KEPT ("every time we update i lose all the music in my music folder"): the songs live in
# Documents\HERO VR\Music for every player, sub-folders are read, and no installer writes the install's Music folder.
# Made by Agent Mulder.
import pathlib, re, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import live_settings as live


def text(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


class MusicKeptSources(unittest.TestCase):
    def test_every_player_reads_the_kept_folder(self):
        pm = text('src/phone_music.cpp')
        self.assertIn('FOLDERID_Documents', pm)
        self.assertIn('vehicle074::musicFolderUnderDocuments', pm)
        self.assertIn('std::vector<std::wstring> musicFolders()', pm)
        self.assertIn('musicFiles(musicFolders())', pm)  # the phone
        ma = text('src/music_audio_win.cpp')
        self.assertIn('musicFolders()', ma)                # the wrist page and the van
        self.assertIn('musicFiles(folders, &quit)', ma)
        self.assertNotIn('folder + L"\\\\" + wide(', ma)     # the list holds full paths now
        gx = text('src/game_xr.cpp')
        self.assertIn('music::Player carMusic;', gx)
        self.assertNotIn('carMusicFolder', gx)

    def test_sub_folders_are_read(self):
        pm = text('src/phone_music.cpp')
        self.assertIn('FILE_ATTRIBUTE_REPARSE_POINT', pm)  # no junction loops
        self.assertIn('music::folderDepth', pm)
        self.assertIn('findSongs(sub, depth + 1', pm)
        self.assertRegex(text('include/spidy/music_player.hpp'), r'constexpr int folderDepth = 8;')
        self.assertEqual(live.MUSIC_DEPTH, 8)
        self.assertIn('Sub-folders are read too', text('include/spidy/vehicle074.hpp'))

    def test_same_formats_as_the_player(self):
        cpp = text('src/music_player.cpp')
        listed = re.search(r'for \(const char\* e : \{([^}]*)\}\)', cpp).group(1)
        exts = tuple('.' + e for e in re.findall(r'"(\w+)"', listed))
        self.assertEqual(sorted(exts), sorted(live.SONG_EXTENSIONS))

    def test_installers_never_write_music(self):
        inst = text('tools/launcher/src/Installer.cs')
        self.assertIn('rel.StartsWith(Backups.MusicFolder + "/", StringComparison.OrdinalIgnoreCase)) continue;', inst)
        bk = text('tools/launcher/src/Backups.cs')
        self.assertIn('public const string MusicFolder = "Music";', bk)
        self.assertIn('"music"', bk)  # SkipTop: never backed up or deleted by a restore
        ps = text('tools/test073/INSTALL-TEST-BUILD.ps1')
        self.assertIn("$entries = @($entries | Where-Object { $_[1] -notmatch '^(?i)Music[\\\\/]' })", ps)
        self.assertLess(ps.index("notmatch '^(?i)Music"), ps.index('# 3. Copy, keeping what is replaced\n') + 400)
        self.assertNotIn("os.makedirs(os.path.join(pay, 'Music')", text('tools/test073/make_test.py'))
        self.assertTrue(ps.isascii())
        self.assertTrue(text('tools/package.ps1').isascii())

    def test_docs(self):
        self.assertIn('Documents\\HERO VR\\Music', text('SHARING.md'))
        self.assertIn('Documents\\HERO VR\\Music', text('tools/package.ps1'))


if __name__ == '__main__':
    unittest.main()
