// HERO VR 0.0.75 SM1: logic checks for the launcher's Spider-Man 1 option (game tags in the server list, his release's
// zip handling). Compiled with the launcher sources by tests/sm1_launcher_tests.py. GPLv3. Made by Agent Mulder.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;

namespace Sm2VrLauncher
{
    static class Sm1SelfTest
    {
        static int fails;
        static void Check(bool ok, string what) { Console.WriteLine((ok ? "PASS " : "FAIL ") + what); if (!ok) fails++; }

        static LobbyHost Lan(uint id, int protocol)
        {
            return new LobbyHost { Address = System.Net.IPAddress.Loopback, Beacon = new LobbyBeacon { HostId = id, Protocol = (byte)protocol, Name = "L" + id, MaxPlayers = 2, Players = 1, Port = 39470 } };
        }

        static void Zip(string file, params string[] names)
        {
            using (var z = ZipFile.Open(file, ZipArchiveMode.Create))
                foreach (var n in names)
                {
                    var e = z.CreateEntry(n);
                    if (!n.EndsWith("/")) using (var w = new StreamWriter(e.Open())) w.Write("his file " + n);
                }
        }

        static int Main()
        {
            // The server list: each game sees its own games only.
            Check(LobbyGame.Protocol(LobbyGame.Sm2) == LobbyBeacon.MpProtocol, "sm2 keeps HERO VR's protocol");
            Check(LobbyGame.Protocol(LobbyGame.Sm1) == 128 + LobbyBeacon.MpProtocol, "sm1 is protocol 129");
            Check(LobbyGame.Protocol(LobbyGame.Sm1) <= 255, "sm1 protocol fits the worker's 0..255");
            Check(LobbyGame.Of(1) == "sm2" && LobbyGame.Of(129) == "sm1" && LobbyGame.Of(0) == "sm2", "Of");
            Check(LobbyGame.SameVersion(1) && LobbyGame.SameVersion(129) && !LobbyGame.SameVersion(2) && !LobbyGame.SameVersion(130), "SameVersion per game");
            Check(LobbyGame.Shown(1, null) && LobbyGame.Shown(129, ""), "no game: every row (as before)");
            Check(LobbyGame.Shown(1, "sm2") && !LobbyGame.Shown(129, "sm2") && LobbyGame.Shown(129, "sm1") && !LobbyGame.Shown(1, "sm1"), "Shown filters");
            var lan = new List<LobbyHost> { Lan(1, 1), Lan(2, 129) };
            var inet = new List<InetGame> { new InetGame { Id = "a", Name = "A", Protocol = 1 }, new InetGame { Id = "b", Name = "B", Protocol = 129 }, new InetGame { Id = "c", Name = "C", Protocol = 2 } };
            var all = InternetLobby.Merge(lan, inet, new uint[0]);
            Check(all.Count == 5, "3-arg Merge keeps every row (" + all.Count + ")");
            var sm2 = InternetLobby.Merge(lan, inet, new uint[0], LobbyGame.Sm2);
            Check(sm2.Count == 3 && sm2.All(e => LobbyGame.Of(e.Lan != null ? e.Lan.Beacon.Protocol : e.Inet.Protocol) == "sm2"), "sm2 list: sm2 rows only (" + sm2.Count + ")");
            var sm1 = InternetLobby.Merge(lan, inet, new uint[0], LobbyGame.Sm1);
            Check(sm1.Count == 2 && sm1.All(e => e.SameVersion), "sm1 list: sm1 rows only, joinable (" + sm1.Count + ")");
            Check(sm2.Any(e => !e.SameVersion), "an other-version sm2 game still lists as 'Update'");

            // His release.
            Check(Sm1.IsReleaseZipName("Spidy-0.7.0-win64.zip") && !Sm1.IsReleaseZipName("Spidy-0.7.0-src.zip") && !Sm1.IsReleaseZipName(null), "release zip name");
            Check(Sm1.RepoUrl == "https://github.com/IlyaMez/SpidyVR", "his repo");
            Check(Sm1.ReleasesUrl.StartsWith(Sm1.RepoUrl + "/releases"), "his releases page");
            Check(Sm1.LatestApi == "https://api.github.com/repos/IlyaMez/SpidyVR/releases/latest", "his releases API");
            Check(Sm1.SteamAppId == "1817070" && Sm1.GameExe == "Spider-Man.exe" && Sm1.SupportedVersion == "4.630.0.0", "game facts");
            Check(Sm1.Title == "iilyn's Spider-Man VR mod, with Mulder's Heroes add-on", "title");

            string tmp = Path.Combine(Path.GetTempPath(), "sm1st-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tmp);
            string f = Path.Combine(tmp, "x.bin");
            File.WriteAllText(f, "abc");
            Check(Sm1.Sha256Of(f) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256");

            var log = new List<string>();
            string good = Path.Combine(tmp, "Spidy-9.9.9-win64.zip");
            Zip(good, "Spidy-9.9.9/", "Spidy-9.9.9/Spidy Launcher.exe", "Spidy-9.9.9/tools/run_game_vr.py", "Spidy-9.9.9/build/windows-ninja/spidy_stereo_probe.dll");
            string dir = Sm1.InstallZip(good, "9.9.9", Sm1.Sha256Of(good), log.Add);
            Check(dir == Sm1.DefaultModFolder && Sm1.IsModFolder(dir), "install: his top folder dropped, mod found");
            Check(File.ReadAllText(Path.Combine(dir, "Spidy Launcher.exe")) == "his file Spidy-9.9.9/Spidy Launcher.exe", "install: his files unchanged");
            Check(Sm1.InstalledVersion(dir) == "9.9.9", "install: version marker");
            Check(!File.Exists(Path.Combine(dir, ".herovr")) && Directory.GetFiles(dir, "*herovr*", SearchOption.AllDirectories).Length == 0, "nothing of ours inside his folder");

            string evil = Path.Combine(tmp, "evil.zip");
            Zip(evil, "../outside.txt", "Spidy Launcher.exe", "tools/run_game_vr.py");
            bool threw = false;
            try { Sm1.InstallZip(evil, "6.6.6", "x", log.Add); } catch (InvalidDataException) { threw = true; }
            Check(threw && !File.Exists(Path.Combine(Sm1.DataFolder, "outside.txt")), "zip-slip refused");
            Check(Sm1.InstalledVersion(dir) == "9.9.9" && Sm1.IsModFolder(dir), "a failed install keeps the old copy");

            string wrong = Path.Combine(tmp, "wrong.zip");
            Zip(wrong, "Spidy-1/readme.txt");
            threw = false;
            try { Sm1.InstallZip(wrong, "1", "x", log.Add); } catch (InvalidDataException) { threw = true; }
            Check(threw && Sm1.IsModFolder(dir), "a zip without his launcher is refused");

            // Texts: what is missing and why; the two answers are the dialog's.
            Check(Sm1.GameMissingText("x").Contains("Yes, Autosetup") && Sm1.ModMissingText().Contains("Yes, Autosetup") && Sm1.AddonMissingText().Contains("Yes, Autosetup"), "pop-up texts say what Yes does");
            Check(Sm1.GameVersion(null) == null && Sm1.GameVersion(f) == null, "GameVersion never throws");
            Check(!Sm1.AddonPresent(null) && Sm1.StageAddon(tmp) == null, "no add-on: nothing staged");

            // 0.0.76: the OpenXR layer - staged in our folder, registered only under HKCU, our entries only removed.
            Check(!Sm1.LayerPresent(null) && Sm1.StageLayer(tmp) == null, "no layer: nothing staged");
            string bin = Path.Combine(Path.Combine(tmp, "build"), "windows-ninja");
            Directory.CreateDirectory(bin);
            File.WriteAllText(Path.Combine(bin, Sm1.LayerDll), "dll");
            File.WriteAllText(Path.Combine(bin, Sm1.LayerJson), "{}");
            string manifest = Sm1.StageLayer(tmp);
            Check(manifest != null && manifest.StartsWith(Path.Combine(Sm1.DataFolder, "layer")) && File.Exists(manifest) &&
                  File.Exists(Path.Combine(Path.GetDirectoryName(manifest), Sm1.LayerDll)), "layer staged in SM1\\layer\\<sha>");
            Check(Sm1.StageLayer(tmp) == manifest, "staging again: the same copy");
            const string key = @"Software\HEROVR-sm1-selftest\Implicit";
            using (var k = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(key))
            {
                k.SetValue(@"C:\other\some_layer.json", 0, Microsoft.Win32.RegistryValueKind.DWord);
                k.SetValue(@"C:\old\stale\" + Sm1.LayerJson, 0, Microsoft.Win32.RegistryValueKind.DWord);
            }
            Sm1.RegisterLayer(manifest, key);
            var ours = Sm1.RegisteredLayers(key);
            Check(ours.Length == 1 && ours[0] == manifest, "register: one entry of ours, the stale one gone");
            using (var k = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(key))
                Check(k.GetValue(@"C:\other\some_layer.json") != null && (int)k.GetValue(manifest) == 0, "register: other layers kept, ours enabled (0)");
            Check(Sm1.RemoveLayers(key) == 1 && Sm1.RegisteredLayers(key).Length == 0, "remove: ours gone");
            using (var k = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(key))
                Check(k.GetValue(@"C:\other\some_layer.json") != null, "remove: other layers kept");
            Check(Sm1.RemoveLayers(@"Software\HEROVR-sm1-selftest\none") == 0, "remove: no key, no throw");
            try { Microsoft.Win32.Registry.CurrentUser.DeleteSubKeyTree(@"Software\HEROVR-sm1-selftest"); } catch { }

            // addon.ini: the switches written, the wrist menu's picks (hero=, outfit.N=) kept.
            Directory.CreateDirectory(Sm1.AddonDataFolder);
            File.WriteAllText(Path.Combine(Sm1.AddonDataFolder, "addon.ini"), "heroes=1\nhero=5\noutfit.5=2\noutfit.1=1\nflight=1\noutfit.x=9\n");
            Sm1.WriteAddonIni(true, false, false);
            string ini = File.ReadAllText(Path.Combine(Sm1.AddonDataFolder, "addon.ini"));
            Check(ini.Contains("hero=5\n") && ini.Contains("outfit.5=2\n") && ini.Contains("outfit.1=1\n") && !ini.Contains("outfit.x"), "addon.ini keeps hero and outfits");
            Check(ini.Contains("wrist_menu=0\n") && ini.Contains("flight=0\n") && ini.Contains("heroes=1\n"), "addon.ini switches");
            Check(Sm1.Features.Length == 7 && Sm1.Features.Count(x => x[1] == "stub") == 1 && Sm1.FeatureSummary().Contains("NOT YET: hero models"), "feature list");

            try { Directory.Delete(tmp, true); } catch { }
            Console.WriteLine(fails == 0 ? "ALL PASS" : fails + " FAILED");
            return fails == 0 ? 0 : 1;
        }
    }
}
