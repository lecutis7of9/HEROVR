// HERO VR 0.0.76 AUTO-SCAN - the launcher runs tools\sm2_suits\sm2scan.py with the mod's own Python on the player's
// PC, in the background, the first time (or when the sm2scan.json in the mod folder is missing, from an older
// sm2scan without the loadout / material ids, or older than the game's toc), so nobody runs it by hand. It only
// READS the game's toc and archives and writes <mod folder>\sm2scan.json (through sm2scan.json.tmp, renamed when
// whole), where the in-game suit probe looks first. Never blocks PLAY VR: a background thread, killed after
// TimeoutMs. If it fails the launcher says so in one line and the probe says it has no new scan. Made by Agent Mulder.
using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;

namespace Sm2VrLauncher
{
    static class SuitScan
    {
        public const string Script = @"tools\sm2_suits\sm2scan.py";
        public const string FileName = "sm2scan.json";
        public const int TimeoutMs = 240000; // 4 minutes, then the scan is stopped (the game is never waited on)
        static int running; // 1 while a scan runs (one at a time)
        static string doneFor; // the game folder scanned (or tried) this session: once a session

        public static string OutPath(string root) { return Path.Combine(root, FileName); }

        /// <summary>Why the scan should run now, or null: no file, an older format (no "sm2scan 2", no loadout or
        /// material ids), or older than the game's toc (a game update).</summary>
        public static string Why(string root, string gameDir)
        {
            string path = OutPath(root);
            if (!File.Exists(path)) return "no " + FileName + " yet";
            try
            {
                string text = File.ReadAllText(path, Encoding.UTF8);
                if (text.IndexOf("\"sm2scan 2\"", StringComparison.Ordinal) < 0 ||
                    text.IndexOf("\"loadout\"", StringComparison.Ordinal) < 0 ||
                    text.IndexOf("\"materials\"", StringComparison.Ordinal) < 0)
                    return "the " + FileName + " there is from an older sm2scan (no loadout / material ids)";
            }
            catch (Exception ex) { return "the " + FileName + " there could not be read (" + ex.Message + ")"; }
            try
            {
                string toc = Path.Combine(gameDir, "toc");
                if (File.Exists(toc) && File.GetLastWriteTimeUtc(toc) > File.GetLastWriteTimeUtc(path))
                    return "the game was updated since the last scan";
            }
            catch { }
            return null;
        }

        /// <summary>Starts the scan in the background when it is needed (at most once a session per game folder).
        /// line: one line for the launcher's log (any thread). Returns at once.</summary>
        public static void StartIfNeeded(string root, string gameExe, Action<string> line)
        {
            try
            {
                if (string.IsNullOrEmpty(root) || string.IsNullOrEmpty(gameExe)) return;
                string gameDir = Path.GetDirectoryName(gameExe);
                if (string.IsNullOrEmpty(gameDir) || !File.Exists(Path.Combine(gameDir, "toc"))) return;
                if (string.Equals(doneFor, gameDir, StringComparison.OrdinalIgnoreCase)) return;
                string script = Path.Combine(root, Script), python = ModFiles.BundledPython(root);
                if (!File.Exists(script) || !File.Exists(python)) return; // an older install: nothing to run
                string why = Why(root, gameDir);
                doneFor = gameDir;
                if (why == null) return;
                if (Interlocked.CompareExchange(ref running, 1, 0) != 0) return;
                if (line != null) line("Scanning your suits (first time only; " + why + ") - in the background, PLAY VR does not wait.");
                var t = new Thread(() => Run(root, gameDir, python, script, line)) { IsBackground = true, Name = "sm2scan" };
                t.Start();
            }
            catch (Exception ex)
            {
                Interlocked.Exchange(ref running, 0);
                if (line != null) line("Suit scan could not start: " + ex.Message + " (the suit probe just says it has no new scan).");
            }
        }

        static void Run(string root, string gameDir, string python, string script, Action<string> line)
        {
            string tmp = OutPath(root) + ".tmp";
            string said = null, err = null;
            try
            {
                try { if (File.Exists(tmp)) File.Delete(tmp); } catch { }
                var psi = new ProcessStartInfo(python, "-B -X utf8 " + Q(script) + " " + Q(gameDir) + " " + Q(tmp))
                {
                    UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true,
                    WorkingDirectory = root, StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8
                };
                var sw = Stopwatch.StartNew();
                using (var p = new Process { StartInfo = psi })
                {
                    p.OutputDataReceived += (s, e) => { if (e.Data != null && e.Data.StartsWith("sm2scan:", StringComparison.Ordinal)) said = e.Data; };
                    p.ErrorDataReceived += (s, e) => { if (e.Data != null && e.Data.Trim().Length > 0) err = e.Data.Trim(); };
                    p.Start();
                    try { p.PriorityClass = ProcessPriorityClass.BelowNormal; } catch { } // the game keeps the CPU
                    p.BeginOutputReadLine();
                    p.BeginErrorReadLine();
                    if (!p.WaitForExit(TimeoutMs))
                    {
                        try { p.Kill(); } catch { }
                        try { p.WaitForExit(5000); } catch { }
                        Fail(line, "it took longer than " + (TimeoutMs / 60000) + " minutes and was stopped");
                        return;
                    }
                    p.WaitForExit(2000);
                    if (p.ExitCode != 0 || !File.Exists(tmp) || new FileInfo(tmp).Length < 2)
                    {
                        Fail(line, "sm2scan ended with code " + p.ExitCode + (err != null ? ": " + err : ""));
                        return;
                    }
                }
                string dest = OutPath(root);
                if (File.Exists(dest)) File.Replace(tmp, dest, null);
                else File.Move(tmp, dest);
                if (line != null)
                    line("Suits scanned in " + (int)Math.Round(sw.Elapsed.TotalSeconds) + " s: " +
                         (said != null ? said.Substring(8).Trim() : dest) + " (the suit probe reads it, even if the game is already running).");
            }
            catch (Exception ex) { Fail(line, ex.Message); }
            finally
            {
                try { if (File.Exists(tmp)) File.Delete(tmp); } catch { }
                Interlocked.Exchange(ref running, 0);
            }
        }

        static void Fail(Action<string> line, string why)
        {
            if (line != null) line("Suit scan failed: " + why + ". The game is not affected; the suit probe just says it has no new scan.");
        }

        static string Q(string s) { return "\"" + s + "\""; }
    }
}
