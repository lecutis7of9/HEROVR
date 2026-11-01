// HERO VR 0.0.76 SM1: PLAY VR starts Spider-Man 1 directly, through iilyn's own tools\run_game_vr.py, with his own
// settings. GPLv3. Made by Agent Mulder.
//
// What his launcher (SpidyVR 0.7.0, apps/launcher) does on START VR, and what this does the same way:
//  - his settings: %APPDATA%\Spidy\launcher.ini, read here READ-ONLY (system.cpp loadSettings; the same keys,
//    ranges and defaults). Nothing of his is ever written; change them in his launcher (OPEN IILYN'S LAUNCHER).
//  - the arguments: launcher_text.hpp sessionArguments, ported line for line (Sm1Session.Arguments); quoted as his
//    quoteArgument does (CommandLineToArgvW / Python read them back unchanged).
//  - the command: <his folder>\python\python.exe -B -u -X utf8 <his folder>\tools\run_game_vr.py <arguments>,
//    in his folder (system.cpp Session::start).
//  - the stop signal: a named manual-reset event of our own (Local\HeroVrSm1Stop-<pid>-<ticks>), passed as
//    --stop-event; setting it ends his VR session as his STOP does.
//  - the report: --output %APPDATA%\HERO VR SM1\reports\game-vr-<time>.json (ours, not his folder), and every
//    line his script prints goes to the HERO launcher's log and to sm1-session-<time>.log beside it.
// Before starting: the game version (4.630.0.0), a VR runtime installed (OpenXR's ActiveRuntime or an available
// runtime), and the HERO launcher not running as administrator.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;
using System.Threading;

namespace Sm2VrLauncher
{
    /// <summary>His SessionOptions (launcher_text.hpp), with his defaults.</summary>
    class Sm1SessionOptions
    {
        public bool SmallWindow = true, StockMonitorView = false, AimMarkers = true, AirWebs = true, Flips = false;
        public bool TriggerWebs = false, ArcadeWebs = false, GameDepthOfField = false, GameSharpening = false;
        public bool CalibrationPrompt = true;
        public int RenderScale = 100, SwingSpeed = 32, SnapTurn = 30, Haptics = 100, BodyHaptics = 100, ScreenSize = 1;
        public int ScreenDepth = 2, SmoothTurn = 0, Weight = 100, FlipSpeed = 180, Hud = 2, EyeHeightMm = 0, ArmLengthMm = 0;
        public int Upscale = Sm1Session.DefaultEyeUpscale, MonitorRate = Sm1Session.DefaultMonitorRate;
        /// <summary>launcher.ini xr_runtime: a runtime manifest, or "auto" (empty: auto).</summary>
        public string Runtime = "auto";
        public string GameExe = "";
    }

    static class Sm1Session
    {
        // His constants (eye_resolution.hpp, monitor_rate.hpp, body_calibration.hpp), 0.7.0.
        public const int MinimumRenderScale = 50, MaximumRenderScale = 200;
        public const int MinimumEyeUpscale = 50, MaximumEyeUpscale = 100, DefaultEyeUpscale = 67;
        public const int DefaultMonitorRate = 30, MaximumMonitorRate = 240;
        public const int MinEyeHeightMm = 1000, MaxEyeHeightMm = 2500, MinArmLengthMm = 250, MaxArmLengthMm = 1200;
        public const string Script = @"tools\run_game_vr.py";   // CONFIRMED (release zip 0.7.0)
        public const string Python = @"python\python.exe";      // CONFIRMED (release zip 0.7.0; his findPython's first)

        public static string SettingsPath
        {
            get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Spidy", "launcher.ini"); }
        }
        public static string ReportsFolder { get { return Path.Combine(Sm1.AddonDataFolder, "reports"); } }

        static int Clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
        static int Atoi(string s)
        {
            // C atoi: leading spaces and sign, then digits; anything else stops it (0 when none).
            int i = 0, sign = 1; long v = 0;
            while (i < s.Length && char.IsWhiteSpace(s[i])) i++;
            if (i < s.Length && (s[i] == '-' || s[i] == '+')) { if (s[i] == '-') sign = -1; i++; }
            while (i < s.Length && s[i] >= '0' && s[i] <= '9') { v = v * 10 + (s[i] - '0'); if (v > int.MaxValue) break; i++; }
            v *= sign;
            return v > int.MaxValue ? int.MaxValue : v < int.MinValue ? int.MinValue : (int)v;
        }
        public static bool Calibrated(Sm1SessionOptions o)
        {
            return o.EyeHeightMm >= MinEyeHeightMm && o.EyeHeightMm <= MaxEyeHeightMm && o.ArmLengthMm >= MinArmLengthMm && o.ArmLengthMm <= MaxArmLengthMm;
        }

