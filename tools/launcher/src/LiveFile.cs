// SM2 VR Launcher - 0.0.74 test 3: reading and changing a setting in the mod folder's live.txt (the file VR reads
// while it runs: "light_checks = off"). Made by Agent Mulder.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Sm2VrLauncher
{
    static class LiveFile
    {
        public static string PathIn(string root) { return Path.Combine(root ?? "", "live.txt"); }

        // The setting's name on a live.txt line ("light_checks = off  # note"), or null.
        static string NameOf(string line, out string value)
        {
            value = null;
            string s = line;
            int hash = s.IndexOf('#');
            if (hash >= 0) s = s.Substring(0, hash);
            s = s.Trim();
            if (s.Length == 0) return null;
            int eq = s.IndexOf('=');
            if (eq < 0) eq = s.IndexOf(' ');
            if (eq < 0) return s.ToLowerInvariant().TrimStart(new[] { '-' });
            value = s.Substring(eq + 1).Trim().ToLowerInvariant();
            return s.Substring(0, eq).Trim().ToLowerInvariant().TrimStart(new[] { '-' });
        }

        /// <summary>The value live.txt gives `key` (the last line naming it wins, as VR reads it), or null.</summary>
        public static string Get(string root, string key)
        {
            string found = null;
            try
            {
                string path = PathIn(root);
                if (!File.Exists(path)) return null;
                foreach (var line in File.ReadAllLines(path))
                {
                    string v;
                    if (string.Equals(NameOf(line, out v), key, StringComparison.OrdinalIgnoreCase)) found = v;
                }
            }
            catch { }
            return found;
        }

        /// <summary>An on/off setting: "on" and "0" are on, "off" and "1" are off (live_settings.py); none: `def`.</summary>
        public static bool On(string root, string key, bool def)
        {
            string v = Get(root, key);
            if (v == "off" || v == "1") return false;
            if (v == "on" || v == "0") return true;
            return def;
        }

        /// <summary>Sets `key = value` in live.txt: the lines naming it are changed, else one is added. False: not written.</summary>
        public static bool Set(string root, string key, string value)
        {
            try
            {
                string path = PathIn(root);
                var lines = File.Exists(path) ? new List<string>(File.ReadAllLines(path)) : new List<string>();
                bool done = false;
                for (int i = 0; i < lines.Count; ++i)
                {
                    string v;
                    if (!string.Equals(NameOf(lines[i], out v), key, StringComparison.OrdinalIgnoreCase)) continue;
                    lines[i] = key + " = " + value;
                    done = true;
                }
                if (!done) lines.Add(key + " = " + value);
                string tmp = path + ".tmp";
                File.WriteAllText(tmp, string.Join("\r\n", lines.ToArray()) + "\r\n", new UTF8Encoding(false));
                if (File.Exists(path)) File.Delete(path);
                File.Move(tmp, path);
                return true;
            }
            catch { return false; }
        }

        // 0.0.74 test 3: the two-GPU device-lost signature (the public 0.0.73 player's sessions 204133 and 225454:
        // RTX 5090 + AMD Radeon graphics; the game's "Device removed detected (0x887A002B" and the eye bridge's 3301).
        public static bool TwoGpuLostLine(string line)
        {
            return line != null && (line.IndexOf("0x887A002B", StringComparison.OrdinalIgnoreCase) >= 0 ||
                                    line.IndexOf("GPU eye bridge: 3301", StringComparison.Ordinal) >= 0);
        }

        // The settings the pop-up turns off (and OPTIONS > SAFETY CHECKS shows). 0.0.74 test 10 PICTURE 72: LIGHT CHECKS
        // only (STREAMER MODE and SL WATCH are gone).
        public static readonly string[] TwoGpuKeys = { "light_checks" };

        // 0.0.74 test 10 MONITOR MEMORY (streamer_view.hpp Memory): %APPDATA%\Spidy\monitor-copy.txt, which VR
        // writes "copy" in while the monitor shows the headset's copy (removed on a clean exit) and "lost" in after a
        // device-removed sign. "skip" makes every later session keep the monitor on the game's own view (0.0.72).
        // No setting: only a device loss writes it.
        public static string MonitorMemoryPath()
        {
            return Path.Combine(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Spidy"), "monitor-copy.txt");
        }

        /// <summary>The file's state word ("copy", "lost", "skip"), or "" (no file, unreadable or other text).</summary>
        public static string MonitorMemory(string path)
        {
            try
            {
                if (string.IsNullOrEmpty(path) || !File.Exists(path)) return "";
                string t = File.ReadAllText(path).TrimStart(new[] { ' ', '\t', '\r', '\n', '\uFEFF' });
                string w = t.Length >= 4 ? t.Substring(0, 4) : "";
                return w == "copy" || w == "lost" || w == "skip" ? w : "";
            }
            catch { return ""; }
        }

        /// <summary>Writes "skip" (the pop-up's Yes, or after a device loss with the copy on).</summary>
        public static bool MonitorSkip(string path)
        {
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                File.WriteAllText(path, "skip - the monitor's copy is skipped after a GPU device loss (0x887A002B); delete this file to try it again\r\n", new UTF8Encoding(false));
                return true;
            }
            catch { return false; }
        }

        /// <summary>After a VR session: if its console showed the device-lost signature (TwoGpuLostLine) while the
        /// copy was on ("copy" or "lost"), "skip" is written; true then. A session killed or crashed for another
        /// reason (no signature) changes nothing.</summary>
        public static bool MonitorSkipAfterSession(string path, bool deviceLostLine)
        {
            if (!deviceLostLine) return false;
            string m = MonitorMemory(path);
            return (m == "copy" || m == "lost") && MonitorSkip(path);
        }
    }
}
