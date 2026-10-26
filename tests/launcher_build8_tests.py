"""0.0.6 hotfix, launcher build 8: a same-version hotfix update replaces stale ready-made mod files, and a blank or
placeholder repository setting falls back to lecutis7of9/HEROVR.

Found by the 0.0.6 hotfix e2e (self-update from launcher build 6): build 6 saw the same Version 0.0.6, kept its working
mod files (Update 39.1) beside the Update 39.1.1 sources, so the swapchain-retry modules never arrived."""
import pathlib, re, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
LAUNCHER = next((p for p in (ROOT/'tools/launcher/src', ROOT.parent/'herovr-launcher-0062/src', ROOT.parent/'herovr-launcher-006/src') if p.is_dir()),
                ROOT/'tools/launcher/src')
def src(n): return (LAUNCHER/n).read_text(encoding='utf-8')

@unittest.skipUnless(LAUNCHER.is_dir(), 'launcher sources not beside this tree')
class LauncherBuild8(unittest.TestCase):
    def test_build_number(self):
        mv = src('ModVersion.cs')
        self.assertRegex(mv, r'public const int Build = (\d+);')
        self.assertGreaterEqual(int(re.search(r'public const int Build = (\d+);', mv).group(1)), 8)

    def test_stale_mod_files_are_detected_from_the_marker(self):
        mf = src('ModFiles.cs')
        self.assertIn('const string MarkerHead = "Ready-made HERO VR mod files for ";', mf)
        # the marker it reads is the one Install writes
        self.assertIn('"Ready-made HERO VR mod files for " + (want ?? have ?? "?")', mf)
        self.assertIn('public static bool StaleFor(string root, string markerPath)', mf)
        self.assertIn('return files != null && sources != null && !string.Equals(files, sources, StringComparison.OrdinalIgnoreCase);', mf)
        # an unknown marker ("?") or none is never stale (developer builds have no marker)
        self.assertIn('return l.Length == 0 || l == "?" ? null : l;', mf)

    def test_same_version_update_downloads_a_newer_builds_mod_files(self):
        ins = src('Installer.cs')
        i = ins.index('void PrepareMod(')
        body = ins[i:ins.index('void DropNewBuild', i)]
        self.assertLess(body.index('ModFiles.StaleFor(Root, parkedMarker)'), body.index('Backups.UnparkBuild(Root, Log);'))
        self.assertIn('else if (parked && nowLong != null', body)
        self.assertIn('if (InstallModFiles()) return;', body)

    def test_start_up_replaces_stale_mod_files_by_itself(self):
        up = src('MainForm.Updates.cs')
        i = up.index('void FirstRunModFiles()')
        body = up[i:up.index('void RepairMod(', i)]
        self.assertIn('string why = ModFiles.RepairReason(root);', body)
        self.assertIn('if (why == null) return;', body)
        self.assertIn('RepairMod(null, true);', body)

    # 0.0.6.5 regression (Bear, 39.1.1): the same-version update dropped python\ (it is only in the mod zip) and the
    # unpark restored only build\, so launch-game-vr.ps1 found no Python. Build 8 checks python\python.exe everywhere.
    def test_repair_reason_covers_missing_bundled_python(self):
        mf = src('ModFiles.cs')
        self.assertIn('return Path.Combine(root, "python", "python.exe");', mf)
        i = mf.index('public static string RepairReason(string root)')
        body = mf[i:mf.index('}', mf.index('return null;', mf.index('Stale(root)', i)))]
        for needle in ('if (!Installed(root))', 'if (!FromRelease(root)) return null;', 'if (!PythonPresent(root))', 'if (Stale(root))'):
            self.assertIn(needle, body)
        # Install refuses to report success when the zip had a Python but the folder has none afterwards
        self.assertIn('zipHasPython = File.Exists(Path.Combine(tmp, "python", "python.exe"));', mf)
        self.assertIn('if (zipHasPython && !PythonPresent(root))', mf)

    def test_same_version_update_reinstalls_when_python_is_missing(self):
        ins = src('Installer.cs')
        i = ins.index('void PrepareMod(')
        body = ins[i:ins.index('void DropNewBuild', i)]
        j = body.index('Backups.UnparkBuild(Root, Log);')
        k = body.index('if (ModFiles.PythonPresent(Root) || !ModFiles.FromRelease(Root))', j)
        self.assertLess(k, body.index('kept its working mod files', j))
        self.assertLess(k, body.rindex('if (InstallModFiles()) return;'))

    def test_play_vr_and_status_repair_a_folder_without_python(self):
        mf = src('MainForm.cs')
        self.assertIn('string repairWhy = ModFiles.RepairReason(root);', mf)
        self.assertIn('RepairMod(ok => { RefreshLive(); if (ok && ModFiles.RepairReason(root) == null) LaunchVr(extra); }, true);', mf)
        self.assertLess(mf.index('string repairWhy = ModFiles.RepairReason(root);'), mf.index('LaunchVr(extra);\n        }\n\n        void LaunchVr('))
        self.assertIn('else if (ModFiles.RepairReason(root) != null) SetState("MOD FILES NEEDED"', mf)

    def test_blank_or_placeholder_repository_falls_back_to_herovr(self):
        st = src('Settings.cs')
        self.assertIn('public const string DefaultRepo = "lecutis7of9/HEROVR";', st)
        self.assertIn('get { string r = Get("UpdateRepo", ""); return IsPlaceholderRepo(r) ? DefaultRepo : r; }', st)
        self.assertIn('return r.Length == 0 || r.Equals(RepoPlaceholder, StringComparison.OrdinalIgnoreCase) || r.StartsWith("YOUR-", StringComparison.OrdinalIgnoreCase);', st)

if __name__ == '__main__':
    unittest.main()