        /// <summary>His loadSettings, from launcher.ini's text (read only).</summary>
        public static Sm1SessionOptions Parse(string text)
        {
            var o = new Sm1SessionOptions();
            int weight = 0, percentWeight = 0, legacyWeight = 0;
            foreach (var raw in (text ?? "").Split(new[] { '\n' }))
            {
                string line = raw.EndsWith("\r") ? raw.Substring(0, raw.Length - 1) : raw;
                int eq = line.IndexOf('=');
                if (eq < 0) continue;
                string key = line.Substring(0, eq), value = line.Substring(eq + 1);
                int n = Atoi(value);
                switch (key)
                {
                    case "game": o.GameExe = value; break;
                    case "xr_runtime": o.Runtime = value.Length == 0 ? "auto" : value; break;
                    case "small_window": o.SmallWindow = n != 0; break;
                    case "stock_monitor_view": o.StockMonitorView = n != 0; break;
                    case "aim_markers": o.AimMarkers = n != 0; break;
                    case "air_webs": o.AirWebs = n != 0; break;
                    case "flips": o.Flips = n != 0; break;
                    case "trigger_webs": o.TriggerWebs = n != 0; break;
                    case "arcade_webs": o.ArcadeWebs = n != 0; break;
                    case "game_depth_of_field": o.GameDepthOfField = n != 0; break;
                    case "game_sharpening": o.GameSharpening = n != 0; break;
                    case "render_scale": o.RenderScale = n >= MinimumRenderScale && n <= MaximumRenderScale ? n : 100; break;
                    case "upscale": o.Upscale = n >= MinimumEyeUpscale && n <= MaximumEyeUpscale ? n : DefaultEyeUpscale; break;
                    case "monitor_rate": o.MonitorRate = n >= 0 && ValidMonitorRate(n) ? n : DefaultMonitorRate; break;
                    case "swing_speed": o.SwingSpeed = Clamp(n, 10, 65); break;
                    case "snap_turn": o.SnapTurn = Clamp(n, 0, 90); break;
                    case "smooth_turn": o.SmoothTurn = Clamp(n, 0, 360); break;
                    case "haptics": o.Haptics = Clamp(n, 0, 100); break;
                    case "body_haptics": o.BodyHaptics = Clamp(n, 0, 100); break;
                    case "screen_size": o.ScreenSize = Clamp(n, 0, 2); break;
                    case "screen_depth": o.ScreenDepth = Clamp(n, 0, 3); break;
                    case "hud": o.Hud = Clamp(n, 0, 3); break;
                    case "flip_speed": o.FlipSpeed = Clamp(n, 90, 480); break;
                    case "weight": legacyWeight = Clamp(n, 40, 300); break;
                    case "weight_percent": percentWeight = Clamp(n, 40, 300); break;
                    case "gravity_percent": weight = Clamp(n, 40, 300); break;
                    case "eye_height_mm": o.EyeHeightMm = n; break;
                    case "arm_length_mm": o.ArmLengthMm = n; break;
                    case "calibration_prompt": o.CalibrationPrompt = n != 0; break;
                }
            }
            if (weight != 0) o.Weight = weight;
            else if (percentWeight != 0 && percentWeight != 80) o.Weight = percentWeight;
            else if (percentWeight == 0 && legacyWeight != 0 && legacyWeight != 60) o.Weight = legacyWeight;
            if (!Calibrated(o)) o.EyeHeightMm = o.ArmLengthMm = 0;
            return o;
        }

        /// <summary>monitor_rate.hpp validMonitorRate: 0 (every frame) or 1..240.</summary>
        public static bool ValidMonitorRate(int n) { return n >= 0 && n <= MaximumMonitorRate; }

