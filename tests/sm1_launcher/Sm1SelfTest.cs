// HERO VR 0.0.75 SM1: logic checks for the launcher's Spider-Man 1 option (game tags in the server list, his release's
// zip handling). Compiled with the launcher sources by tests/sm1_launcher_tests.py. GPLv3. Made by Agent Mulder.
using System;
using System.Threading;
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
            string rootFile = Path.Combine(Path.GetDirectoryName(manifest), Sm1.LayerRootFile);
            Check(File.Exists(rootFile) && File.ReadAllText(rootFile) == Path.GetFullPath(tmp), "the staged layer knows HERO VR's folder (herovr_root.txt)");
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
            Check(Sm1.Features.Length == 10 && Sm1.Features.Count(x => x[1] == "stub") == 2 && Sm1.FeatureSummary().Contains("holo suit switch") && Sm1.FeatureSummary().Contains("color"), "feature list");


            // 0.0.76 SM1: his launcher.ini read as his loadSettings reads it, and his sessionArguments.
            var o0 = Sm1Session.Parse("");
            var a0 = Sm1Session.Arguments(o0, @"C:\r\game-vr-1.json", "", @"Local\HeroVrSm1Stop-1-2", false);
            Check(string.Join(" ", a0.ToArray()) == @"--auto-launch --seconds 0 --swing-speed 32 --output C:\r\game-vr-1.json --stop-event Local\HeroVrSm1Stop-1-2", "session arguments: his defaults add nothing");
            var o1 = Sm1Session.Parse("game=D:\\g\\Spider-Man.exe\r\nsmall_window=0\r\naim_markers=0\nflips=1\ngame_sharpening=1\nrender_scale=250\nupscale=80\nmonitor_rate=0\nswing_speed=99\nsnap_turn=45\nhud=7\nweight_percent=80\nweight=120\neye_height_mm=1700\narm_length_mm=650\nxr_runtime=\njunk\n");
            var a1 = string.Join(" ", Sm1Session.Arguments(o1, "C:\\a b\\r.json", "", "E", true).ToArray());
            Check(a1 == "--auto-launch --seconds 0 --swing-speed 65 --output C:\\a b\\r.json --full-desktop-view --no-aim-markers --flips --game-sharpening --eye-height 1700 --arm-length 650 --upscale 80 --snap-turn 45 --hud 3 --monitor-rate 0 --stop-event E --close-game", "session arguments: " + a1);
            Check(o1.RenderScale == 100 && o1.Runtime == "auto" && o1.GameExe == "D:\\g\\Spider-Man.exe", "launcher.ini: out-of-range render scale is his default; empty runtime is auto");
            var o2 = Sm1Session.Parse("gravity_percent=150\nweight_percent=90\neye_height_mm=900\narm_length_mm=650\ncalibration_prompt=0\n");
            Check(o2.Weight == 150 && o2.EyeHeightMm == 0 && o2.ArmLengthMm == 0, "launcher.ini: gravity_percent wins; a half calibration is none");
            Check(Sm1Session.Arguments(o2, "r", "", "", false).Contains("--no-calibration-prompt"), "no calibration and no prompt: --no-calibration-prompt");
            Check(Sm1Session.Parse("weight=120\nweight_percent=80\n").Weight == 100 && Sm1Session.Parse("weight=120\n").Weight == 120 && Sm1Session.Parse("weight=60\n").Weight == 100 && Sm1Session.Parse("weight_percent=80\n").Weight == 100, "launcher.ini: old default weights are today's");
            Check(Sm1Session.Quote("plain") == "plain" && Sm1Session.Quote("") == "\"\"" && Sm1Session.Quote("a b") == "\"a b\"" &&
                  Sm1Session.Quote("C:\\a b\\") == "\"C:\\a b\\\\\"" && Sm1Session.Quote("say \"hi\"") == "\"say \\\"hi\\\"\"", "quoteArgument as his");
            Check(Sm1Session.CommandLine("C:\\Spidy 0.7\\tools\\run_game_vr.py", new List<string> { "--output", "x y" }) == "-B -u -X utf8 \"C:\\Spidy 0.7\\tools\\run_game_vr.py\" --output \"x y\"", "command line");
            var rt = Sm1Session.OpenVrRuntimes("{\"config\":[\"C:\\\\x\"],\"runtime\" : [ \"C:\\\\Program Files (x86)\\\\Steam\\\\steamapps\\\\common\\\\SteamVR\" ],\"version\":1}");
            Check(rt.Count == 1 && rt[0] == @"C:\Program Files (x86)\Steam\steamapps\common\SteamVR", "openvrpaths runtime folder");
            var bl = Sm1Session.Blockers(null, "x.exe", "4.629.0.0", false, true);
            Check(bl.Count == 4 && Sm1Session.Blockers(tmp, null, null, true, false).Count == 2, "pre-start checks: mod, version, runtime, administrator");
            Check(Sm1Session.NewReportPath(new DateTime(2026, 11, 1, 3, 4, 5)).EndsWith(Path.Combine("HERO VR SM1", "reports", "game-vr-20261101-030405.json")), "report in HERO VR SM1\\reports");
            if (Environment.OSVersion.Platform != PlatformID.Win32NT)
            {
                // A stand-in for his python: prints its arguments; our session pipes them to the log file.
                string mod = Path.Combine(tmp, "spidy");
                Directory.CreateDirectory(Path.Combine(mod, "python")); Directory.CreateDirectory(Path.Combine(mod, "tools"));
                string py = Path.Combine(mod, Sm1Session.Python.Replace('\\', Path.DirectorySeparatorChar));
                File.WriteAllText(py, "#!/bin/sh\necho \"started $*\"\necho oops >&2\n");
                System.Diagnostics.Process.Start("chmod", "+x \"" + py + "\"").WaitForExit();
                var session = new Sm1VrSession();
                var lines = new List<string>(); int exit = -99;
                var done = new ManualResetEvent(false);
                try
                {
                    session.Start(mod, o0, false, l => { lock (lines) lines.Add(l); }, c => { exit = c; done.Set(); });
                    Check(done.WaitOne(20000) && exit == 0, "session ran and ended");
                    Thread.Sleep(300);
                    string logText = File.ReadAllText(session.LogPath);
                    Check(logText.Contains("started -B -u -X utf8") && logText.Contains("--stop-event Local") && logText.Contains("oops"), "his output piped to our log");
                    Check(!session.Running && !session.Stop(), "ended session: not running, no stop");
                }
                catch (Exception ex) { Check(false, "session start: " + ex.Message); }
            }

            try { Directory.Delete(tmp, true); } catch { }
            Console.WriteLine(fails == 0 ? "ALL PASS" : fails + " FAILED");
            return fails == 0 ? 0 : 1;
        }
    }
}
