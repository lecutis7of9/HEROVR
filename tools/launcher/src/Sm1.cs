// HERO VR 0.0.76 SM1 (hotfix 0.0.75.2) - Spider-Man 1 in the launcher: iilyn's Spider-Man VR mod (Spidy by Ilya Mezerowsky,
// https://github.com/IlyaMez/SpidyVR, GPL-3.0), with Mulder's Heroes add-on. His mod is downloaded from HIS GitHub
// releases (so every download counts for him) and is never changed: no file of his is edited, renamed or patched.
// Mulder's Heroes add-on is our own separate files (src/sm1): herovr_sm1_layer.dll, an OpenXR implicit API layer the
// launcher registers for the current Windows user only while Spider-Man 1 plays (wrist menu, heroes, outfits, holo
// card, powers, flight), and herovr_sm1_addon.dll, a report-only DLL loaded after his VR runs. Their settings live in
// %APPDATA%\HERO VR SM1, never in his %APPDATA%\Spidy.
// Addresses below are CONFIRMED (read from his repository / release v0.7.0 on 2026-10-31) unless marked CANDIDATE.
// Made by Agent Mulder.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace Sm2VrLauncher
{
    /// <summary>What a GitHub release of his mod offers: the win64 zip and its checksum.</summary>
    class Sm1Release
    {
        public string Tag = "", Version = "", AssetName = "", Url = "", Sha256 = "", HtmlUrl = "";
        public long Size;
        public DateTime Published = DateTime.MinValue;
    }

    static class Sm1
    {
        // ---- Names and addresses -------------------------------------------------------------------------
        public const string Title = "iilyn's Spider-Man VR mod, with Mulder's Heroes add-on";
        public const string Author = "iilyn (Ilya Mezerowsky)";
        public const string ModName = "Spidy";
        public const string AddonName = "Mulder's Heroes add-on";
        public const string RepoUrl = "https://github.com/IlyaMez/SpidyVR";                                   // CONFIRMED
        public const string ReleasesUrl = "https://github.com/IlyaMez/SpidyVR/releases";                      // CONFIRMED
        public const string LatestApi = "https://api.github.com/repos/IlyaMez/SpidyVR/releases/latest";       // CONFIRMED
        public const string KofiUrl = "https://ko-fi.com/ilyamezerowsky";                                    // CONFIRMED (his README)
        public const string DiscordUrl = "https://discord.gg/x27ZXDpdNt";                                    // CONFIRMED (his README)
        public const string License = "GPL-3.0";
        public const string SteamAppId = "1817070";                                  // CONFIRMED: Marvel's Spider-Man Remastered
        public const string GameName = "Marvel's Spider-Man Remastered";
        public const string GameFolder = "Marvel's Spider-Man Remastered";          // CONFIRMED (his launcher's default installdir)
        public const string GameExe = "Spider-Man.exe", GameProcess = "Spider-Man";  // CONFIRMED
        public const string SupportedVersion = "4.630.0.0";                         // CONFIRMED (his tools/inspect_game.py, 0.7.0)
        public const string LauncherExe = "Spidy Launcher.exe";                     // CONFIRMED (release zip 0.7.0)
        public const string ModCheckFile = @"tools\run_game_vr.py";                 // CONFIRMED (release zip 0.7.0)
        public const string VrModule = "spidy_stereo_probe.dll";                    // CONFIRMED: loaded when his VR runs
        public const string AddonDll = "herovr_sm1_addon.dll";                      // ours (src/sm1)
        public const string LayerDll = "herovr_sm1_layer.dll";                      // ours (src/sm1/sm1_layer.cpp)
        public const string LayerJson = "herovr_sm1_layer.json";                    // ours (src/sm1/herovr_sm1_layer.json)
        public const string LayerRootFile = "herovr_root.txt";                      // beside the staged layer: HERO VR's folder (its Models)
        public const string LayerName = "XR_APILAYER_HEROVR_sm1_heroes";
        // CONFIRMED (OpenXR loader 1.1.49, src/loader/manifest_file.cpp): implicit layers are read from this key in
        // HKLM and, when the game is not run as administrator, HKCU; value name = manifest path, DWORD 0 = enabled.
        public const string LayerKey = @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit";
        public const string KnownVersion = "0.7.0";                                 // CONFIRMED: latest on 2026-10-31

        // ---- Settings (settings.ini) ---------------------------------------------------------------------
        public const string KeyGame = "Game";                 // "sm2" (HERO VR, default) or "sm1"
        public const string KeyGamePath = "Sm1.GamePath";
        public const string KeyModFolder = "Sm1.ModFolder";
        public const string KeyAddon = "Sm1.Addon";
        public const string KeyWrist = "Sm1.WristMenu";
        public const string KeyFlight = "Sm1.Flight";
        public const string KeyNews = "Sm1.ReleaseNews";
        public const string KeyDeclined = "Sm1.SetupDeclined";

        /// <summary>What the add-on does in Spider-Man 1 and how far each part is (include/spidy/sm1_addon.hpp features[];
        /// sm1_launcher_tests.py checks they match). "untested": built and wired, not yet tried in the game.</summary>
        public static readonly string[][] Features = {
            new[] { "wrist hero menu", "untested", "the SM2 hologram: right hand on the left wrist plus the right trigger opens it; point and pull the right trigger to pick" },
            new[] { "hero select", "untested", "11 heroes: a tile, then PLAY AS; the hologram closes and his powers work at once" },
            new[] { "outfits", "untested", "on each hero's OUTFITS tab, saved; the pick does not change the model in the game yet" },
            new[] { "holo card", "untested", "the hero's CONTROLS, OUTFITS and POWERS tabs on the hologram" },
            new[] { "hero powers", "untested", "trigger of an outstretched hand hits the ringed enemy, with a beam" },
            new[] { "flight in game", "untested", "Iron Man, Superman, Thor, Green Lantern: both arms up or A held to take off, the arms steer" },
            new[] { "color", "untested", "the SM2 COLOR looks (Metropolis, Smallville, Gotham, Cyberpunk, Synthwave and more) and sliders under the heroes on the wrist HEROES page; off by default, no GPU work when off" },
            new[] { "hero suits", "stub", "Install suits adds each hero as a new suit next to Spider-Man's in the game's Suits menu; nothing is replaced (not yet seen in the game)" },
            new[] { "holo suit switch", "stub", "Picking a hero on the wrist holo tells you to pick that hero's suit in the game's Suits menu; a live switch is not in yet" },
        };

        /// <summary>The feature list for the Spider-Man 1 page: what runs in the game and what does not yet.</summary>
        public static string FeatureSummary()
        {
            var built = Features.Where(f => f[1] != "stub").Select(f => f[0]).ToArray();
            var not = Features.Where(f => f[1] == "stub").Select(f => f[0]).ToArray();
            return "WITH SPIDER-MAN 1 (built, not yet tried in the game): " + string.Join(", ", built) + ". " +
                   "NOT YET: " + string.Join(", ", not) + ". Multiplayer: not for Spider-Man 1.";
        }

        public static string FeatureDetails()
        {
            var sb = new StringBuilder();
            foreach (var f in Features)
                sb.Append(f[0]).Append(": ").Append(f[1] == "stub" ? "NOT YET - " : "built, not yet tried in the game - ").Append(f[2]).Append('\n');
            return sb.ToString().TrimEnd(new[] { '\r', '\n', ' ' });
        }

        public static string DataFolder { get { return Path.Combine(Settings.Folder, "SM1"); } }
        public static string DefaultModFolder { get { return Path.Combine(DataFolder, "Spidy"); } }
        public static string Marker { get { return Path.Combine(DataFolder, "installed.txt"); } }
        /// <summary>%APPDATA%\HERO VR SM1: addon.ini (the launcher writes it) and addon.log (the add-on writes it).</summary>
        public static string AddonDataFolder
        {
            get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "HERO VR SM1"); }
        }

        // ---- The game --------------------------------------------------------------------------------------

        static IEnumerable<string> VdfValues(string text, string key)
        {
            foreach (Match m in Regex.Matches(text ?? "", "\"" + Regex.Escape(key) + "\"\\s*:?\\s*\"([^\"]*)\"", RegexOptions.IgnoreCase))
                yield return m.Groups[1].Value.Replace("\\\\", "\\");
        }

        public static bool IsGameExe(string exe)
        {
            try { return !string.IsNullOrEmpty(exe) && File.Exists(exe) && string.Equals(Path.GetFileName(exe), GameExe, StringComparison.OrdinalIgnoreCase); }
            catch { return false; }
        }

        /// <summary>Spider-Man.exe of Marvel's Spider-Man Remastered: every Steam library (appmanifest_1817070), Steam's
        /// uninstall entry, then the Epic Games Store's manifest (the same order as his launcher). null when none.</summary>
        public static string FindGame(out string where)
        {
            where = "";
            var libs = new List<string>();
            string steam = null;
            try { steam = SystemInfo.SteamRoot(); } catch { }
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
            where = libs.Count == 0 ? "no Steam folder found" : string.Join(", ", libs.ToArray());
            foreach (var lib in libs)
            {
                try
                {
                    string acf = Path.Combine(lib, "steamapps", "appmanifest_" + SteamAppId + ".acf");
                    if (!File.Exists(acf)) continue;
                    string folder = VdfValues(File.ReadAllText(acf), "installdir").FirstOrDefault() ?? GameFolder;
                    string exe = Path.Combine(Path.Combine(Path.Combine(lib, "steamapps"), "common"), Path.Combine(folder, GameExe));
                    if (File.Exists(exe)) return exe;
                }
                catch { }
            }
            try
            {
                if (Environment.OSVersion.Platform == PlatformID.Win32NT)
                    using (var hk = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
                    using (var k = hk.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App " + SteamAppId))
                    {
                        string loc = k == null ? null : k.GetValue("InstallLocation") as string;
                        if (!string.IsNullOrEmpty(loc) && File.Exists(Path.Combine(loc, GameExe))) return Path.Combine(loc, GameExe);
                    }
            }
            catch { }
            try
            {
                string epic = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), @"Epic\EpicGamesLauncher\Data\Manifests");
                if (Directory.Exists(epic))
                    foreach (var item in Directory.GetFiles(epic, "*.item"))
                    {
                        string t = File.ReadAllText(item);
                        if (t.IndexOf("Spider-Man", StringComparison.Ordinal) < 0 || t.IndexOf("Remastered", StringComparison.Ordinal) < 0) continue;
                        foreach (var loc in VdfValues(t, "InstallLocation"))
                        {
                            string exe = Path.Combine(loc, GameExe);
                            if (File.Exists(exe)) return exe;
                        }
                    }
            }
            catch { }
            return null;
        }

        public static string GamePath(Settings s, out string where)
        {
            where = "not searched";
            string saved = s.Get(KeyGamePath, null);
            if (IsGameExe(saved)) return saved;
            return FindGame(out where);
        }

        /// <summary>Spider-Man.exe's file version ("4.630.0.0"), or null when it cannot be read.</summary>
        public static string GameVersion(string exe)
        {
            try
            {
                if (string.IsNullOrEmpty(exe) || !File.Exists(exe)) return null;
                var vi = FileVersionInfo.GetVersionInfo(exe);
                if (vi.FileMajorPart > 0 || vi.FileMinorPart > 0)
                    return vi.FileMajorPart + "." + vi.FileMinorPart + "." + vi.FileBuildPart + "." + vi.FilePrivatePart;
                string v = (vi.FileVersion ?? "").Replace(", ", ".").Replace(',', '.').Trim();
                return v.Length == 0 ? null : v;
            }
            catch { return null; }
        }

        public static bool GameRunning()
        {
            try { var p = Process.GetProcessesByName(GameProcess); bool r = p.Length > 0; foreach (var x in p) x.Dispose(); return r; }
            catch { return false; }
        }

        // ---- His mod ---------------------------------------------------------------------------------------

        /// <summary>A folder holding his release: Spidy Launcher.exe and tools\run_game_vr.py.</summary>
        public static bool IsModFolder(string dir)
        {
            try
            {
                return !string.IsNullOrEmpty(dir) && File.Exists(Path.Combine(dir, LauncherExe)) && File.Exists(Path.Combine(dir, ModCheckFile.Replace('\\', Path.DirectorySeparatorChar)));
            }
            catch { return false; }
        }

        /// <summary>The folder picked with Browse, else the one this launcher installed; null when neither has his mod.</summary>
        public static string ModFolder(Settings s)
        {
            string saved = s.Get(KeyModFolder, null);
            if (IsModFolder(saved)) return saved;
            return IsModFolder(DefaultModFolder) ? DefaultModFolder : null;
        }

        /// <summary>The release this launcher installed into its own folder ("0.7.0"), or "" (picked by hand / unknown).</summary>
        public static string InstalledVersion(string dir)
        {
            try
            {
                if (dir == null || !string.Equals(Path.GetFullPath(dir).TrimEnd(new[] { '\\', '/' }), Path.GetFullPath(DefaultModFolder).TrimEnd(new[] { '\\', '/' }), StringComparison.OrdinalIgnoreCase)) return "";
                if (!File.Exists(Marker)) return "";
                foreach (var l in File.ReadAllLines(Marker))
                    if (l.StartsWith("version=", StringComparison.Ordinal)) return l.Substring(8).Trim();
            }
            catch { }
            return "";
        }

        public static bool LauncherRunning()
        {
            try { var p = Process.GetProcessesByName(Path.GetFileNameWithoutExtension(LauncherExe)); bool r = p.Length > 0; foreach (var x in p) x.Dispose(); return r; }
            catch { return false; }
        }

        /// <summary>His latest release (GitHub API): the Spidy-x.y.z-win64.zip asset and its sha256 digest.</summary>
        public static Sm1Release LatestRelease()
        {
            string json = Manifest.Fetch(LatestApi);
            var js = new JavaScriptSerializer { MaxJsonLength = 8 * 1024 * 1024 };
            var d = js.Deserialize<Dictionary<string, object>>(json);
            if (d == null) throw new InvalidDataException("GitHub sent no release.");
            var r = new Sm1Release { Tag = Str(d, "tag_name"), HtmlUrl = Str(d, "html_url") };
            r.Version = r.Tag.TrimStart(new[] { 'v', 'V' });
            DateTime pub;
            if (DateTime.TryParse(Str(d, "published_at"), System.Globalization.CultureInfo.InvariantCulture, System.Globalization.DateTimeStyles.AdjustToUniversal, out pub)) r.Published = pub;
            object assets;
            if (d.TryGetValue("assets", out assets) && assets is System.Collections.IEnumerable)
                foreach (var o in (System.Collections.IEnumerable)assets)
                {
                    var a = o as Dictionary<string, object>;
                    if (a == null) continue;
                    string name = Str(a, "name");
                    if (!IsReleaseZipName(name)) continue;
                    r.AssetName = name;
                    r.Url = Str(a, "browser_download_url");
                    long size; long.TryParse(Str(a, "size"), out size); r.Size = size;
                    string digest = Str(a, "digest");
                    if (digest.StartsWith("sha256:", StringComparison.OrdinalIgnoreCase)) r.Sha256 = digest.Substring(7).Trim().ToLowerInvariant();
                    break;
                }
            if (r.Url.Length == 0) throw new InvalidDataException("his latest release (" + r.Tag + ") has no Spidy-...-win64.zip");
            if (!r.Url.StartsWith("https://github.com/IlyaMez/SpidyVR/", StringComparison.OrdinalIgnoreCase) &&
                !r.Url.StartsWith("https://objects.githubusercontent.com/", StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("the download address is not on his GitHub (" + r.Url + ")");
            return r;
        }

        public static bool IsReleaseZipName(string name)
        {
            return !string.IsNullOrEmpty(name) && name.StartsWith("Spidy-", StringComparison.OrdinalIgnoreCase) &&
                name.EndsWith("-win64.zip", StringComparison.OrdinalIgnoreCase);
        }

        static string Str(Dictionary<string, object> d, string k)
        {
            object v;
            return d != null && d.TryGetValue(k, out v) && v != null ? Convert.ToString(v, System.Globalization.CultureInfo.InvariantCulture) : "";
        }

        public static string Sha256Of(string file)
        {
            using (var sha = SHA256.Create())
            using (var f = File.OpenRead(file))
                return BitConverter.ToString(sha.ComputeHash(f)).Replace("-", "").ToLowerInvariant();
        }

        /// <summary>Downloads his release zip from his GitHub (checked against GitHub's sha256 and size) and unpacks it,
        /// unchanged, into the launcher's SM1\Spidy folder. Returns that folder. Throws with a plain reason.</summary>
        public static string DownloadAndInstall(Sm1Release r, Action<string> log, Action<int> progress)
        {
            Directory.CreateDirectory(DataFolder);
            string part = Path.Combine(DataFolder, r.AssetName + ".part");
            string zip = Path.Combine(DataFolder, r.AssetName);
            try { if (File.Exists(part)) File.Delete(part); } catch { }
            log("Downloading " + r.AssetName + " from his GitHub (" + (r.Size > 0 ? (r.Size / (1024 * 1024)) + " MB" : "size unknown") + ")...");
            var req = (HttpWebRequest)WebRequest.Create(r.Url);
            req.UserAgent = LauncherInfo.UserAgent;
            req.Timeout = 30000;
            req.ReadWriteTimeout = 60000;
            req.AllowAutoRedirect = true;
            using (var resp = (HttpWebResponse)req.GetResponse())
            using (var input = resp.GetResponseStream())
            using (var output = File.Create(part))
            {
                long total = r.Size > 0 ? r.Size : resp.ContentLength, got = 0;
                var buf = new byte[81920];
                int n, last = -1;
                while ((n = input.Read(buf, 0, buf.Length)) > 0)
                {
                    output.Write(buf, 0, n);
                    got += n;
                    if (got > 512L * 1024 * 1024) throw new InvalidDataException("the download is far bigger than his release");
                    int pct = total > 0 ? (int)Math.Min(100, got * 100 / total) : -1;
                    if (pct != last && progress != null) { last = pct; progress(pct); }
                }
            }
            long size = new FileInfo(part).Length;
            if (r.Size > 0 && size != r.Size) throw new InvalidDataException("the download is " + size + " bytes, GitHub says " + r.Size + " (try again)");
            string sha = Sha256Of(part);
            if (r.Sha256.Length > 0 && sha != r.Sha256) throw new InvalidDataException("the download's checksum does not match GitHub's (try again)");
            log(r.Sha256.Length > 0 ? "Checksum OK (sha256 " + sha.Substring(0, 12) + "..., as GitHub lists it)." : "GitHub lists no checksum for this file; its size matched. sha256 " + sha);
            if (File.Exists(zip)) File.Delete(zip);
            File.Move(part, zip);
            string dir = InstallZip(zip, r.Version, sha, log);
            try { File.Delete(zip); } catch { }
            return dir;
        }

        /// <summary>Unpacks his zip (files unchanged; the zip's one top folder "Spidy-x.y.z" dropped) into SM1\Spidy.
        /// Unpacks next to it first, so a failed unpack never leaves half a mod.</summary>
        public static string InstallZip(string zip, string version, string sha, Action<string> log)
        {
            string target = DefaultModFolder;
            string fresh = Path.Combine(DataFolder, "new-" + DateTime.UtcNow.Ticks);
            Directory.CreateDirectory(fresh);
            string freshFull = Path.GetFullPath(fresh).TrimEnd(new[] { '\\', '/' }) + Path.DirectorySeparatorChar;
            using (var z = ZipFile.OpenRead(zip))
            {
                var names = z.Entries.Select(e => e.FullName.Replace('\\', '/')).Where(e => e.Length > 0).ToList();
                string top = null;
                var firsts = names.Select(e => e.Split(new[] { '/' })[0]).Distinct().ToList();
                if (firsts.Count == 1 && names.All(e => e.Contains("/"))) top = firsts[0] + "/";
                foreach (var e in z.Entries)
                {
                    string name = e.FullName.Replace('\\', '/');
                    if (top != null) name = name.Length > top.Length ? name.Substring(top.Length) : "";
                    if (name.Length == 0) continue;
                    string dest = Path.GetFullPath(Path.Combine(fresh, name.Replace('/', Path.DirectorySeparatorChar)));
                    if (!dest.StartsWith(freshFull, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("the zip has a path outside its folder (" + e.FullName + ")");
                    if (name.EndsWith("/")) { Directory.CreateDirectory(dest); continue; }
                    Directory.CreateDirectory(Path.GetDirectoryName(dest));
                    e.ExtractToFile(dest, true);
                }
            }
            if (!IsModFolder(fresh))
            {
                try { Directory.Delete(fresh, true); } catch { }
                throw new InvalidDataException("the zip has no " + LauncherExe + " and " + ModCheckFile + " (his release layout changed?)");
            }
            string old = null;
            if (Directory.Exists(target))
            {
                old = Path.Combine(DataFolder, "old-" + DateTime.UtcNow.Ticks);
                Directory.Move(target, old);
            }
            try { Directory.Move(fresh, target); }
            catch
            {
                if (old != null) try { Directory.Move(old, target); } catch { }
                throw;
            }
            if (old != null) try { Directory.Delete(old, true); } catch { log("The old copy in " + old + " could not be deleted yet (a file is open); it is harmless."); }
            File.WriteAllText(Marker, "version=" + version + "\nsha256=" + sha + "\nfrom=" + ReleasesUrl + "\ninstalled=" +
                DateTime.Now.ToString("yyyy-MM-dd HH:mm", System.Globalization.CultureInfo.InvariantCulture) + "\n");
            log("His mod " + version + " is in " + target + " (unchanged, as he released it).");
            return target;
        }

        // ---- Mulder's Heroes add-on ------------------------------------------------------------------------

        /// <summary>Where the HERO VR mod files put the add-on (build\windows-ninja beside spidy_stereo_probe.dll).</summary>
        public static string AddonSource(string root)
        {
            try { return root == null ? null : Path.Combine(Path.Combine(Path.Combine(root, "build"), "windows-ninja"), AddonDll); }
            catch { return null; }
        }

        public static bool AddonPresent(string root)
        {
            string f = AddonSource(root);
            try { return f != null && File.Exists(f); } catch { return false; }
        }

        /// <summary>A copy the game loads (SM1\addon\&lt;sha&gt;\), so a HERO VR update can replace the original while the game runs.</summary>
        public static string StageAddon(string root)
        {
            string src = AddonSource(root);
            if (src == null || !File.Exists(src)) return null;
            string sha = Sha256Of(src).Substring(0, 16);
            string dir = Path.Combine(Path.Combine(DataFolder, "addon"), sha);
            string dest = Path.Combine(dir, AddonDll);
            if (!File.Exists(dest))
            {
                Directory.CreateDirectory(dir);
                File.Copy(src, dest + ".tmp", true);
                File.Move(dest + ".tmp", dest);
            }
            return dest;
        }

        /// <summary>The layer's two files in the HERO VR mod files (build\windows-ninja).</summary>
        public static string LayerSource(string root, string file)
        {
            try { return root == null ? null : Path.Combine(Path.Combine(Path.Combine(root, "build"), "windows-ninja"), file); }
            catch { return null; }
        }

        public static bool LayerPresent(string root)
        {
            try { return File.Exists(LayerSource(root, LayerDll)) && File.Exists(LayerSource(root, LayerJson)); } catch { return false; }
        }

        /// <summary>Copies the layer (DLL and manifest) to SM1\layer\&lt;sha&gt;\, so a HERO VR update can replace the
        /// originals while the game runs. Returns the staged manifest's path, or null when the layer is not there.</summary>
        public static string StageLayer(string root)
        {
            if (!LayerPresent(root)) return null;
            string dll = LayerSource(root, LayerDll), json = LayerSource(root, LayerJson);
            string sha = Sha256Of(dll).Substring(0, 12) + Sha256Of(json).Substring(0, 4);
            string dir = Path.Combine(Path.Combine(DataFolder, "layer"), sha);
            Directory.CreateDirectory(dir);
            foreach (var src in new[] { dll, json })
            {
                string dest = Path.Combine(dir, Path.GetFileName(src));
                if (File.Exists(dest)) continue;
                File.Copy(src, dest + ".tmp", true);
                File.Move(dest + ".tmp", dest);
            }
            WriteLayerRoot(dir, root);
            return Path.Combine(dir, LayerJson);
        }

        /// <summary>0.0.76 HERO PROPS: the staged layer is in SM1\layer\&lt;sha&gt;\, not in HERO VR's build folder, so it
        /// cannot find HERO VR's Models (the props, the portraits) from its own path: HERO VR's folder goes in
        /// herovr_root.txt beside it (UTF-8, one line), replaced whole (written to .tmp first). Never throws.</summary>
        public static bool WriteLayerRoot(string dir, string root)
        {
            try
            {
                string dest = Path.Combine(dir, LayerRootFile), tmp = dest + ".tmp";
                File.WriteAllText(tmp, Path.GetFullPath(root), new System.Text.UTF8Encoding(false));
                if (File.Exists(dest)) File.Delete(dest);
                File.Move(tmp, dest);
                return true;
            }
            catch { return false; }
        }

        static bool OurLayerValue(string name)
        {
            try { return !string.IsNullOrEmpty(name) && string.Equals(Path.GetFileName(name.Trim()), LayerJson, StringComparison.OrdinalIgnoreCase); }
            catch { return false; }
        }

        /// <summary>Registers the staged manifest as an implicit OpenXR layer for this Windows user (HKCU, never HKLM).
        /// Any older entry of ours (another path, a stale one) is removed first. Only our own value is touched.</summary>
        public static void RegisterLayer(string manifest, string key = LayerKey)
        {
            RemoveLayers(key);
            using (var k = Registry.CurrentUser.CreateSubKey(key))
                k.SetValue(manifest, 0, RegistryValueKind.DWord);
        }

        /// <summary>Removes every entry of ours (a value named ...\herovr_sm1_layer.json) from HKCU; other layers stay.
        /// Returns how many it removed; never throws.</summary>
        public static int RemoveLayers(string key = LayerKey)
        {
            int n = 0;
            try
            {
                using (var k = Registry.CurrentUser.OpenSubKey(key, true))
                {
                    if (k == null) return 0;
                    foreach (var v in k.GetValueNames())
                        if (OurLayerValue(v)) { try { k.DeleteValue(v, false); n++; } catch { } }
                }
            }
            catch { }
            return n;
        }

        public static string[] RegisteredLayers(string key = LayerKey)
        {
            try
            {
                using (var k = Registry.CurrentUser.OpenSubKey(key))
                    return k == null ? new string[0] : k.GetValueNames().Where(OurLayerValue).ToArray();
            }
            catch { return new string[0]; }
        }

        /// <summary>Removes the layer entry once Spider-Man 1 has run and closed (or never started within the wait), so
        /// no other OpenXR app on this PC ever sees it. A background thread; never throws.</summary>
        public static void RemoveLayersAfterGame(Action<string> log, int startWaitSeconds = 900)
        {
            var t = new Thread(() =>
            {
                try
                {
                    bool seen = false;
                    for (int i = 0; i < startWaitSeconds && !seen; i++) { seen = GameRunning(); if (!seen) Thread.Sleep(1000); }
                    while (seen && GameRunning()) Thread.Sleep(2000);
                    int n = RemoveLayers();
                    if (log != null && n > 0) log(seen ? "Spider-Man 1 closed: the add-on's OpenXR layer entry was removed." : "Spider-Man 1 did not start: the add-on's OpenXR layer entry was removed.");
                }
                catch { }
            }) { IsBackground = true, Name = "SM1 layer cleanup" };
            t.Start();
        }

        /// <summary>%APPDATA%\HERO VR SM1\addon.ini. The launcher writes the switches; the hero and the outfits the
        /// wrist menu picked (hero=, outfit.N=) are kept as they were.</summary>
        public static void WriteAddonIni(bool heroes, bool wrist, bool flight)
        {
            string dir = AddonDataFolder;
            Directory.CreateDirectory(dir);
            string file = Path.Combine(dir, "addon.ini");
            string hero = "0";
            var outfits = new List<string>();
            try
            {
                if (File.Exists(file))
                    foreach (var l in File.ReadAllLines(file))
                    {
                        var t = l.Trim();
                        if (t.StartsWith("hero=", StringComparison.Ordinal) && Regex.IsMatch(t.Substring(5).Trim(), "^[0-9]{1,3}$")) hero = t.Substring(5).Trim();
                        else if (Regex.IsMatch(t, "^outfit\\.[0-9]{1,2}=[0-9]{1,3}$")) outfits.Add(t);
                    }
            }
            catch { }
            string text = "# Mulder's Heroes add-on (HERO VR) for iilyn's Spider-Man VR mod. The HERO VR launcher writes the switches; the wrist menu writes hero= and outfit.N=.\n" +
                "heroes=" + (heroes ? "1" : "0") + "\nwrist_menu=" + (wrist ? "1" : "0") + "\nflight=" + (flight ? "1" : "0") + "\nhero=" + hero + "\n";
            foreach (var o in outfits) text += o + "\n";
            File.WriteAllText(file + ".tmp", text, new UTF8Encoding(false));
            if (File.Exists(file)) File.Delete(file);
            File.Move(file + ".tmp", file);
        }

        /// <summary>The add-on's own report (addon.log), last line, or "".</summary>
        public static string AddonLogLast()
        {
            try
            {
                string f = Path.Combine(AddonDataFolder, "addon.log");
                if (!File.Exists(f)) return "";
                var lines = File.ReadAllLines(f).Where(l => l.Trim().Length > 0).ToArray();
                return lines.Length == 0 ? "" : lines[lines.Length - 1].Trim();
            }
            catch { return ""; }
        }

        // ---- The pop-up texts (what is missing and why) -----------------------------------------------------

        public static string GameMissingText(string where)
        {
            return "Spider-Man 1 is missing.\n\n" + GameName + " (Steam) was not found on this PC (" + where + "). " +
                "iilyn's Spider-Man VR mod runs on that game only (Spider-Man.exe v" + SupportedVersion + "); other Spider-Man games are not supported.\n\n" +
                "Yes, Autosetup opens Steam's install page for it. Already installed somewhere else? Press No, then Browse game on the Spider-Man 1 page.";
        }

        public static string ModMissingText()
        {
            return "iilyn's Spider-Man VR mod is missing.\n\nThe Spider-Man 1 option runs his mod (" + ModName + " by Ilya Mezerowsky). " +
                "It is not on this PC yet.\n\nYes, Autosetup downloads his latest release from his own GitHub (" + ReleasesUrl + ", about 17 MB), " +
                "checks its checksum and unpacks it unchanged into the launcher's folder. Nothing else to do.";
        }

        public static string AddonMissingText()
        {
            return "Mulder's Heroes add-on is missing.\n\nThe add-on file (" + AddonDll + ") is not in the HERO VR mod files yet, " +
                "so Spider-Man 1 would start with iilyn's mod alone.\n\nYes, Autosetup downloads the ready-made HERO VR mod files (Repair). " +
                "No starts his mod without the add-on this time.";
        }

        public static string VersionWarning(string v)
        {
            return "Spider-Man.exe is v" + (v ?? "?") + "; iilyn's mod is made for v" + SupportedVersion + ". His launcher says whether it can run it.";
        }
    }

    /// <summary>Loads Mulder's Heroes add-on into Spider-Man.exe once iilyn's VR is running (his spidy_stereo_probe.dll is
    /// loaded). Read-only checks, then one LoadLibraryW in the game; every step has a time limit and never throws.</summary>
    class Sm1AddonWatcher
    {
        [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
        [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr addr, UIntPtr size, uint type, uint protect);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool VirtualFreeEx(IntPtr p, IntPtr addr, UIntPtr size, uint type);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool WriteProcessMemory(IntPtr p, IntPtr addr, byte[] data, UIntPtr size, out UIntPtr written);
        [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr CreateRemoteThread(IntPtr p, IntPtr attrs, UIntPtr stack, IntPtr start, IntPtr param, uint flags, IntPtr id);
        [DllImport("kernel32.dll", SetLastError = true)] static extern uint WaitForSingleObject(IntPtr h, uint ms);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool GetExitCodeThread(IntPtr h, out uint code);
        [DllImport("kernel32.dll", SetLastError = true)] static extern bool CloseHandle(IntPtr h);
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr GetModuleHandleW(string name);
        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true, ExactSpelling = true)] static extern IntPtr GetProcAddress(IntPtr module, string name);

        const uint ProcessAccess = 0x0002 | 0x0008 | 0x0020 | 0x0400 | 0x0010; // create thread, vm op, vm write, query, vm read
        const uint MemCommitReserve = 0x3000, MemRelease = 0x8000, PageReadWrite = 0x04;

        readonly string dll;
        readonly Action<string, bool> say;   // (line, good)
        volatile bool stop;
        Thread thread;
        /// <summary>How long to wait for the player to press PLAY in his launcher and his VR to start.</summary>
        public TimeSpan WaitFor = TimeSpan.FromMinutes(20);

        public Sm1AddonWatcher(string dll, Action<string, bool> say) { this.dll = dll; this.say = say; }

        public void Start()
        {
            thread = new Thread(Run) { IsBackground = true, Name = "SM1 add-on" };
            thread.Start();
        }

        public void Stop() { stop = true; }
        public bool Running { get { return thread != null && thread.IsAlive; } }

        void Say(string l, bool good) { try { if (say != null) say(l, good); } catch { } }

        static bool HasModule(Process p, string name)
        {
            try
            {
                foreach (ProcessModule m in p.Modules)
                    if (string.Equals(m.ModuleName, name, StringComparison.OrdinalIgnoreCase)) return true;
            }
            catch { }
            return false;
        }

        void Run()
        {
            try
            {
                if (!Environment.Is64BitProcess) { Say("Heroes add-on: this launcher runs as 32-bit, so it cannot load the add-on into the 64-bit game. iilyn's mod runs on its own.", false); return; }
                var until = DateTime.UtcNow + WaitFor;
                DateTime vrSeen = DateTime.MinValue;
                while (!stop && DateTime.UtcNow < until)
                {
                    Thread.Sleep(1000);
                    Process[] found;
                    try { found = Process.GetProcessesByName(Sm1.GameProcess); } catch { continue; }
                    try
                    {
                        if (found.Length == 0) { vrSeen = DateTime.MinValue; continue; }
                        var game = found[0];
                        if (HasModule(game, Sm1.AddonDll)) { Say("Heroes add-on: already loaded in the game.", true); return; }
                        if (!HasModule(game, Sm1.VrModule)) { vrSeen = DateTime.MinValue; continue; }
                        if (vrSeen == DateTime.MinValue) { vrSeen = DateTime.UtcNow; continue; }
                        if (DateTime.UtcNow - vrSeen < TimeSpan.FromSeconds(5)) continue;   // let his VR settle first
                        string problem;
                        if (Load(game.Id, dll, out problem)) Say("Heroes add-on: loaded into Spider-Man 1 (its report: %APPDATA%\\HERO VR SM1\\addon.log).", true);
                        else Say("Heroes add-on: not loaded (" + problem + "). iilyn's mod keeps running on its own.", false);
                        return;
                    }
                    finally { foreach (var x in found) try { x.Dispose(); } catch { } }
                }
                if (!stop) Say("Heroes add-on: iilyn's VR did not start within " + (int)WaitFor.TotalMinutes + " minutes, so the add-on was not loaded. Press Play again to retry.", false);
            }
            catch (Exception ex) { Say("Heroes add-on: stopped (" + ex.Message + "). iilyn's mod is not affected.", false); }
        }

        /// <summary>LoadLibraryW(dll) in the game (pid), waiting at most 10 s. False and a reason when it did not load.</summary>
        public static bool Load(int pid, string dll, out string problem)
        {
            problem = "";
            if (Environment.OSVersion.Platform != PlatformID.Win32NT) { problem = "Windows only"; return false; }
            if (string.IsNullOrEmpty(dll) || !File.Exists(dll)) { problem = "the add-on file is missing"; return false; }
            IntPtr proc = IntPtr.Zero, mem = IntPtr.Zero, th = IntPtr.Zero;
            try
            {
                IntPtr k32 = GetModuleHandleW("kernel32.dll");
                IntPtr loadLib = k32 == IntPtr.Zero ? IntPtr.Zero : GetProcAddress(k32, "LoadLibraryW");
                if (loadLib == IntPtr.Zero) { problem = "LoadLibraryW not found"; return false; }
                proc = OpenProcess(ProcessAccess, false, pid);
                if (proc == IntPtr.Zero) { problem = "Windows did not let the launcher open the game (" + new Win32Exception(Marshal.GetLastWin32Error()).Message + ")"; return false; }
                byte[] path = Encoding.Unicode.GetBytes(dll + "\0");
                mem = VirtualAllocEx(proc, IntPtr.Zero, (UIntPtr)(uint)path.Length, MemCommitReserve, PageReadWrite);
                if (mem == IntPtr.Zero) { problem = "no memory in the game for the file name"; return false; }
                UIntPtr written;
                if (!WriteProcessMemory(proc, mem, path, (UIntPtr)(uint)path.Length, out written) || (uint)written != (uint)path.Length) { problem = "could not pass the file name to the game"; return false; }
                th = CreateRemoteThread(proc, IntPtr.Zero, UIntPtr.Zero, loadLib, mem, 0, IntPtr.Zero);
                if (th == IntPtr.Zero) { problem = "Windows did not start the loader in the game (" + new Win32Exception(Marshal.GetLastWin32Error()).Message + ")"; return false; }
                if (WaitForSingleObject(th, 10000) != 0) { problem = "the game did not answer within 10 s"; mem = IntPtr.Zero; return false; }   // still running: leave its memory
                uint code;
                if (!GetExitCodeThread(th, out code) || code == 0) { problem = "the game could not load " + Sm1.AddonDll; return false; }
                return true;
            }
            catch (Exception ex) { problem = ex.Message; return false; }
            finally
            {
                if (th != IntPtr.Zero) CloseHandle(th);
                if (mem != IntPtr.Zero && proc != IntPtr.Zero) VirtualFreeEx(proc, mem, UIntPtr.Zero, MemRelease);
                if (proc != IntPtr.Zero) CloseHandle(proc);
            }
        }
    }
}