        /// <summary>launcher.ini read only (shared read); his defaults when it is missing.</summary>
        public static Sm1SessionOptions Load(out string note)
        {
            note = null;
            try
            {
                if (!File.Exists(SettingsPath)) { note = "his launcher.ini is not there yet: his defaults"; return new Sm1SessionOptions(); }
                using (var f = new FileStream(SettingsPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                using (var r = new StreamReader(f, new UTF8Encoding(false)))
                {
                    var buf = new char[1 << 16];
                    int n = r.ReadBlock(buf, 0, buf.Length);
                    return Parse(new string(buf, 0, n));
                }
            }
            catch (Exception ex) { note = "his launcher.ini could not be read (" + ex.Message + "): his defaults"; return new Sm1SessionOptions(); }
        }

        /// <summary>His sessionArguments (launcher_text.hpp), line for line.</summary>
        public static List<string> Arguments(Sm1SessionOptions o, string report, string runtime, string stopEvent, bool closeGame)
        {
            var a = new List<string> { "--auto-launch", "--seconds", "0", "--swing-speed", Clamp(o.SwingSpeed, 1, 65).ToString(CultureInfo.InvariantCulture), "--output", report };
            if (!o.SmallWindow) a.Add("--full-desktop-view");
            if (o.StockMonitorView) a.Add("--stock-monitor-view");
            if (!o.AimMarkers) a.Add("--no-aim-markers");
            if (!o.AirWebs) a.Add("--no-air-webs");
            if (o.Flips) a.Add("--flips");
            if (o.TriggerWebs) a.Add("--trigger-webs");
            if (o.ArcadeWebs) a.Add("--arcade-webs");
            if (o.GameDepthOfField) a.Add("--game-depth-of-field");
            if (o.GameSharpening) a.Add("--game-sharpening");
            if (Calibrated(o))
            {
                a.Add("--eye-height"); a.Add(o.EyeHeightMm.ToString(CultureInfo.InvariantCulture));
                a.Add("--arm-length"); a.Add(o.ArmLengthMm.ToString(CultureInfo.InvariantCulture));
            }
            else if (!o.CalibrationPrompt) a.Add("--no-calibration-prompt");
            var settings = new[]
            {
                new object[] { Clamp(o.RenderScale, MinimumRenderScale, MaximumRenderScale), "--render-scale", 100 },
                new object[] { Clamp(o.Upscale, MinimumEyeUpscale, MaximumEyeUpscale), "--upscale", DefaultEyeUpscale },
                new object[] { Clamp(o.SnapTurn, 0, 90), "--snap-turn", 30 },
                new object[] { Clamp(o.SmoothTurn, 0, 360), "--smooth-turn", 0 },
                new object[] { Clamp(o.Haptics, 0, 100), "--haptics", 100 },
                new object[] { Clamp(o.BodyHaptics, 0, 100), "--body-haptics", 100 },
                new object[] { Clamp(o.ScreenSize, 0, 2), "--screen-size", 1 },
                new object[] { Clamp(o.ScreenDepth, 0, 3), "--screen-depth", 2 },
                new object[] { Clamp(o.Weight, 40, 300), "--weight", 100 },
                new object[] { Clamp(o.Hud, 0, 3), "--hud", 2 },
                new object[] { Clamp(o.FlipSpeed, 90, 480), "--flip-speed", 180 },
                new object[] { Clamp(o.MonitorRate, 0, MaximumMonitorRate), "--monitor-rate", DefaultMonitorRate },
            };
            foreach (var s in settings)
                if ((int)s[0] != (int)s[2]) { a.Add((string)s[1]); a.Add(((int)s[0]).ToString(CultureInfo.InvariantCulture)); }
            if (!string.IsNullOrEmpty(runtime)) { a.Add("--xr-runtime"); a.Add(runtime); }
            if (!string.IsNullOrEmpty(stopEvent)) { a.Add("--stop-event"); a.Add(stopEvent); }
            if (closeGame) a.Add("--close-game");
            return a;
        }

        /// <summary>His quoteArgument: CommandLineToArgvW and Python read it back unchanged.</summary>
        public static string Quote(string arg)
        {
            if (arg.Length > 0 && arg.IndexOfAny(new[] { ' ', '\t', '\n', '\v', '"' }) < 0) return arg;
            var q = new StringBuilder("\"");
            for (int i = 0; ; ++i)
            {
                int slashes = 0;
                while (i < arg.Length && arg[i] == '\\') { ++slashes; ++i; }
                if (i == arg.Length) { q.Append('\\', slashes * 2); break; }
                if (arg[i] == '"') q.Append('\\', slashes * 2 + 1); else q.Append('\\', slashes);
                q.Append(arg[i]);
            }
            return q.Append('"').ToString();
        }

        public static string CommandLine(string script, List<string> args)
        {
            var s = new StringBuilder("-B -u -X utf8 " + Quote(script));
            foreach (var a in args) s.Append(' ').Append(Quote(a));
            return s.ToString();
        }

        /// <summary>His launcher passes a chosen runtime only when it is installed; "auto" passes none.</summary>
        public static string RuntimeArgument(Sm1SessionOptions o)
        {
            if (string.IsNullOrEmpty(o.Runtime) || o.Runtime == "auto") return "";
            try { return File.Exists(o.Runtime) ? o.Runtime : ""; } catch { return ""; }
        }

        public static string NewReportPath(DateTime now)
        {
            return Path.Combine(ReportsFolder, "game-vr-" + now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + ".json");
        }
        public static string NewStopEventName()
        {
            return @"Local\HeroVrSm1Stop-" + Process.GetCurrentProcess().Id + "-" + Environment.TickCount;
        }

        // ---- the checks before starting ----

        /// <summary>A VR runtime, from where his openXrRuntimes looks: Virtual Desktop's manifest, OpenXR's
        /// ActiveRuntime and AvailableRuntimes (64-bit view), and SteamVR from OpenVR's openvrpaths.vrpath.</summary>
        public static bool VrRuntimePresent(out string which)
        {
            which = null;
            if (Environment.OSVersion.Platform != PlatformID.Win32NT) return false;
            var manifests = new List<string>();
            manifests.Add(Path.Combine(Environment.GetEnvironmentVariable("ProgramFiles") ?? "", @"Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json"));
            try
            {
                using (var hk = Microsoft.Win32.RegistryKey.OpenBaseKey(Microsoft.Win32.RegistryHive.LocalMachine, Microsoft.Win32.RegistryView.Registry64))
                {
                    using (var k = hk.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1"))
                    {
                        var active = k == null ? null : k.GetValue("ActiveRuntime") as string;
                        if (!string.IsNullOrEmpty(active)) manifests.Add(active);
                    }
                    using (var k = hk.OpenSubKey(@"SOFTWARE\Khronos\OpenXR\1\AvailableRuntimes"))
                        if (k != null) manifests.AddRange(k.GetValueNames());
                }
            }
            catch { }
            try
            {
                string vrpath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "openvr", "openvrpaths.vrpath");
                if (File.Exists(vrpath))
                    foreach (var folder in OpenVrRuntimes(File.ReadAllText(vrpath)))
                        manifests.Add(Path.Combine(folder, "steamxr_win64.json"));
            }
            catch { }
            foreach (var m in manifests)
            {
                try
                {
                    string path = Environment.ExpandEnvironmentVariables(m);
                    if (path.Length > 0 && File.Exists(path)) { which = path; return true; }
                }
                catch { }
            }
            return false;
        }

        /// <summary>The folders in openvrpaths.vrpath's "runtime" list (JSON strings, backslashes escaped).</summary>
        public static List<string> OpenVrRuntimes(string json)
        {
            var r = new List<string>();
            var m = System.Text.RegularExpressions.Regex.Match(json ?? "", "\"runtime\"\\s*:\\s*\\[([^\\]]*)\\]");
            if (!m.Success) return r;
            foreach (System.Text.RegularExpressions.Match s in System.Text.RegularExpressions.Regex.Matches(m.Groups[1].Value, "\"((?:[^\"\\\\]|\\\\.)*)\""))
                r.Add(s.Groups[1].Value.Replace("\\\\", "\\").Replace("\\/", "/"));
            return r;
        }

        public static bool RunningAsAdministrator()
        {
            if (Environment.OSVersion.Platform != PlatformID.Win32NT) return false;
            try
            {
                using (var id = System.Security.Principal.WindowsIdentity.GetCurrent())
                    return new System.Security.Principal.WindowsPrincipal(id).IsInRole(System.Security.Principal.WindowsBuiltInRole.Administrator);
            }
            catch { return false; }
        }

        /// <summary>The reasons not to start (empty: go). His blockers, as far as they apply here.</summary>
        public static List<string> Blockers(string modFolder, string gameExe, string gameVersion, bool vrRuntime, bool admin)
        {
            var r = new List<string>();
            if (string.IsNullOrEmpty(modFolder) || !File.Exists(Path.Combine(modFolder, Script)) || !File.Exists(Path.Combine(modFolder, Python)))
                r.Add("iilyn's mod files are incomplete (" + Script + " and " + Python + " are needed): Update his mod.");
            if (string.IsNullOrEmpty(gameExe)) r.Add("Spider-Man 1 (" + Sm1.GameName + ") was not found.");
            else if (gameVersion != Sm1.SupportedVersion) r.Add("Spider-Man.exe is version " + (gameVersion ?? "unknown") + "; iilyn's mod " + Sm1.KnownVersion + " supports " + Sm1.SupportedVersion + " only.");
            if (!vrRuntime) r.Add("No VR runtime is installed (SteamVR, Oculus/Meta, Virtual Desktop or another OpenXR runtime).");
            if (admin) r.Add("The HERO VR launcher is running as administrator: close it and open it normally (the game and his mod must not run elevated).");
            return r;
        }
    }

    /// <summary>One VR session: his script run with our arguments, its output to our log, our stop event.</summary>
    class Sm1VrSession
    {
        Process process;
        EventWaitHandle stop;
        StreamWriter log;
        readonly object gate = new object();
        public string Report { get; private set; }
        public string LogPath { get; private set; }

        public bool Running { get { lock (gate) return process != null && !process.HasExited; } }

        /// <summary>Starts his script; `line` gets every printed line (any thread), `ended` the exit code.</summary>
        public void Start(string modFolder, Sm1SessionOptions o, bool closeGame, Action<string> line, Action<int> ended)
        {
            lock (gate)
            {
                if (process != null && !process.HasExited) throw new InvalidOperationException("Spider-Man 1 VR is already running.");
                Directory.CreateDirectory(Sm1Session.ReportsFolder);
                var now = DateTime.Now;
                Report = Sm1Session.NewReportPath(now);
                LogPath = Path.Combine(Sm1Session.ReportsFolder, "sm1-session-" + now.ToString("yyyyMMdd-HHmmss", CultureInfo.InvariantCulture) + ".log");
                string eventName = Sm1Session.NewStopEventName();
                if (stop != null) stop.Dispose();
                stop = new EventWaitHandle(false, EventResetMode.ManualReset, eventName);
                var args = Sm1Session.Arguments(o, Report, Sm1Session.RuntimeArgument(o), eventName, closeGame);
                var psi = new ProcessStartInfo(Path.Combine(modFolder, Sm1Session.Python), Sm1Session.CommandLine(Path.Combine(modFolder, Sm1Session.Script), args))
                {
                    WorkingDirectory = modFolder, UseShellExecute = false, CreateNoWindow = true,
                    RedirectStandardOutput = true, RedirectStandardError = true,
                    StandardOutputEncoding = new UTF8Encoding(false), StandardErrorEncoding = new UTF8Encoding(false),
                };
                log = new StreamWriter(new FileStream(LogPath, FileMode.Create, FileAccess.Write, FileShare.ReadWrite | FileShare.Delete), new UTF8Encoding(false)) { AutoFlush = true };
                log.WriteLine("HERO VR SM1 session " + now.ToString("yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture) + ": " + psi.FileName + " " + psi.Arguments);
                var p = new Process { StartInfo = psi, EnableRaisingEvents = true };
                DataReceivedEventHandler take = (s, e) => { if (e.Data != null) Say(e.Data, line); };
                p.OutputDataReceived += take;
                p.ErrorDataReceived += take;
                p.Exited += (s, e) =>
                {
                    int code = -1;
                    try { p.WaitForExit(); code = p.ExitCode; } catch { }
                    Say("his VR session ended (exit " + code + ").", line);
                    lock (gate) { if (log != null) { log.Dispose(); log = null; } }
                    if (ended != null) ended(code);
                };
                p.Start();
                p.BeginOutputReadLine();
                p.BeginErrorReadLine();
                process = p;
            }
        }

        void Say(string text, Action<string> line)
        {
            lock (gate) { try { if (log != null) log.WriteLine(text); } catch { } }
            if (line != null) line(text);
        }

        /// <summary>Our stop event: his session ends VR as his own STOP does (the game is his script's to close).</summary>
        public bool Stop()
        {
            lock (gate)
            {
                if (stop == null || process == null || process.HasExited) return false;
                stop.Set();
                return true;
            }
        }
    }
}
