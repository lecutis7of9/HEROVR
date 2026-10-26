// HERO VR launcher - opt-in reports (Version 0.0.6). Made by Agent Mulder.
//
// Nothing here sends anything by itself. Every report is a GitHub issue page opened in the player's browser,
// filled in already, that the player reads and then submits (or closes). There is no token, key or server in
// the launcher. Personal details are taken out first (Privacy.Scrub): user names, the PC's name, home folders,
// e-mail addresses, IP addresses and Steam IDs.
//
//  - Crash: when VR starts, telemetry\session-open.txt is written; when it ends the launcher looks for the
//    game's crash dumps (Windows Error Reporting's AppCrash_Spider-Man2.exe folders and *.dmp files newer than
//    the session) and the session report's "device_removed". A crash found, or a session the launcher never saw
//    end (the launcher or the PC went down), is asked about ONCE at the next launch: "Send this crash report
//    to help fix it?" (Send report / Not now / Don't ask again).
//  - Report a bug: a short description and, if ticked, the mod version, GPU and headset.
//  - Play stats: OFF unless the player turns it on (asked once at the first run). After each VR session a
//    summary is kept (fps average and low, stalls, heroes and features used, versions, GPU, headset), at most
//    the last 10, and Send play stats (Reports) opens one issue with all of them.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace Sm2VrLauncher
{
    static class Privacy
    {
        static readonly Regex WinHome = new Regex(@"(?i)\b([a-z]:\\+(?:users|documents and settings)\\+)[^\\/\r\n""<>|]+");
        static readonly Regex UnixHome = new Regex(@"(?i)(/(?:home|users)/)[^/\s""]+");
        static readonly Regex Email = new Regex(@"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}");
        static readonly Regex Ipv4 = new Regex(@"\b(?:\d{1,3}\.){3}\d{1,3}\b");
        static readonly Regex SteamId = new Regex(@"\b7656119\d{10}\b");

        /// <summary>The text with personal details replaced: home folders become %USERPROFILE%, the Windows user
        /// name &lt;user&gt;, the PC's name &lt;pc&gt;, e-mail addresses &lt;email&gt;, IPs &lt;ip&gt;, Steam IDs &lt;steamid&gt;.</summary>
        public static string Scrub(string s)
        {
            if (string.IsNullOrEmpty(s)) return s ?? "";
            try
            {
                string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                if (!string.IsNullOrEmpty(home) && home.Length > 3)
                    s = Regex.Replace(s, Regex.Escape(home), "%USERPROFILE%", RegexOptions.IgnoreCase);
            }
            catch { }
            s = WinHome.Replace(s, "$1<user>");
            s = UnixHome.Replace(s, "$1<user>");
            s = Email.Replace(s, "<email>");
            s = SteamId.Replace(s, "<steamid>");
            s = Ipv4.Replace(s, m => VersionLike.FourPart(m.Value) ? m.Value : "<ip>");
            s = Word(s, SafeEnv(() => Environment.UserName), "<user>");
            s = Word(s, SafeEnv(() => Environment.MachineName), "<pc>");
            return s;
        }
        static string SafeEnv(Func<string> f) { try { return f(); } catch { return null; } }
        static string Word(string s, string w, string with)
        {
            if (string.IsNullOrEmpty(w) || w.Length < 3) return s;   // "a" or "pc" would eat real words
            return Regex.Replace(s, @"(?<![A-Za-z0-9])" + Regex.Escape(w) + @"(?![A-Za-z0-9])", with, RegexOptions.IgnoreCase);
        }
    }

    // Four-part version numbers (0.0.6.2, Windows 10.0.22631.0) are not IP addresses.
    static class VersionLike
    {
        public static bool FourPart(string v)
        {
            var parts = v.Split(new[] { '.' });
            if (parts.Length != 4) return false;
            if (parts.Any(p => int.Parse(p, CultureInfo.InvariantCulture) > 255)) return true;   // 10.0.22631.0: no IP has a part over 255
            return parts.Take(3).All(p => p.Length == 1);
        }
    }

    class CrashFinding
    {
        public string Kind;          // what was found, in one line
        public string Body;          // the issue text (already scrubbed)
    }

    static class Telemetry
    {
        public const string IssueRepo = "lecutis7of9/HEROVR";
        public const int MaxUrl = 7000;             // GitHub refuses very long new-issue links; stay well under
        public const int MaxStatsKept = 10;
        public const string KeyAsked = "Telemetry.Asked", KeyStats = "Telemetry.ShareStats", KeyCrash = "Telemetry.OfferCrashReports";
        public static readonly string[] Heroes = { "iron_man", "daredevil", "hulk" };
        public static readonly string[] Features = { "fly_mode", "invincible", "supersonic", "web_physics", "flips", "arcade_webs",
                                                     "trigger_webs", "stand_on_walls", "hero_height", "hero_damage", "smooth_turn" };

        public static string Folder { get { return Path.Combine(Settings.Folder, "telemetry"); } }
        static string Marker { get { return Path.Combine(Folder, "session-open.txt"); } }
        static string CrashFile { get { return Path.Combine(Folder, "crash-pending.txt"); } }
        static string LauncherCrashFile { get { return Path.Combine(Folder, "launcher-crash-pending.txt"); } }
        static string StatsFile { get { return Path.Combine(Folder, "stats-pending.txt"); } }
        const string StatsSeparator = "\n----\n";

        // ---- the GitHub issue link -----------------------------------------------------------------------

        /// <summary>A new-issue link on the HERO VR repository with the title and text filled in. The text is
        /// cut (whole lines, with a note) so the link stays under MaxUrl.</summary>
        public static string IssueUrl(string title, string body)
        {
            string head = "https://github.com/" + IssueRepo + "/issues/new?title=" + Uri.EscapeDataString(Privacy.Scrub(title)) + "&body=";
            body = Privacy.Scrub(body ?? "");
            const string cut = "\n\n(cut to fit the link; the rest is in the launcher's reports)";
            string url = body.Length < MaxUrl ? head + Uri.EscapeDataString(body) : null;
            if (url != null && url.Length <= MaxUrl) return url;
            var lines = new List<string>(body.Replace("\r", "").Split(new[] { '\n' }));
            // The escaped text is never shorter than the text: drop what cannot fit before escaping anything
            // (Uri.EscapeDataString also refuses very long strings on older .NET).
            int raw = head.Length + cut.Length;
            int keep = 0;
            while (keep < lines.Count && raw + lines[keep].Length + 1 <= MaxUrl) { raw += lines[keep].Length + 1; keep++; }
            if (keep < lines.Count) lines.RemoveRange(keep, lines.Count - keep);
            while (lines.Count > 1)
            {
                lines.RemoveAt(lines.Count - 1);
                url = head + Uri.EscapeDataString(string.Join("\n", lines) + cut);
                if (url.Length <= MaxUrl) return url;
            }
            string one = body.Length > 1000 ? body.Substring(0, 1000) : body;
            return head + Uri.EscapeDataString(one + cut);
        }

        public static void Open(string title, string body)
        {
            SystemInfo.OpenUrl(IssueUrl(title, body));
        }

        // ---- what every report starts with --------------------------------------------------------------

        public static string Header(string root, bool withSystem)
        {
            var sb = new StringBuilder();
            sb.AppendLine("Mod: " + SafeCall(() => SystemInfo.PortVersionLong(root), "unknown"));
            sb.AppendLine("Launcher: " + LauncherInfo.Version + " (build " + LauncherInfo.Build + ")");
            if (withSystem)
            {
                sb.AppendLine("Windows: " + SafeCall(() => Environment.OSVersion.Version.ToString(), "?") + (Environment.Is64BitOperatingSystem ? " 64-bit" : ""));
                sb.AppendLine("GPU: " + SafeCall(Gpu, "unknown"));
                sb.AppendLine("VR runtime: " + SafeCall(() => RuntimeName(SystemInfo.ActiveOpenXr()), "unknown"));
            }
            return sb.ToString();
        }

        static string RuntimeName(string manifest)
        {
            if (string.IsNullOrEmpty(manifest)) return "none found";
            string f = Path.GetFileNameWithoutExtension(manifest);   // the manifest's name only, never its folder
            return string.IsNullOrEmpty(f) ? "unknown" : f;
        }

        static volatile string gpuCache;
        static System.Threading.Thread gpuThread;
        static readonly object gpuLock = new object();

        /// <summary>The graphics card name(s) from WMI. WMI can be slow or stuck on some PCs, so it is read on its own
        /// thread and waited for at most 2 s; the launcher warms it at start, so a report normally has it at once.</summary>
        public static string Gpu()
        {
            if (gpuCache != null) return gpuCache;
            WarmGpu();
            var t = gpuThread;
            if (t != null) t.Join(2000);
            return gpuCache ?? "unknown";
        }

        public static void WarmGpu()
        {
            lock (gpuLock)
            {
                if (gpuCache != null || gpuThread != null) return;
                gpuThread = new System.Threading.Thread(() =>
                {
                    var names = new List<string>();
                    try
                    {
                        using (var s = new System.Management.ManagementObjectSearcher("SELECT Name FROM Win32_VideoController"))
                        {
                            s.Options.Timeout = TimeSpan.FromSeconds(5);
                            foreach (var o in s.Get()) { var n = Convert.ToString(o["Name"]); if (!string.IsNullOrEmpty(n) && !n.Contains("Basic Render") && !names.Contains(n)) names.Add(n); }
                        }
                    }
                    catch { }
                    gpuCache = names.Count > 0 ? string.Join(" + ", names) : "unknown";
                }) { IsBackground = true, Name = "gpu-name" };
                gpuThread.Start();
            }
        }

        static T SafeCall<T>(Func<T> f, T fallback) { try { var v = f(); return v == null ? fallback : v; } catch { return fallback; } }

        // ---- VR session bookkeeping ---------------------------------------------------------------------

        public static void SessionStarted()
        {
            try
            {
                Directory.CreateDirectory(Folder);
                File.WriteAllText(Marker, DateTime.UtcNow.Ticks.ToString(CultureInfo.InvariantCulture));
            }
            catch { }
        }

        /// <summary>VR did not start after all: no session to check.</summary>
        public static void SessionAborted() { try { File.Delete(Marker); } catch { } }

        static DateTime? MarkerStart()
        {
            try
            {
                long t;
                if (File.Exists(Marker) && long.TryParse(File.ReadAllText(Marker).Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out t))
                    return new DateTime(t, DateTimeKind.Utc);
            }
            catch { }
            return null;
        }

        /// <summary>Called when the VR session ends (any thread; does file work only). Looks for a crash and, if the
        /// player shares play stats, keeps this session's summary. Never throws.</summary>
        public static void SessionEnded(string root, int exitCode, Settings settings, Action<string> log)
        {
            try
            {
                DateTime start = MarkerStart() ?? DateTime.UtcNow.AddHours(-12);
                string reportPath = NewestReport(root, start);
                Dictionary<string, object> report = reportPath == null ? null : ReadReportFields(reportPath);
                var evidence = CrashEvidence(start, report);
                if (evidence.Count > 0 && settings.GetBool(KeyCrash, true))
                {
                    var body = new StringBuilder();
                    body.AppendLine("**What happened** (optional, add a line):");
                    body.AppendLine();
                    body.AppendLine("**Found by the launcher**");
                    foreach (var e in evidence) body.AppendLine("- " + e);
                    body.AppendLine("- VR session exit code: " + exitCode);
                    body.AppendLine();
                    body.AppendLine("```");
                    body.Append(Header(root, true));
                    if (report != null) body.Append(SummaryLines(report, false));
                    body.AppendLine("```");
                    body.AppendLine("Full logs (optional): Reports > Collect reports in the launcher makes reports\\send-me.zip; drag it here.");
                    Directory.CreateDirectory(Folder);
                    File.WriteAllText(CrashFile, "The game crashed during the last VR session.\n" + Privacy.Scrub(body.ToString()));
                    if (log != null) log("A crash was found; the launcher asks whether to send it the next time it starts.");
                }
                if (settings.GetBool(KeyStats, false) && report != null)
                {
                    KeepStats(Privacy.Scrub("Session " + DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm", CultureInfo.InvariantCulture) + " UTC\n" +
                                            Header(root, false) + SummaryLines(report, true)));
                    if (log != null) log("Play stats for this session were saved (Reports > Send play stats shares them; nothing is sent without your click).");
                }
            }
            catch (Exception ex) { Program.LogError(ex); }
            finally { try { File.Delete(Marker); } catch { } }
        }

        /// <summary>At startup: a crash found last time, a VR session that never ended (the launcher or the PC
        /// went down while it ran: only when the game is not running now), or a launcher crash.</summary>
        public static CrashFinding PendingCrash(string root, Settings settings)
        {
            try
            {
                if (!settings.GetBool(KeyCrash, true)) { Clear(); return null; }
                if (File.Exists(LauncherCrashFile))
                {
                    string t = File.ReadAllText(LauncherCrashFile);
                    return new CrashFinding { Kind = "The launcher itself crashed last time.", Body = t };
                }
                if (File.Exists(CrashFile))
                {
                    string t = File.ReadAllText(CrashFile);
                    int nl = t.IndexOf('\n');
                    return new CrashFinding { Kind = nl > 0 ? t.Substring(0, nl) : "The game crashed.", Body = nl > 0 ? t.Substring(nl + 1) : t };
                }
                var start = MarkerStart();
                if (start != null && !SystemInfo.GameRunning())
                {
                    string reportPath = NewestReport(root, start.Value);
                    var report = reportPath == null ? null : ReadReportFields(reportPath);
                    var evidence = CrashEvidence(start.Value, report);
                    var body = new StringBuilder();
                    body.AppendLine("**What happened** (optional, add a line):");
                    body.AppendLine();
                    body.AppendLine("**Found by the launcher**");
                    body.AppendLine("- The VR session did not end cleanly: the launcher or the PC closed while it ran.");
                    foreach (var e in evidence) body.AppendLine("- " + e);
                    body.AppendLine();
                    body.AppendLine("```");
                    body.Append(Header(root, true));
                    if (report != null) body.Append(SummaryLines(report, false));
                    body.AppendLine("```");
                    try { File.Delete(Marker); } catch { }
                    return new CrashFinding { Kind = evidence.Count > 0 ? "The game crashed during the last VR session." : "The last VR session did not end cleanly.", Body = Privacy.Scrub(body.ToString()) };
                }
            }
            catch (Exception ex) { Program.LogError(ex); }
            return null;
        }

        /// <summary>The crash was answered (sent, not now, or don't ask again): it is not asked about again.</summary>
        public static void Clear()
        {
            try { File.Delete(CrashFile); } catch { }
            try { File.Delete(LauncherCrashFile); } catch { }
        }

        /// <summary>From Program's crash handler: the launcher's own crash, asked about at the next start.</summary>
        public static void NoteLauncherCrash(Exception ex)
        {
            try
            {
                Directory.CreateDirectory(Folder);
                string first = ex == null ? "unknown error" : ex.GetType().Name + ": " + ex.Message;
                string stack = ex == null || ex.StackTrace == null ? "" : string.Join("\n", ex.StackTrace.Replace("\r", "").Split(new[] { '\n' }).Take(12));
                File.WriteAllText(LauncherCrashFile, Privacy.Scrub("**What happened** (optional, add a line):\n\n**The launcher crashed**\n```\nLauncher: " +
                    LauncherInfo.Version + " (build " + LauncherInfo.Build + ")\n" + first + "\n" + stack + "\n```\n"));
            }
            catch { }
        }

        // ---- crash evidence -------------------------------------------------------------------------------

        /// <summary>Signs of a game crash since `start` (UTC): Windows Error Reporting's AppCrash folders for the
        /// game, crash dumps (*.dmp) in the game's usual folders, and the session report's device_removed.</summary>
        public static List<string> CrashEvidence(DateTime start, Dictionary<string, object> report)
        {
            var found = new List<string>();
            DateTime since = start.AddMinutes(-1);
            string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            string common = Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
            string roaming = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
            string docs = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
            int wer = 0;
            foreach (var baseDir in new[] { local, common })
                foreach (var sub in new[] { "ReportArchive", "ReportQueue" })
                {
                    string d = Path.Combine(baseDir ?? "", "Microsoft", "Windows", "WER", sub);
                    try
                    {
                        if (!Directory.Exists(d)) continue;
                        foreach (var dir in new DirectoryInfo(d).EnumerateDirectories("AppCrash_" + SystemInfo.GameExe + "*"))
                            if (dir.CreationTimeUtc >= since || dir.LastWriteTimeUtc >= since) wer++;
                    }
                    catch { }
                }
            if (wer > 0) found.Add("Windows Error Reporting recorded a crash of " + SystemInfo.GameExe + " (" + wer + " report" + (wer == 1 ? "" : "s") + ").");
            int dumps = 0;
            foreach (var d in new[] { Path.Combine(local ?? "", "Insomniac Games"), Path.Combine(roaming ?? "", "Insomniac Games"),
                                      Path.Combine(docs ?? "", "Marvel's Spider-Man 2"), Path.Combine(local ?? "", "CrashDumps") })
                dumps += CountDumps(d, since, 0);
            if (dumps > 0) found.Add("The game wrote " + dumps + " crash dump" + (dumps == 1 ? "" : "s") + " (*.dmp; not attached).");
            if (report != null)
            {
                object dr;
                if (report.TryGetValue("device_removed", out dr) && dr != null && Convert.ToString(dr).Length > 0)
                    found.Add("The graphics driver reset (DEVICE_REMOVED): " + Short(Convert.ToString(dr), 160));
            }
            return found;
        }

        static int CountDumps(string dir, DateTime since, int depth)
        {
            int n = 0;
            try
            {
                if (!Directory.Exists(dir) || depth > 3) return 0;
                var di = new DirectoryInfo(dir);
                foreach (var f in di.EnumerateFiles("*.dmp").Take(500))
                    if (f.LastWriteTimeUtc >= since && (dir.IndexOf("CrashDumps", StringComparison.OrdinalIgnoreCase) < 0 || f.Name.StartsWith(SystemInfo.GameProcess, StringComparison.OrdinalIgnoreCase))) n++;
                foreach (var d in di.EnumerateDirectories().Take(50)) n += CountDumps(d.FullName, since, depth + 1);
            }
            catch { }
            return n;
        }

        // ---- the session report (reports\game-vr-*.json) -------------------------------------------------

        static string NewestReport(string root, DateTime since)
        {
            try
            {
                string d = Path.Combine(root ?? "", "reports");
                if (!Directory.Exists(d)) return null;
                var f = new DirectoryInfo(d).GetFiles("game-vr-*.json").Where(x => x.LastWriteTimeUtc >= since.AddMinutes(-1))
                    .OrderByDescending(x => x.LastWriteTimeUtc).FirstOrDefault();
                return f == null || f.Length > 400L * 1024 * 1024 ? null : f.FullName;
            }
            catch { return null; }
        }

        static readonly string[] ReportKeys = { "port_version", "version", "session_fps", "stream_stalls", "play_rate", "xr_runtime", "game_adapters",
                                                "vr_settings", "live_changes", "device_removed", "game_exited", "error", "render_scale", "supersample" };

        /// <summary>Only the small top-level fields of the report (it can be tens of MB of samples): each is cut
        /// out of the text (the report is written with indent=2) and parsed on its own.</summary>
        public static Dictionary<string, object> ReadReportFields(string path)
        {
            var result = new Dictionary<string, object>();
            string text;
            try { text = File.ReadAllText(path); } catch { return null; }
            var js = new JavaScriptSerializer { MaxJsonLength = 16 * 1024 * 1024 };
            foreach (var key in ReportKeys)
            {
                string needle = "\n  \"" + key + "\": ";
                int at = text.IndexOf(needle, StringComparison.Ordinal);
                if (at < 0) continue;
                int s = at + needle.Length;
                int e = ValueEnd(text, s);
                if (e <= s || e - s > 2 * 1024 * 1024) continue;
                try { result[key] = js.DeserializeObject(text.Substring(s, e - s)); } catch { }
            }
            return result;
        }

        static int ValueEnd(string t, int s)
        {
            if (s >= t.Length) return s;
            char c = t[s];
            if (c == '{' || c == '[')
            {
                int depth = 0; bool str = false;
                for (int i = s; i < t.Length; i++)
                {
                    char ch = t[i];
                    if (str) { if (ch == '\\') i++; else if (ch == '"') str = false; continue; }
                    if (ch == '"') str = true;
                    else if (ch == '{' || ch == '[') depth++;
                    else if ((ch == '}' || ch == ']') && --depth == 0) return i + 1;
                }
                return s;
            }
            if (c == '"')
            {
                for (int i = s + 1; i < t.Length; i++) { if (t[i] == '\\') i++; else if (t[i] == '"') return i + 1; }
                return s;
            }
            int j = s;
            while (j < t.Length && t[j] != ',' && t[j] != '\n' && t[j] != '}') j++;
            return j;
        }

        static object Field(Dictionary<string, object> d, string key, string sub)
        {
            object v;
            if (d == null || !d.TryGetValue(key, out v) || v == null) return null;
            if (sub == null) return v;
            var m = v as IDictionary<string, object>;
            object w;
            return m != null && m.TryGetValue(sub, out w) ? w : null;
        }
        static string Num(object v, string fmt)
        {
            if (v == null) return "?";
            try { return Convert.ToDouble(v, CultureInfo.InvariantCulture).ToString(fmt, CultureInfo.InvariantCulture); } catch { return "?"; }
        }
        static string Short(string s, int n) { s = (s ?? "").Replace("\r", " ").Replace("\n", " "); return s.Length > n ? s.Substring(0, n) + "..." : s; }

        /// <summary>The per-session summary: no paths, no names, only numbers, on/off names from a fixed list, the
        /// GPU and the headset model.</summary>
        public static string SummaryLines(Dictionary<string, object> r, bool forStats)
        {
            var sb = new StringBuilder();
            object pv = Field(r, "port_version", null);
            if (pv != null) sb.AppendLine("Build: " + Short(Convert.ToString(pv), 40));
            object secs = Field(r, "session_fps", "tracked_seconds") ?? Field(r, "play_rate", "seconds");
            if (secs != null) sb.AppendLine("Played: " + Num(Convert.ToDouble(secs, CultureInfo.InvariantCulture) / 60.0, "0.0") + " min");
            if (Field(r, "session_fps", null) != null)
                sb.AppendLine("Game fps: average " + Num(Field(r, "session_fps", "average"), "0.0") + ", low " + Num(Field(r, "session_fps", "low"), "0.0"));
            if (Field(r, "play_rate", null) != null)
                sb.AppendLine("VR: new frames " + Num(Field(r, "play_rate", "new_pairs_fps"), "0.0") + " fps (lowest " + Num(Field(r, "play_rate", "lowest_new_pairs_fps"), "0.0") +
                              "), headset " + Num(Field(r, "play_rate", "headset_fps"), "0") + " Hz");
            if (Field(r, "stream_stalls", null) != null)
                sb.AppendLine("Stalls: " + Num(Field(r, "stream_stalls", "rounds"), "0") + " (" + Num(Field(r, "stream_stalls", "seconds"), "0.0") + " s)");
            var on = OnNames(r);
            var heroes = on.Where(n => Heroes.Contains(n)).ToList();
            sb.AppendLine("Hero: " + (heroes.Count == 0 ? "Spider-Man" : "Spider-Man, " + string.Join(", ", heroes)));
            var feats = on.Where(n => Features.Contains(n)).ToList();
            sb.AppendLine("Features on: " + (feats.Count == 0 ? "none of the tracked ones" : string.Join(", ", feats)));
            if (r != null && r.ContainsKey("render_scale")) sb.AppendLine("Render scale: " + Num(Field(r, "render_scale", null), "0") + "%, supersample " + Num(Field(r, "supersample", null), "0") + "%");
            var adapters = Field(r, "game_adapters", null) as IEnumerable;
            if (adapters != null && !(adapters is string))
            {
                var gpu = adapters.Cast<object>().Select(o => Convert.ToString(o)).FirstOrDefault(n => !string.IsNullOrEmpty(n) && !n.Contains("Basic Render"));
                if (gpu != null) sb.AppendLine("Game GPU: " + Short(gpu, 60));
            }
            object hs = Field(r, "xr_runtime", "headset"), rt = Field(r, "xr_runtime", "name");
            if (hs != null || rt != null) sb.AppendLine("Headset: " + Short(Convert.ToString(hs ?? "?"), 40) + " via " + Short(Convert.ToString(rt ?? "?"), 40));
            object exited = Field(r, "game_exited", null), err = Field(r, "error", null);
            if (forStats) sb.AppendLine("Ended: " + (err != null && Convert.ToString(err).Length > 0 ? "with an error" : true.Equals(exited) ? "game closed" : "VR stopped"));
            else if (err != null && Convert.ToString(err).Length > 0) sb.AppendLine("Error: " + Short(Convert.ToString(err), 200));
            return sb.ToString();
        }

        /// <summary>Settings from the fixed hero and feature lists that were on at the start or turned on while playing.</summary>
        static List<string> OnNames(Dictionary<string, object> r)
        {
            var on = new List<string>();
            var start = Field(r, "vr_settings", null) as IDictionary<string, object>;
            if (start != null)
                foreach (var kv in start) if (IsOn(kv.Value) && !on.Contains(kv.Key)) on.Add(kv.Key);
            var changes = Field(r, "live_changes", null) as IEnumerable;
            if (changes != null)
                foreach (var c in changes)
                {
                    var m = c as IDictionary<string, object>;
                    object n, v;
                    if (m != null && m.TryGetValue("name", out n) && m.TryGetValue("value", out v) && IsOn(v))
                    { string s = Convert.ToString(n); if (!on.Contains(s)) on.Add(s); }
                }
            return on;
        }
        static bool IsOn(object v)
        {
            if (v is bool) return (bool)v;
            try { return v != null && !(v is string) && Convert.ToDouble(v, CultureInfo.InvariantCulture) != 0; } catch { return false; }
        }

        // ---- play stats ---------------------------------------------------------------------------------

        static void KeepStats(string summary)
        {
            Directory.CreateDirectory(Folder);
            var all = PendingStats();
            all.Add(summary.Trim());
            while (all.Count > MaxStatsKept) all.RemoveAt(0);
            File.WriteAllText(StatsFile, string.Join(StatsSeparator, all));
        }

        public static List<string> PendingStats()
        {
            try
            {
                if (!File.Exists(StatsFile)) return new List<string>();
                return File.ReadAllText(StatsFile).Replace("\r", "").Split(new[] { StatsSeparator }, StringSplitOptions.RemoveEmptyEntries)
                           .Select(s => s.Trim()).Where(s => s.Length > 0).ToList();
            }
            catch { return new List<string>(); }
        }

        public static string StatsBody()
        {
            var all = PendingStats();
            var sb = new StringBuilder();
            sb.AppendLine("Play stats from the HERO VR launcher (opt-in; no personal data). " + all.Count + " session" + (all.Count == 1 ? "" : "s") + ":");
            sb.AppendLine();
            sb.AppendLine("```");
            sb.AppendLine(string.Join("\n\n", all));
            sb.AppendLine("```");
            return sb.ToString();
        }

        public static void ClearStats() { try { File.Delete(StatsFile); } catch { } }
    }
}
