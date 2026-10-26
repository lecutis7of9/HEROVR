// HERO VR Launcher - the ready-made mod files. Players never build: every release carries the compiled mod
// (build\windows-ninja), its own Python 3.12 (python\) and the notices (docs\licenses) as one zip, listed in that
// release's version.json as kind "prebuilt" with its SHA-256. The launcher downloads it, checks it, and puts those
// three folders into the install (first start, PLAY VR, every update and revert). tools\install-prebuilt.ps1 does
// the same for launchers older than build 3, which run tools\build.ps1 after an update.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;

namespace Sm2VrLauncher
{
    static class ModFiles
    {
        public const string Kind = "prebuilt";
        /// <summary>Written next to the modules: which release they came from (and so not "older than the sources").</summary>
        public const string MarkerName = "HERO-VR-MOD-FILES.txt";
        /// <summary>The parts of the package that go into an install; nothing else in it is used.</summary>
        public static readonly string[] Parts = { "build/windows-ninja/", "python/", "docs/licenses/" };

        public static string Marker(string root) { return Path.Combine(root, "build", "windows-ninja", MarkerName); }
        public static bool Installed(string root) { return File.Exists(SystemInfo.BuiltModule(root)); }
        public static bool FromRelease(string root) { return File.Exists(Marker(root)); }
        /// <summary>The mod's own Python (python\python.exe from the ready-made mod files). Players never install Python.</summary>
        public static string BundledPython(string root) { return Path.Combine(root, "python", "python.exe"); }
        public static bool PythonPresent(string root) { return File.Exists(BundledPython(root)); }
        /// <summary>Why this folder's ready-made mod files must be (re)installed before PLAY VR, or null when they are complete:
        /// no modules, the bundled Python missing (launcher builds before 8 dropped python\ on a same-version hotfix update),
        /// or modules from an older build than the sources. Developer builds (no marker) only need the modules.</summary>
        public static string RepairReason(string root)
        {
            if (!Installed(root)) return "not installed yet";
            if (!FromRelease(root)) return null;
            if (!PythonPresent(root)) return "missing the mod's own Python (python folder)";
            if (Stale(root)) return "from " + MarkerBuild(Marker(root)) + ", older than this folder";
            return null;
        }
        const string MarkerHead = "Ready-made HERO VR mod files for ";
        /// <summary>The mod build a marker file names ("Update 39.1"), or null when unreadable or unknown.</summary>
        public static string MarkerBuild(string markerPath)
        {
            try
            {
                if (!File.Exists(markerPath)) return null;
                string l = (ModVersion.FirstLine(markerPath) ?? "").Trim();
                if (!l.StartsWith(MarkerHead, StringComparison.OrdinalIgnoreCase)) return null;
                l = l.Substring(MarkerHead.Length).Trim();
                return l.Length == 0 || l == "?" ? null : l;
            }
            catch { return null; }
        }
        /// <summary>True when ready-made mod files from an older build sit beside newer sources: the same Version (0.0.6) with a
        /// newer build (Update 39.1 files, Update 39.1.1 PORT-VERSION). Launchers before build 8 kept the old files on such
        /// a same-version hotfix update, so the hotfix's modules never arrived; the launcher now replaces them by itself.</summary>
        public static bool Stale(string root) { return StaleFor(root, Marker(root)); }
        public static bool StaleFor(string root, string markerPath)
        {
            string files = MarkerBuild(markerPath), sources = PortLine(root);
            return files != null && sources != null && !string.Equals(files, sources, StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>"v0.0.5" for the installed VERSION, or null.</summary>
        public static string Tag(string root)
        {
            var v = ModVersion.Read(root);
            return v == null || v.Legacy ? null : "v" + v.Number;
        }

        /// <summary>The version.json of the installed version's own release (not "latest": the files must match the
        /// sources in the folder). launcher.ini PrebuiltManifestUrl=... replaces it (local tests).</summary>
        public static Manifest ManifestFor(string root, Settings settings, Manifest known, Action<string> log)
        {
            string tag = Tag(root);
            if (tag == null) throw new InvalidDataException("The mod folder has no VERSION file, so the launcher can't tell which mod files it needs. Unzip the HERO VR zip again into a new folder.");
            if (known != null && known.File(Kind) != null && ModVersion.Same(known.VersionNumber ?? known.Version, tag)) return known;
            string url = settings.Get("PrebuiltManifestUrl", "").Trim();
            if (url.Length == 0)
            {
                if (!settings.RepoConfigured) throw new InvalidDataException("No GitHub repository is set (Updates page), so the mod files can't be downloaded.");
                url = "https://github.com/" + settings.UpdateRepo.Trim().Trim(new[] { '/' }) + "/releases/download/" + tag + "/version.json";
            }
            if (!Manifest.SafeUrl(url)) throw new InvalidDataException("PrebuiltManifestUrl must be https (or http://127.0.0.1 for a local test): " + url);
            log("Reading " + url);
            var m = Manifest.Parse(Manifest.Fetch(url));
            m.ManifestUrl = url;
            return m;
        }

        /// <summary>The installed PORT-VERSION first line ("Update 38.9"), or null.</summary>
        static string PortLine(string root) { try { return ModVersion.FirstLine(Path.Combine(root, "PORT-VERSION")); } catch { return null; } }

        /// <summary>Checks the downloaded package and puts its mod files into the install. The caller holds the folder lock
        /// (and, in a transaction, has the old build parked). Throws with a plain message when anything is wrong; the
        /// install's own files are never touched.</summary>
        public static void Install(string root, string zipPath, string sha256, Action<string> log, Action<string, int> progress)
        {
            progress("Checking the mod files...", -1);
            var c = Installer.Inspect(zipPath);
            string want = PortLine(root), have = null;
            bool zipHasPython = false;
            using (var z = ZipFile.OpenRead(zipPath))
            {
                var pv = z.Entries.FirstOrDefault(e => e.FullName.Replace('\\', '/').Equals(c.Prefix + "PORT-VERSION", StringComparison.OrdinalIgnoreCase));
                if (pv != null) using (var rd = new StreamReader(pv.Open())) have = (rd.ReadLine() ?? "").Trim().TrimStart(new[] { '\uFEFF' });
                if (want != null && have != null && !string.Equals(want, have, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("The downloaded mod files are for " + have + ", but this folder has " + want + ". Nothing was changed.");
                if (!z.Entries.Any(e => e.FullName.Replace('\\', '/').Equals(c.Prefix + "build/windows-ninja/spidy_stereo_probe.dll", StringComparison.OrdinalIgnoreCase)))
                    throw new InvalidDataException("The downloaded file is not the HERO VR mod files (it has no build\\windows-ninja\\spidy_stereo_probe.dll). Nothing was changed.");

                // Unpacked next to the install first (same drive), then moved in: a half-finished unzip never counts.
                string tmp = Path.Combine(Backups.TxnFolder(root), "modfiles");
                if (Directory.Exists(tmp)) Directory.Delete(tmp, true);
                Directory.CreateDirectory(tmp);
                string full = Path.GetFullPath(tmp).TrimEnd(new[] { '\\', '/' }) + Path.DirectorySeparatorChar;
                var entries = z.Entries.Where(e => e.Name.Length > 0).ToList();
                int n = 0;
                foreach (var e in entries)
                {
                    string rel = e.FullName.Replace('\\', '/');
                    if (c.Prefix.Length > 0 && rel.StartsWith(c.Prefix)) rel = rel.Substring(c.Prefix.Length);
                    if (!Parts.Any(p => rel.StartsWith(p, StringComparison.OrdinalIgnoreCase))) continue;
                    string to = Path.GetFullPath(Path.Combine(tmp, rel.Replace('/', Path.DirectorySeparatorChar)));
                    if (!to.StartsWith(full, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Unsafe path in the mod files: " + e.FullName);
                    Directory.CreateDirectory(Path.GetDirectoryName(to));
                    e.ExtractToFile(to, true);
                    if (++n % 50 == 0) progress("Unpacking the mod files: " + n, -1);
                }
                log("Unpacked " + n + " mod files.");

                progress("Installing the mod files...", -1);
                string build = Path.Combine(root, "build");
                Directory.CreateDirectory(build);
                Replace(Path.Combine(tmp, "build", "windows-ninja"), Path.Combine(build, "windows-ninja"));
                zipHasPython = File.Exists(Path.Combine(tmp, "python", "python.exe"));
                if (Directory.Exists(Path.Combine(tmp, "python"))) Replace(Path.Combine(tmp, "python"), Path.Combine(root, "python"));
                string lic = Path.Combine(tmp, "docs", "licenses");
                if (Directory.Exists(lic))
                {
                    string dest = Path.Combine(root, "docs", "licenses");
                    Directory.CreateDirectory(dest);
                    foreach (var f in Directory.GetFiles(lic)) File.Copy(f, Path.Combine(dest, Path.GetFileName(f)), true);
                }
                try { Directory.Delete(tmp, true); } catch { }
            }
            // Release mod zips always carry their own Python; a folder left without it can't start VR on a player's PC.
            if (zipHasPython && !PythonPresent(root))
                throw new InvalidDataException("The mod files were installed but python\\python.exe is missing afterwards. Try Repair again.");
            // Dated now: the modules are newer than the sources they were built from, so nothing says "build again".
            var now = DateTime.Now;
            foreach (var f in Directory.GetFiles(Path.Combine(root, "build", "windows-ninja"))) try { File.SetLastWriteTime(f, now); } catch { }
            File.WriteAllText(Marker(root), "Ready-made HERO VR mod files for " + (want ?? have ?? "?") + "\r\nPackage=" + Path.GetFileName(zipPath) +
                "\r\nSHA256=" + (sha256 ?? "") + "\r\nInstalled=" + now.ToString("o") + "\r\n");
            if (!Installed(root)) throw new InvalidDataException("The mod files did not install (build\\windows-ninja\\spidy_stereo_probe.dll is missing).");
            log("Mod files installed: " + (want ?? have) + ".");
        }

        static void Replace(string from, string to)
        {
            if (!Directory.Exists(from)) throw new InvalidDataException("The mod files package has no " + Path.GetFileName(from) + " folder.");
            if (Directory.Exists(to))
            {
                foreach (var f in Directory.GetFiles(to, "*", SearchOption.AllDirectories)) try { File.SetAttributes(f, FileAttributes.Normal); } catch { }
                try { Directory.Delete(to, true); }
                catch (Exception ex) { throw new IOException("Could not replace " + to + " (close the game and anything using that folder): " + ex.Message); }
            }
            Directory.Move(from, to);
        }

        // ---- Developers only --------------------------------------------------------------------------------

        static bool? devTools;
        /// <summary>Visual Studio's C++ tools and git: only then can the mod be built from source on this PC.</summary>
        public static bool DevToolsPresent()
        {
            if (devTools.HasValue) return devTools.Value;
            bool ok = false;
            try
            {
                string pf = Environment.GetEnvironmentVariable("ProgramFiles(x86)") ?? "";
                string vswhere = Path.Combine(pf, "Microsoft Visual Studio", "Installer", "vswhere.exe");
                if (pf.Length > 0 && File.Exists(vswhere))
                {
                    var psi = new ProcessStartInfo(vswhere, "-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath")
                    { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true };
                    using (var p = Process.Start(psi))
                    {
                        string o = p.StandardOutput.ReadToEnd();
                        p.WaitForExit(10000);
                        ok = o.Trim().Length > 0 && OnPath("git.exe");
                    }
                }
            }
            catch { ok = false; }
            if (Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_DEVTOOLS") == "1") ok = true;   // automated tests only
            devTools = ok;
            return ok;
        }

        static bool OnPath(string exe)
        {
            foreach (var d in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(new[] { Path.PathSeparator }))
                try { if (d.Length > 0 && File.Exists(Path.Combine(d, exe))) return true; } catch { }
            return false;
        }
    }
}
