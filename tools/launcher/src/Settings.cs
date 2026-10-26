// SM2 VR Launcher - settings stored as key=value lines in %APPDATA%\SM2 VR Launcher\settings.ini.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Sm2VrLauncher
{
    class Settings
    {
        public const string RepoPlaceholder = "YOUR-GITHUB-NAME/sm2-vr";
        /// <summary>The HERO VR releases. Used when the repository setting is blank or still the placeholder, so a launcher
        /// whose settings.ini or launcher.ini lost UpdateRepo still finds updates, the mod files and Revert (launcher build 8).</summary>
        public const string DefaultRepo = "lecutis7of9/HEROVR";
        public static bool IsPlaceholderRepo(string r)
        {
            r = (r ?? "").Trim();
            return r.Length == 0 || r.Equals(RepoPlaceholder, StringComparison.OrdinalIgnoreCase) || r.StartsWith("YOUR-", StringComparison.OrdinalIgnoreCase);
        }
        /// <summary>%APPDATA%\SM2 VR Launcher, or "launcher-data" beside the exe when launcher.ini says Portable=1
        /// (so a second copy of the launcher, e.g. for a test build, keeps fully separate settings and downloads).</summary>
        public static readonly string Folder = PickFolder();
        public static readonly string FilePath = Path.Combine(Folder, "settings.ini");

        static string PickFolder()
        {
            try
            {
                string ini = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launcher.ini");
                if (File.Exists(ini))
                    foreach (var raw in File.ReadAllLines(ini))
                    {
                        string l = raw.Trim().Replace(" ", "");
                        if (l.Equals("Portable=1", StringComparison.OrdinalIgnoreCase) || l.Equals("Portable=true", StringComparison.OrdinalIgnoreCase))
                            return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launcher-data");
                    }
            }
            catch { }
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "SM2 VR Launcher");
        }
        readonly Dictionary<string, string> values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        public static Settings Load()
        {
            var s = new Settings();
            // Defaults shipped next to the exe (launcher.ini), e.g. UpdateRepo=owner/name, then the user's own file.
            try
            {
                string exeDir = AppDomain.CurrentDomain.BaseDirectory;
                s.ReadFile(Path.Combine(exeDir, "launcher.ini"));
            }
            catch { }
            s.ReadFile(FilePath);
            return s;
        }

        void ReadFile(string path)
        {
            if (!File.Exists(path)) return;
            foreach (var raw in File.ReadAllLines(path, Encoding.UTF8))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith("#") || line.StartsWith(";") || line.StartsWith("[")) continue;
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                values[line.Substring(0, eq).Trim()] = line.Substring(eq + 1).Trim();
            }
        }

        public void Save()
        {
            try
            {
                Directory.CreateDirectory(Folder);
                var sb = new StringBuilder();
                sb.AppendLine("# SM2 VR Launcher settings. Safe to edit while the launcher is closed.");
                sb.AppendLine("# UpdateRepo = your GitHub owner/repository for CHECK FOR UPDATE (see PUBLISHING.md).");
                var keys = new List<string>(values.Keys);
                keys.Sort(StringComparer.OrdinalIgnoreCase);
                foreach (var k in keys) sb.AppendLine(k + "=" + values[k]);
                string tmp = FilePath + ".tmp";
                File.WriteAllText(tmp, sb.ToString(), new UTF8Encoding(false));
                if (File.Exists(FilePath)) File.Replace(tmp, FilePath, null);
                else File.Move(tmp, FilePath);
            }
            catch { /* settings are a convenience; never block the launcher on them */ }
        }

        public string Get(string key, string fallback)
        {
            string v;
            return values.TryGetValue(key, out v) ? v : fallback;
        }
        public void Set(string key, string value)
        {
            if (value == null) values.Remove(key); else values[key] = value.Replace("\r", " ").Replace("\n", " ");
        }
        public bool GetBool(string key, bool fallback)
        {
            string v = Get(key, null);
            if (v == null) return fallback;
            return v == "1" || v.Equals("true", StringComparison.OrdinalIgnoreCase) || v.Equals("yes", StringComparison.OrdinalIgnoreCase);
        }
        public void SetBool(string key, bool v) { Set(key, v ? "1" : "0"); }
        public int GetInt(string key, int fallback)
        {
            int v;
            return int.TryParse(Get(key, ""), out v) ? v : fallback;
        }

        public string UpdateRepo
        {
            get { string r = Get("UpdateRepo", ""); return IsPlaceholderRepo(r) ? DefaultRepo : r; }
            set { Set("UpdateRepo", value); }
        }
        public bool RepoConfigured
        {
            get
            {
                string r = UpdateRepo.Trim();
                return r.Length > 2 && r.Contains("/") && !r.Equals(RepoPlaceholder, StringComparison.OrdinalIgnoreCase)
                       && !r.StartsWith("YOUR-", StringComparison.OrdinalIgnoreCase);
            }
        }
        public string RepoUrl { get { return "https://github.com/" + UpdateRepo.Trim(); } }
        public int KeepBackups { get { return Math.Max(1, Math.Min(20, GetInt("KeepBackups", 3))); } }
        /// <summary>User settings files that a revert keeps (unless asked to restore that version's).</summary>
        public string[] PreservePatterns
        {
            get
            {
                return Get("PreserveFiles", "live.txt;next-launch.txt;bhaptics-app.local;*.local;*gesture*.txt;*gesture*.json;*gestures*.ini")
                    .Split(new[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
            }
        }
    }
}
