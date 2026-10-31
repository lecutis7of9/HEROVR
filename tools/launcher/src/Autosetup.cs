// SM2 VR Launcher - 0.0.74 MP INTERNET: the "No" / "Yes, Autosetup" pop-up. It lists what multiplayer still needs on
// this PC, each with one or two plain sentences on why; YES does all of it with ONE Windows admin prompt:
// downloads are fetched and their SHA-256 checked first (a wrong file is deleted, never run), then the installers
// and the firewall rule run together in a single elevated cmd. Today only the firewall rule is needed: the relay
// runs on our server, so players install nothing. NO is remembered; the Multiplayer sheet offers it again.
// Made by Agent Mulder.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;

namespace Sm2VrLauncher
{
    enum SetupKind { Firewall, Download }

    class SetupItem
    {
        public string Title = "", Why = "";
        public SetupKind Kind;
        // Download: fetched to the temp folder, checked against Sha256 (hex), then run elevated with InstallArgs.
        public string Url = "", Sha256 = "", FileName = "", InstallArgs = "";
        public Func<bool> IsDone = () => false;
    }

    static class Autosetup
    {
        public const string KeyDeclined = "MpSetupDeclined";
        public const string FirewallRule = Lobby.FirewallRule;
        public const string PortRange = "39469-39474";   // the LAN list (39469) and the game (39470-39474)

        /// <summary>The firewall rule: inbound UDP on the multiplayer ports, Private and Public networks (Windows calls
        /// most home Wi-Fi "Public"; without it a friend's game cannot reach yours). Only these ports, only UDP.</summary>
        public static SetupItem FirewallItem()
        {
            return new SetupItem
            {
                Title = "Let multiplayer through the Windows Firewall",
                Why = "Windows blocks other players from reaching your game until you allow it. This adds one rule for the HERO VR multiplayer ports (UDP " + PortRange + ") and nothing else.",
                Kind = SetupKind.Firewall,
                IsDone = FirewallRuleExists,
            };
        }

        /// <summary>Everything multiplayer needs that this PC does not have yet.</summary>
        public static List<SetupItem> Needed(IEnumerable<SetupItem> all)
        {
            return all.Where(i => { try { return !i.IsDone(); } catch { return true; } }).ToList();
        }

        public static List<SetupItem> All() { return new List<SetupItem> { FirewallItem() }; }

        public static bool FirewallRuleExists()
        {
            if (Environment.OSVersion.Platform != PlatformID.Win32NT) return true;
            try
            {
                var psi = new ProcessStartInfo("netsh", "advfirewall firewall show rule name=\"" + FirewallRule + "\"")
                { UseShellExecute = false, RedirectStandardOutput = true, CreateNoWindow = true };
                using (var p = Process.Start(psi))
                {
                    string o = p.StandardOutput.ReadToEnd();
                    p.WaitForExit(5000);
                    return p.ExitCode == 0 && o.IndexOf(PortRange, StringComparison.Ordinal) >= 0 && o.IndexOf("Public", StringComparison.OrdinalIgnoreCase) >= 0;
                }
            }
            catch { return false; }
        }

        /// <summary>The netsh steps: the old LAN-only rule goes, the new one (Private + Public) comes.</summary>
        public static List<string> FirewallCommands()
        {
            return new List<string>
            {
                "netsh advfirewall firewall delete rule name=\"" + FirewallRule + "\" >nul 2>&1",
                "netsh advfirewall firewall add rule name=\"" + FirewallRule + "\" dir=in action=allow protocol=UDP localport=" + PortRange + " profile=private,public",
            };
        }

        /// <summary>ONE command line for every step (one admin prompt): cmd /c "a & b & c". Installers first.</summary>
        public static string ElevatedArgs(IEnumerable<SetupItem> items, IDictionary<SetupItem, string> downloaded)
        {
            var steps = new List<string>();
            foreach (var i in items.Where(x => x.Kind == SetupKind.Download))
            {
                string file;
                if (downloaded != null && downloaded.TryGetValue(i, out file)) steps.Add("\"" + file + "\" " + i.InstallArgs);
            }
            if (items.Any(x => x.Kind == SetupKind.Firewall)) steps.AddRange(FirewallCommands());
            return "/c \"" + string.Join(" & ", steps.ToArray()) + "\"";
        }

        public static string Sha256Of(string path)
        {
            using (var s = File.OpenRead(path))
            using (var h = SHA256.Create())
                return string.Concat(h.ComputeHash(s).Select(b => b.ToString("x2")).ToArray());
        }

        /// <summary>Downloads to folder and checks the hash; a mismatch deletes the file and says so (null).</summary>
        public static string DownloadChecked(SetupItem i, string folder, out string problem)
        {
            problem = "";
            string path = Path.Combine(folder, string.IsNullOrEmpty(i.FileName) ? "herovr-setup.exe" : i.FileName);
            try
            {
                try { ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072; } catch { }
                Directory.CreateDirectory(folder);
                using (var wc = new WebClient()) wc.DownloadFile(i.Url, path);
                string got = Sha256Of(path);
                if (!string.Equals(got, (i.Sha256 ?? "").Trim(), StringComparison.OrdinalIgnoreCase))
                {
                    try { File.Delete(path); } catch { }
                    problem = i.Title + ": the download is not the expected file (SHA-256 " + got + ") - not installed.";
                    return null;
                }
                return path;
            }
            catch (Exception ex)
            {
                try { if (File.Exists(path)) File.Delete(path); } catch { }
                problem = i.Title + ": download failed (" + ex.Message + ").";
                return null;
            }
        }

        /// <summary>YES: downloads (checked), then one elevated cmd. Returns the lines for the session log.</summary>
        public static List<string> Run(IList<SetupItem> items)
        {
            var log = new List<string>();
            var got = new Dictionary<SetupItem, string>();
            string tmp = Path.Combine(Path.GetTempPath(), "herovr-setup");
            foreach (var i in items.Where(x => x.Kind == SetupKind.Download))
            {
                string problem, f = DownloadChecked(i, tmp, out problem);
                if (f == null) log.Add(problem); else got[i] = f;
            }
            var run = items.Where(x => x.Kind == SetupKind.Firewall || got.ContainsKey(x)).ToList();
            if (run.Count == 0) return log;
            try
            {
                var psi = new ProcessStartInfo("cmd.exe", ElevatedArgs(run, got)) { Verb = "runas", UseShellExecute = true, WindowStyle = ProcessWindowStyle.Hidden };
                using (var p = Process.Start(psi)) { if (p != null) p.WaitForExit(120000); }
            }
            catch (Exception ex) { log.Add("Setup was not run (" + ex.Message + ")."); return log; }
            foreach (var i in run)
            {
                bool ok;
                try { ok = i.IsDone(); } catch { ok = false; }
                log.Add((ok ? "Done: " : "Not done: ") + i.Title + (ok ? "." : " (the admin prompt was refused or it failed)."));
            }
            return log;
        }

        /// <summary>The pop-up's text (no UI here, so tests can check it).</summary>
        public static string Text(IList<SetupItem> items)
        {
            var sb = new StringBuilder("To play with others, HERO VR needs " + (items.Count == 1 ? "one thing" : items.Count + " things") + " on this PC:\n\n");
            foreach (var i in items) sb.Append("\u2022 ").Append(i.Title).Append("\n   ").Append(i.Why).Append("\n\n");
            sb.Append("Yes, Autosetup: the launcher does it for you (Windows asks once for permission).\nNo: skip it - you can still play, but friends may not be able to join you.");
            return sb.ToString();
        }
    }
}
