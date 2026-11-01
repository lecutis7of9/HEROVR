// HERO VR 0.0.76 SM1 HERO SUITS - the launcher runs tools\sm1_suits\sm1suits.py with the mod's own Python on the
// player's PC: it fits our ten heroes (Models\SM1Suits\*.hsm1) to Spider-Man Remastered's own suit model and adds
// them as suits through the game's toc, the way Overstrike suit mods do (Overstrike / ALERT by Tkachov, GPL-3.0).
// It writes only asset_archive\HeroVR\ and asset_archive\toc.herovr-sm1.bak (the untouched toc), never iilyn's
// SpidyVR files; Remove puts the toc back byte for byte. Made by Agent Mulder.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;

namespace Sm2VrLauncher
{
    static class Sm1Suits
    {
        public const string Script = @"tools\sm1_suits\sm1suits.py";
        public const string Packs = @"Models\SM1Suits";

        public static string ScriptPath(string root) { return Path.Combine(root, Script); }
        public static string PacksPath(string root) { return Path.Combine(root, Packs); }
        public static string LogPath { get { return Path.Combine(Sm1.AddonDataFolder, "sm1suits.log"); } }
        public static string DumpFolder { get { return Path.Combine(Sm1.AddonDataFolder, "sm1-dump"); } }

        /// <summary>The game folder (the one holding asset_archive) from Spider-Man.exe's path, or null.</summary>
        public static string GameFolder(string gameExe)
        {
            try
            {
                if (string.IsNullOrEmpty(gameExe)) return null;
                string d = Path.GetDirectoryName(gameExe);
                return File.Exists(Path.Combine(Path.Combine(d, "asset_archive"), "toc")) ? d : null;
            }
            catch { return null; }
        }

        /// <summary>Why the tool cannot run here, or null when it can.</summary>
        public static string Missing(string root, string gameExe)
        {
            if (!ModFiles.PythonPresent(root)) return "the mod's Python (python\\python.exe) is missing: Repair gets it";
            if (!File.Exists(ScriptPath(root))) return Script + " is missing: Repair gets it";
            if (!Directory.Exists(PacksPath(root))) return Packs + " (the converted heroes) is missing";
            if (GameFolder(gameExe) == null) return "Spider-Man 1's asset_archive\\toc was not found (press Browse for the game)";
            return null;
        }

        public class Result
        {
            public bool Ok;
            public string Error = "";
            public Dictionary<string, object> Data = new Dictionary<string, object>();
        }

        /// <summary>Runs one command (status, install, revert, dump) to the end on the calling (background) thread.
        /// Every line goes to `line`; the tool's last line is RESULT {json}.</summary>
        public static Result Run(string root, string gameExe, string command, Action<string> line)
        {
            var res = new Result();
            string why = Missing(root, gameExe);
            if (why != null) { res.Error = why; return res; }
            if (command != "status" && Sm1.GameRunning()) { res.Error = "close Spider-Man first: its files cannot change while it runs"; return res; }
            try { Directory.CreateDirectory(Sm1.AddonDataFolder); } catch { }
            var args = new StringBuilder();
            args.Append("-B -X utf8 ").Append(Q(ScriptPath(root))).Append(' ').Append(command)
                .Append(" --game ").Append(Q(GameFolder(gameExe)))
                .Append(" --packs ").Append(Q(PacksPath(root)))
                .Append(" --log ").Append(Q(LogPath));
            if (command == "dump") args.Append(" --out ").Append(Q(DumpFolder));
            var psi = new ProcessStartInfo(ModFiles.BundledPython(root), args.ToString())
            {
                UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true,
                WorkingDirectory = root, StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8
            };
            string last = null;
            try
            {
                using (var p = new Process { StartInfo = psi })
                {
                    p.OutputDataReceived += (s, e) =>
                    {
                        if (e.Data == null) return;
                        if (e.Data.StartsWith("RESULT ", StringComparison.Ordinal)) last = e.Data.Substring(7);
                        else if (line != null) line(e.Data);
                    };
                    p.ErrorDataReceived += (s, e) => { if (e.Data != null && line != null) line(e.Data); };
                    p.Start();
                    p.BeginOutputReadLine();
                    p.BeginErrorReadLine();
                    p.WaitForExit();
                    p.WaitForExit(2000);
                }
            }
            catch (Exception ex) { res.Error = "could not start the mod's Python: " + ex.Message; return res; }
            if (last == null) { res.Error = "the tool ended without a result (see " + LogPath + ")"; return res; }
            try
            {
                var d = new JavaScriptSerializer { MaxJsonLength = 64 * 1024 * 1024 }.Deserialize<Dictionary<string, object>>(last);
                res.Ok = d.ContainsKey("ok") && d["ok"] is bool && (bool)d["ok"];
                if (d.ContainsKey("error") && d["error"] != null) res.Error = d["error"].ToString();
                var r = d.ContainsKey("result") ? d["result"] as Dictionary<string, object> : null;
                if (r != null) res.Data = r;
            }
            catch (Exception ex) { res.Error = "unreadable result: " + ex.Message; }
            return res;
        }

        /// <summary>A one-line summary of a status result for the Spider-Man 1 page.</summary>
        public static string Describe(Result r)
        {
            if (!r.Ok) return "Hero suits: " + r.Error + ".";
            object v;
            bool installed = r.Data.TryGetValue("installed", out v) && v is bool && (bool)v;
            if (!installed) return "Hero suits: not installed. The game's files are as Steam put them.";
            var names = new List<string>();
            if (r.Data.TryGetValue("suits", out v) && v is System.Collections.IEnumerable)
                foreach (var s in (System.Collections.IEnumerable)v) names.Add(s.ToString().Replace("herovr_", ""));
            bool matches = r.Data.TryGetValue("toc_matches_install", out v) && v is bool && (bool)v;
            return "Hero suits: " + names.Count + " installed (" + string.Join(", ", names.ToArray()) + ")" +
                   (matches ? "; pick them in the game's Suits menu." : "; the game's toc changed since (a game update?): press Install again.");
        }

        static string Q(string s) { return "\"" + s.Replace("\"", "") + "\""; }
    }
}
