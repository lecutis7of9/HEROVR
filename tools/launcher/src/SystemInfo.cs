// SM2 VR Launcher - finding the game, its version, the VR runtime, the mod folder and its build.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace Sm2VrLauncher
{
    static class SystemInfo
    {
        public const string SteamAppId = "2651280";
        public const string GameExe = "Spider-Man2.exe";
        public const string GameProcess = "Spider-Man2";
        public const string GameFolder = "Marvel's Spider-Man 2";
        public const string SupportedVersion = "2.810.0.0";

        public static bool Preview { get { return Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_PREVIEW") == "1"; } }

        // ---- The mod folder ------------------------------------------------------------------------------

        public static bool IsPortRoot(string dir)
        {
            // tools\launch-game-vr.ps1 marks a mod folder (it carries VERSION and/or PORT-VERSION beside it).
            try { return !string.IsNullOrEmpty(dir) && File.Exists(Path.Combine(dir, "tools", "launch-game-vr.ps1")); }
            catch { return false; }
        }

        /// <summary>The saved folder, else the folder above the exe that holds tools\launch-game-vr.ps1, else C:\sm2-port.</summary>
        public static string FindPortRoot(string saved)
        {
            // A launcher that lives inside an install (tools\launcher) always manages that install and no other,
            // so copies in different folders never touch each other's files.
            try
            {
                var d = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
                for (int i = 0; d != null && i < 5; i++, d = d.Parent) if (IsPortRoot(d.FullName)) return d.FullName;
            }
            catch { }
            if (IsPortRoot(saved)) return Path.GetFullPath(saved);
            if (IsPortRoot(@"C:\sm2-port")) return @"C:\sm2-port";
            return string.IsNullOrEmpty(saved) ? @"C:\sm2-port" : saved;
        }

        /// <summary>The installed mod version for display: "Version 0.0.1" (VERSION, else the semver in PORT-VERSION),
        /// "Update 38.5" for an install from before 0.0.1, the raw PORT-VERSION line if it parses as neither, or null.</summary>
        public static string PortVersion(string root)
        {
            var v = ModVersion.Read(root);
            if (v != null) return v.Display;
            try { return ModVersion.FirstLine(Path.Combine(root, "VERSION")) ?? ModVersion.FirstLine(Path.Combine(root, "PORT-VERSION")); }
            catch { return null; }
        }

        /// <summary>"Version 0.0.1 (build 38.5)" when PORT-VERSION names the internal build.</summary>
        public static string PortVersionLong(string root)
        {
            var v = ModVersion.Read(root);
            return v != null ? v.DisplayLong : PortVersion(root);
        }

        /// <summary>A dotted number ("2.810.0.0") to a Version, for game file versions. Mod versions use ModVersion.</summary>
        public static Version ParseVersion(string s)
        {
            if (string.IsNullOrEmpty(s)) return null;
            var m = Regex.Match(s, @"\d+(\.\d+){0,3}");
            if (!m.Success) return null;
            string v = m.Value;
            if (!v.Contains(".")) v += ".0";
            Version r;
            return Version.TryParse(v, out r) ? r : null;
        }

        public static string BuiltModule(string root) { return Path.Combine(root, "build", "windows-ninja", "spidy_stereo_probe.dll"); }

        /// <summary>(built time or null, newest source file newer than the build or null).</summary>
        public static void BuildState(string root, out DateTime? built, out string staleBecause)
        {
            built = null; staleBecause = null;
            try
            {
                string dll = BuiltModule(root);
                if (!File.Exists(dll)) return;
                built = File.GetLastWriteTime(dll);
                DateTime builtUtc = File.GetLastWriteTimeUtc(dll);
                foreach (var folder in new[] { "src", "include" })
                {
                    string p = Path.Combine(root, folder);
                    if (!Directory.Exists(p)) continue;
                    foreach (var f in Directory.EnumerateFiles(p, "*", SearchOption.AllDirectories))
                        if (File.GetLastWriteTimeUtc(f) > builtUtc) { staleBecause = Path.GetFileName(f); return; }
                }
            }
            catch { }
        }

        /// <summary>The param() names (and their default text) of a PowerShell script, read from its text.</summary>
        public static Dictionary<string, string> ScriptParams(string ps1)
        {
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            try
            {
                string text = File.ReadAllText(ps1);
                int start = text.IndexOf("param(", StringComparison.OrdinalIgnoreCase);
                if (start < 0) return result;
                int depth = 0, end = start + 5;
                for (int i = start + 5; i < text.Length; i++)
                {
                    char c = text[i];
                    if (c == '#') { int nl = text.IndexOf('\n', i); if (nl < 0) break; i = nl; continue; }
                    if (c == '(') depth++;
                    else if (c == ')') { depth--; if (depth == 0) { end = i; break; } }
                }
                string block = text.Substring(start, end - start);
                foreach (Match m in Regex.Matches(block, @"\$(\w+)\s*(=\s*('[^']*'|[^,\r\n#\)]+))?"))
                {
                    string def = m.Groups[3].Success ? m.Groups[3].Value.Trim().Trim(new[] { '\'' }) : null;
                    result[m.Groups[1].Value] = def;
                }
            }
            catch { }
            return result;
        }

        // ---- The game ------------------------------------------------------------------------------------

        static string Reg(RegistryHive hive, string key, string name)
        {
            try
            {
                using (var b = RegistryKey.OpenBaseKey(hive, RegistryView.Registry64))
                using (var k = b.OpenSubKey(key))
                    return k == null ? null : k.GetValue(name) as string;
            }
            catch { return null; }
        }

        public static string SteamRoot()
        {
            foreach (var v in new[] {
                Reg(RegistryHive.CurrentUser, @"Software\Valve\Steam", "SteamPath"),
                Reg(RegistryHive.LocalMachine, @"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath"),
                Reg(RegistryHive.LocalMachine, @"SOFTWARE\Valve\Steam", "InstallPath"),
                Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam") })
            {
                if (string.IsNullOrEmpty(v)) continue;
                string p = v.Replace('/', '\\');
                try { if (File.Exists(Path.Combine(p, "steam.exe"))) return p; } catch { }
            }
            return null;
        }

        static IEnumerable<string> VdfValues(string text, string key)
        {
            foreach (Match m in Regex.Matches(text, "\"" + Regex.Escape(key) + "\"\\s+\"([^\"]*)\"", RegexOptions.IgnoreCase))
                yield return m.Groups[1].Value.Replace("\\\\", "\\");
        }

        /// <summary>Spider-Man2.exe from every Steam library (libraryfolders.vdf), else the Epic Games Store's; null if none.</summary>
        public static string FindGame(out string where)
        {
            where = "";
            var libs = new List<string>();
            string steam = SteamRoot();
            if (steam != null)
            {
                libs.Add(steam);
                foreach (var vdf in new[] { @"steamapps\libraryfolders.vdf", @"config\libraryfolders.vdf" })
                {
                    try
                    {
                        string f = Path.Combine(steam, vdf);
                        if (File.Exists(f)) foreach (var p in VdfValues(File.ReadAllText(f), "path"))
                                if (!libs.Any(l => string.Equals(l.TrimEnd(new[] { '\\' }), p.TrimEnd(new[] { '\\' }), StringComparison.OrdinalIgnoreCase))) libs.Add(p);
                    }
                    catch { }
                }
            }
            where = libs.Count == 0 ? "no Steam folder found" : string.Join(", ", libs);
            foreach (var lib in libs)
            {
                try
                {
                    string acf = Path.Combine(lib, "steamapps", "appmanifest_" + SteamAppId + ".acf");
                    if (!File.Exists(acf)) continue;
                    string folder = VdfValues(File.ReadAllText(acf), "installdir").FirstOrDefault() ?? GameFolder;
                    string exe = Path.Combine(lib, "steamapps", "common", folder, GameExe);
                    if (File.Exists(exe)) return exe;
                }
                catch { }
            }
            try
            {
                string epic = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), @"Epic\EpicGamesLauncher\Data\Manifests");
                if (Directory.Exists(epic))
                    foreach (var item in Directory.GetFiles(epic, "*.item"))
                    {
                        string t = File.ReadAllText(item);
                        if (!t.Contains("Spider-Man 2")) continue;
                        var m = Regex.Match(t, "\"InstallLocation\"\\s*:\\s*\"([^\"]*)\"");
                        if (!m.Success) continue;
                        string exe = Path.Combine(m.Groups[1].Value.Replace("\\\\", "\\"), GameExe);
                        if (File.Exists(exe)) return exe;
                    }
            }
            catch { }
            return null;
        }

        public static string FileVersion(string exe)
        {
            try
            {
                var v = FileVersionInfo.GetVersionInfo(exe);
                if (v.FileMajorPart != 0 || v.FileMinorPart != 0)
                    return v.FileMajorPart + "." + v.FileMinorPart + "." + v.FileBuildPart + "." + v.FilePrivatePart;
                if (!string.IsNullOrEmpty(v.ProductVersion)) return v.ProductVersion.Trim();
            }
            catch { }
            return null;
        }

        public static bool GameRunning()
        {
            try { var p = Process.GetProcessesByName(GameProcess); bool r = p.Length > 0; foreach (var x in p) x.Dispose(); return r; }
            catch { return false; }
        }

        static bool Running(params string[] names)
        {
            foreach (var n in names)
            {
                try { var p = Process.GetProcessesByName(n); bool r = p.Length > 0; foreach (var x in p) x.Dispose(); if (r) return true; }
                catch { }
            }
            return false;
        }

        /// <summary>(state, one-line description) of the VR side: Virtual Desktop, SteamVR, Meta Link and the active OpenXR runtime.</summary>
        public static CheckState VrStatus(out string detail)
        {
            var running = new List<string>();
            if (Running("VirtualDesktop.Streamer", "VirtualDesktop.Service")) running.Add("Virtual Desktop streamer");
            if (Running("vrserver", "vrmonitor")) running.Add("SteamVR");
            if (Running("OVRServer_x64")) running.Add("Meta Quest Link");
            string active = ActiveOpenXr();
            string act = active == null ? "no active OpenXR runtime set in Windows" : "OpenXR active runtime: " + active;
            if (running.Count > 0)
            {
                detail = string.Join(" + ", running) + " running. " + act + ".";
                return CheckState.Ok;
            }
            detail = "Nothing VR is running yet. Quest: open Virtual Desktop (or Steam Link) and connect; SteamVR headsets: start SteamVR. " + act + ".";
            return CheckState.Warn;
        }

        public static string ActiveOpenXr()
        {
            string manifest = Reg(RegistryHive.LocalMachine, @"SOFTWARE\Khronos\OpenXR\1", "ActiveRuntime");
            if (string.IsNullOrEmpty(manifest)) return null;
            try
            {
                if (File.Exists(manifest))
                {
                    var m = Regex.Match(File.ReadAllText(manifest), "\"name\"\\s*:\\s*\"([^\"]+)\"");
                    if (m.Success) return m.Groups[1].Value;
                }
            }
            catch { }
            string stem = Path.GetFileNameWithoutExtension(manifest);
            if (stem.IndexOf("steamxr", StringComparison.OrdinalIgnoreCase) >= 0) return "SteamVR";
            if (stem.IndexOf("virtualdesktop", StringComparison.OrdinalIgnoreCase) >= 0) return "Virtual Desktop";
            if (stem.IndexOf("oculus", StringComparison.OrdinalIgnoreCase) >= 0) return "Meta (Oculus)";
            return stem;
        }

        // ---- Folders -------------------------------------------------------------------------------------

        [DllImport("shell32.dll")]
        static extern int SHGetKnownFolderPath([MarshalAs(UnmanagedType.LPStruct)] Guid id, uint flags, IntPtr token, out IntPtr path);

        public static string Downloads()
        {
            try
            {
                IntPtr p;
                if (SHGetKnownFolderPath(new Guid("374DE290-123F-4565-9164-39C4925E467B"), 0, IntPtr.Zero, out p) == 0)
                {
                    string s = Marshal.PtrToStringUni(p);
                    Marshal.FreeCoTaskMem(p);
                    if (!string.IsNullOrEmpty(s)) return s;
                }
            }
            catch { }
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");
        }

        /// <summary>The newest mod update zip in Downloads (sm2-vr-*.zip, or the older sm2-update-*.zip), or null.
        /// Full packages (sm2-full-*) and prebuilt packages (*-win64.zip) are not update zips.</summary>
        public static FileInfo NewestUpdateZip()
        {
            try
            {
                var d = new DirectoryInfo(Downloads());
                if (!d.Exists) return null;
                return d.GetFiles("sm2-*.zip").Where(f => IsUpdateZipName(f.Name)).OrderByDescending(f => f.LastWriteTimeUtc).FirstOrDefault();
            }
            catch { return null; }
        }

        public static bool IsUpdateZipName(string name)
        {
            return name != null && Regex.IsMatch(name, @"^sm2-(vr|update)-.*\.zip$", RegexOptions.IgnoreCase)
                && name.IndexOf("full", StringComparison.OrdinalIgnoreCase) < 0 && name.IndexOf("win64", StringComparison.OrdinalIgnoreCase) < 0;
        }

        public static void OpenFolder(string path, string select)
        {
            try
            {
                if (select != null && File.Exists(select)) Process.Start("explorer.exe", "/select,\"" + select + "\"");
                else if (Directory.Exists(path)) Process.Start("explorer.exe", "\"" + path + "\"");
            }
            catch { }
        }

        public static void OpenUrl(string url)
        {
            try { Process.Start(new ProcessStartInfo(url) { UseShellExecute = true }); } catch { }
        }
    }
}
