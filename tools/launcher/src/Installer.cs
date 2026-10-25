// SM2 VR Launcher - installing an update zip, reverting to a backup, and putting everything back if a step fails.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Threading;

namespace Sm2VrLauncher
{
    class ZipCheck
    {
        public string Prefix = "";       // a single top folder to strip ("sm2-port/"), or ""
        public string Version;           // "Version 0.0.1" from VERSION / PORT-VERSION inside the zip, or null (an update that does not change it)
        public int Files;
        public bool LooksFull;           // has tools\launch-game-vr.ps1 and tools\build.ps1 and VERSION or PORT-VERSION
        public bool HasModFiles;         // carries the ready-made mod files (build/windows-ninja/spidy_stereo_probe.dll)
        public bool ModFilesOnly;        // only the mod files (HERO-VR-x-mod-win64.zip from tools\package.ps1): no VERSION, no build.ps1
    }

    /// <summary>The install folder's lock: <folder>\.sm2vr.lock, held open with no sharing while files change or a
    /// build runs, so two launchers (ours and a collaborator's) never change the same folder at once. A file left by
    /// a crash is not held by anyone, so it never blocks.</summary>
    sealed class FolderLock : IDisposable
    {
        public const string FileName = ".sm2vr.lock";
        FileStream stream;
        public static string PathFor(string root) { return Path.Combine(root, FileName); }

        public static FolderLock Take(string root, string what)
        {
            string p = PathFor(root);
            try
            {
                Directory.CreateDirectory(root);
                var fs = new FileStream(p, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None, 4096, FileOptions.DeleteOnClose);
                var text = System.Text.Encoding.UTF8.GetBytes("SM2 VR Launcher, pid " + System.Diagnostics.Process.GetCurrentProcess().Id + ", " + what + ", since " + DateTime.Now.ToString("o") + "\r\n");
                fs.SetLength(0); fs.Write(text, 0, text.Length); fs.Flush();
                return new FolderLock { stream = fs };
            }
            catch (IOException)
            {
                string who = "";
                try { using (var r = new FileStream(p, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete)) using (var sr = new StreamReader(r)) who = sr.ReadToEnd().Trim(); } catch { }
                throw new InvalidOperationException("Another launcher is updating or building " + root + " right now" + (who.Length > 0 ? " (" + who + ")" : "") + ". Wait for it to finish.");
            }
        }
        public static bool Busy(string root)
        {
            try { using (Take(root, "check")) return false; } catch (InvalidOperationException) { return true; } catch { return false; }
        }
        public void Dispose() { if (stream != null) { stream.Dispose(); stream = null; } }
    }

    class InstallResult
    {
        public bool Ok;
        public string Version, Message;
    }

    class Installer
    {
        public string Root;
        public Settings Settings;
        public Action<string> Log = s => { };
        public Action<string, bool> BuildOutput = (s, e) => { };
        public Action<string, int> Progress = (s, pct) => { };    // stage text, -1 = busy
        public CancellationToken Cancel;
        /// <summary>The manifest being installed (UPDATE NOW), whose "prebuilt" file is used when it is that version's.</summary>
        public Manifest Release;
        /// <summary>Downloads a manifest file and checks its SHA-256 (MainForm.DownloadVerified); returns the local path.</summary>
        public Func<ManifestFile, string> Download;

        // ---- Reading a zip -------------------------------------------------------------------------------

        public static ZipCheck Inspect(string zipPath)
        {
            var c = new ZipCheck();
            using (var z = ZipFile.OpenRead(zipPath))
            {
                var names = z.Entries.Where(e => e.Name.Length > 0).Select(e => e.FullName.Replace('\\', '/')).ToList();
                if (names.Count == 0) throw new InvalidDataException("The zip is empty.");
                Func<string, bool> has = n => names.Any(x => x.Equals(c.Prefix + n, StringComparison.OrdinalIgnoreCase));
                bool rootLooksRight = names.Any(x => x.Equals("PORT-VERSION", StringComparison.OrdinalIgnoreCase) || x.Equals("VERSION", StringComparison.OrdinalIgnoreCase) || x.StartsWith("tools/", StringComparison.OrdinalIgnoreCase)
                                                    || x.StartsWith("src/", StringComparison.OrdinalIgnoreCase) || x.StartsWith("python/", StringComparison.OrdinalIgnoreCase));
                if (!rootLooksRight)
                {
                    var tops = names.Select(x => x.Split(new[] { '/' })[0]).Distinct().ToList();
                    if (tops.Count == 1 && names.All(x => x.Contains("/"))) c.Prefix = tops[0] + "/";
                }
                foreach (var n in names)
                {
                    string rel = n.Substring(n.StartsWith(c.Prefix) ? c.Prefix.Length : 0);
                    if (rel.StartsWith("/") || rel.Contains(":") || rel.Split(new[] { '/' }).Any(p => p == ".."))
                        throw new InvalidDataException("The zip has an unsafe path (" + n + "); not installing it.");
                }
                c.Files = names.Count;
                Func<string, string> firstLine = file =>
                {
                    var e = z.Entries.FirstOrDefault(x => x.FullName.Replace('\\', '/').Equals(c.Prefix + file, StringComparison.OrdinalIgnoreCase));
                    if (e == null) return null;
                    using (var rd = new StreamReader(e.Open()))
                    {
                        string l;
                        while ((l = rd.ReadLine()) != null) { l = l.Trim().TrimStart(new[] { '\uFEFF' }); if (l.Length > 0) return l; }
                    }
                    return null;
                };
                string vLine = firstLine("VERSION"), pLine = firstLine("PORT-VERSION");
                var mv = ModVersion.FromFiles(vLine, pLine);
                c.Version = mv != null ? mv.Display : (vLine ?? pLine);
                c.HasModFiles = has("build/windows-ninja/spidy_stereo_probe.dll");
                c.LooksFull = (has("VERSION") || has("PORT-VERSION")) && has("tools/launch-game-vr.ps1") && has("tools/build.ps1");
                c.ModFilesOnly = c.HasModFiles && !has("VERSION") && !has("tools/build.ps1");
            }
            return c;
        }

        /// <summary>Expand-Archive -Force: every file in the zip over the install (others untouched).</summary>
        int Extract(string zipPath, ZipCheck c, string dest, string[] skipParts = null)
        {
            int n = 0;
            string full = Path.GetFullPath(dest).TrimEnd(new[] { '\\', '/' }) + Path.DirectorySeparatorChar;
            using (var z = ZipFile.OpenRead(zipPath))
            {
                var entries = z.Entries.Where(e => e.Name.Length > 0).ToList();
                foreach (var e in entries)
                {
                    Cancel.ThrowIfCancellationRequested();
                    string rel = e.FullName.Replace('\\', '/');
                    if (c.Prefix.Length > 0 && rel.StartsWith(c.Prefix)) rel = rel.Substring(c.Prefix.Length);
                    if (skipParts != null && skipParts.Any(pt => rel.StartsWith(pt, StringComparison.OrdinalIgnoreCase))) continue;   // installed by ModFiles.Install
                    string to = Path.GetFullPath(Path.Combine(dest, rel.Replace('/', Path.DirectorySeparatorChar)));
                    if (!to.StartsWith(full, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Unsafe path in zip: " + e.FullName);
                    Directory.CreateDirectory(Path.GetDirectoryName(to));
                    if (Backups.IsSelf(to)) Backups.MoveSelfAside(to);
                    if (File.Exists(to)) File.SetAttributes(to, FileAttributes.Normal);
                    e.ExtractToFile(to, true);
                    n++;
                    if (n % 25 == 0) Progress("Extracting " + n + " / " + entries.Count, (int)(100.0 * n / entries.Count));
                }
            }
            return n;
        }

        // ---- Building ------------------------------------------------------------------------------------

        /// <summary>Developers only: compiles the mod from source (Visual Studio C++ and git). Players never get here.</summary>
        public int Build()
        {
            // Called inside a transaction (which holds the folder lock).
            Progress("Building the VR mod from source (tools\\build.ps1 -Observer)...", -1);
            Log("Building from source: tools\\build.ps1 -Observer (developer PC; a fresh build takes a few minutes).");
            return PsRunner.RunAndWait(Root, Path.Combine(Root, "tools", "build.ps1"), BuildArgs(Root), BuildOutput, Cancel);
        }

        /// <summary>-Observer, and -FromSource when the folder's build.ps1 knows it (from 0.0.5 hotfix 2 it otherwise
        /// installs the ready-made files when the launcher starts it).</summary>
        public static string[] BuildArgs(string root)
        {
            bool knows = false;
            try { knows = File.ReadAllText(Path.Combine(root, "tools", "build.ps1")).Contains("FromSource"); } catch { }
            return knows ? new[] { "-Observer", "-FromSource" } : new[] { "-Observer" };
        }

        // ---- The ready-made mod files ---------------------------------------------------------------------

        /// <summary>Downloads the installed version's ready-made mod files and installs them (lock held by the caller).
        /// False when that release has none; throws when they could not be installed.</summary>
        public bool InstallModFiles()
        {
            Progress("Finding the mod files for " + (SystemInfo.PortVersion(Root) ?? "this version") + "...", -1);
            Manifest m;
            try { m = ModFiles.ManifestFor(Root, Settings, Release, Log); }
            catch (System.Net.WebException ex)
            {
                var resp = ex.Response as System.Net.HttpWebResponse;
                if (resp != null && resp.StatusCode == System.Net.HttpStatusCode.NotFound) { Log("This version's release has no version.json."); return false; }
                throw new Exception("Could not reach GitHub to download the mod files (" + ex.Message + "). Check the internet connection and try again.");
            }
            var f = m.File(ModFiles.Kind);
            if (f == null) { Log(m.Version + "'s release lists no ready-made mod files."); return false; }
            if (Download == null) throw new InvalidOperationException("No downloader.");
            string zip = Download(f);
            Cancel.ThrowIfCancellationRequested();
            ModFiles.Install(Root, zip, f.Sha256, Log, Progress);
            return true;
        }

        /// <summary>The mod files for the version now in the folder: the ones in the zip being installed, else the working
        /// ones kept when the version did not change, else the release's ready-made files. Never compiles (players have no
        /// build tools; developers use Build from source).</summary>
        void PrepareMod(string beforeLong, bool parked, string modZip)
        {
            if (modZip != null)
            {
                ModFiles.Install(Root, modZip, null, Log, Progress);
                return;
            }
            string nowLong = SystemInfo.PortVersionLong(Root);
            string parkedDll = Path.Combine(Backups.TxnFolder(Root), "build", "windows-ninja", "spidy_stereo_probe.dll");
            if (parked && nowLong != null && string.Equals(nowLong, beforeLong, StringComparison.OrdinalIgnoreCase) && File.Exists(parkedDll))
            {
                Backups.UnparkBuild(Root, Log);
                Log("Same mod version (" + nowLong + "): kept its working mod files.");
                return;
            }
            if (InstallModFiles()) return;
            throw new Exception("The release of " + (nowLong ?? "this version") + " has no ready-made mod files yet, so the mod can't be installed on this PC. " +
                                "Nothing needs installing on your side: try again later, or use the version you had.");
        }

        /// <summary>A failed first install leaves no half-installed mod files behind.</summary>
        void DropNewBuild() { try { string b = Path.Combine(Root, "build"); if (Directory.Exists(b)) Directory.Delete(b, true); } catch { } }

        /// <summary>REPAIR MOD FILES / first start / PLAY VR: (re)installs this version's ready-made mod files. The working
        /// files are put back if anything fails.</summary>
        public InstallResult RepairModFiles() { return RepairModFiles(null); }

        /// <summary>Installs the mod files for the version in the folder: from `localZip` (a HERO-VR-x-mod-win64.zip the
        /// player picked) when given, else downloaded from the release. Nothing but the mod files changes.</summary>
        public InstallResult RepairModFiles(string localZip)
        {
            RequireGameClosed();
            using (FolderLock.Take(Root, "installing the mod files"))
            {
                string v = SystemInfo.PortVersion(Root) ?? "the mod";
                bool parked = false, hadBuild = Directory.Exists(Path.Combine(Root, "build"));
                try
                {
                    Backups.ParkBuild(Root, Log);
                    parked = true;
                    if (localZip != null) ModFiles.Install(Root, localZip, null, Log, Progress);
                    else if (!InstallModFiles())
                        throw new Exception("The release of " + v + " has no ready-made mod files yet. Nothing needs installing on your side: try again later.");
                    Backups.DropParkedBuild(Root);
                    Progress("Mod files ready: " + v, 100);
                    return new InstallResult { Ok = true, Version = v, Message = "The mod files for " + v + " are installed. PLAY VR is ready." };
                }
                catch (Exception ex)
                {
                    string why = ex is OperationCanceledException ? "Cancelled." : ex.Message;
                    Log("FAILED: " + why);
                    try { if (parked && !hadBuild) DropNewBuild(); if (parked) Backups.UnparkBuild(Root, Log); } catch (Exception rb) { Log("ROLLBACK PROBLEM: " + rb.Message); }
                    Progress("Mod files not installed.", 0);
                    return new InstallResult { Ok = false, Version = v, Message = "Installing the mod files failed: " + why };
                }
            }
        }

        void RequireGameClosed()
        {
            if (SystemInfo.GameRunning()) throw new InvalidOperationException("Spider-Man 2 is running. Close the game first; files it uses can't be replaced.");
        }

        // ---- The one transaction every change goes through ------------------------------------------------

        /// <summary>Backs up the current version, applies `change`, builds, checks the version; on any failure
        /// puts back exactly what was there (files and the working build).</summary>
        InstallResult Transaction(string what, string reason, string expectVersion, Action change, string[] protect, string modZip = null)
        {
            RequireGameClosed();
            if (!Directory.Exists(Root)) Directory.CreateDirectory(Root);
            using (FolderLock.Take(Root, what))
                return TransactionLocked(what, reason, expectVersion, change, protect, modZip);
        }

        InstallResult TransactionLocked(string what, string reason, string expectVersion, Action change, string[] protect, string modZip)
        {
            if (Backups.FinishDeferred(Root, Log) > 0)
                throw new IOException("Some files from the last rollback are still open in another program (see the log). Close it, or restart the PC, and try again. Nothing was changed.");
            string before = SystemInfo.PortVersion(Root) ?? "unknown";
            string beforeLong = SystemInfo.PortVersionLong(Root);
            Progress("Backing up " + before + "...", -1);
            BackupInfo snap;
            string expectedName = expectVersion == null ? null : Backups.SafeName(expectVersion);
            if (expectedName != null && (ModVersion.Same(expectVersion, before) || expectedName.Equals(Backups.SafeName(before), StringComparison.OrdinalIgnoreCase)))
                snap = Backups.CreateAt(Path.Combine(Backups.TxnFolder(Root), "before"), Root, before, reason, Log);  // don't overwrite the target's backup
            else
                snap = Backups.Create(Root, before, reason, Log);
            Backups.WriteMarker(Root, snap.Folder, what);
            bool parked = false, hadBuild = Directory.Exists(Path.Combine(Root, "build"));
            try
            {
                Backups.ParkBuild(Root, Log);
                parked = true;
                change();
                Cancel.ThrowIfCancellationRequested();
                PrepareMod(beforeLong, parked, modZip);
                Cancel.ThrowIfCancellationRequested();
                string now = SystemInfo.PortVersion(Root);
                if (expectVersion != null && !ModVersion.Same(now, expectVersion))
                    throw new Exception("After installing, the mod reads \"" + (now ?? "no version") + "\" (VERSION / PORT-VERSION), not \"" + expectVersion + "\".");
                Backups.DropParkedBuild(Root);
                Backups.ClearMarker(Root);
                var keep = new List<string> { snap.Folder };
                if (protect != null) keep.AddRange(protect);
                Backups.Prune(Root, Settings.KeepBackups, keep.ToArray(), Log);
                if (snap.Folder.StartsWith(Backups.TxnFolder(Root))) try { Directory.Delete(snap.Folder, true); } catch { }
                Progress("Done: " + now, 100);
                return new InstallResult { Ok = true, Version = now, Message = what + " finished: " + now + " is installed, with its mod files." };
            }
            catch (Exception ex)
            {
                string why = ex is OperationCanceledException ? "Cancelled." : ex.Message;
                Log("FAILED: " + why);
                Log("Rolling back to " + before + "...");
                Progress("Rolling back to " + before + "...", -1);
                try
                {
                    int later = Backups.Restore(Root, snap.Folder, new string[0], true, Log, true);
                    if (parked && !hadBuild) DropNewBuild();
                    if (parked) Backups.UnparkBuild(Root, Log);
                    Backups.ClearMarker(Root);
                    Log("Rolled back: " + (SystemInfo.PortVersion(Root) ?? before) + " is installed again, with its mod files.");
                    Progress("Rolled back to " + before, 0);
                    return new InstallResult { Ok = false, Version = before, Message = what + " failed: " + why + "\n\nNothing changed: " + before + " and its mod files were put back." +
                        (later > 0 ? " (" + later + " file" + (later == 1 ? " was" : "s were") + " open in another program; the launcher puts " + (later == 1 ? "it" : "them") + " back the next time it starts.)" : "") };
                }
                catch (Exception rb)
                {
                    Log("ROLLBACK PROBLEM: " + rb.Message);
                    return new InstallResult
                    {
                        Ok = false, Version = SystemInfo.PortVersion(Root),
                        Message = what + " failed: " + why + "\n\nPutting the previous files back also failed: " + rb.Message +
                                  "\n\nYour files are safe in " + snap.Folder + ". Close anything using " + Root + " and restart the launcher: it finishes the rollback on start."
                    };
                }
            }
        }

        /// <summary>INSTALL UPDATE / UPDATE NOW: the zip over the install (as Expand-Archive -Force), then its mod files (never a build).</summary>
        public InstallResult InstallZip(string zipPath, string reason)
        {
            Progress("Checking the zip...", -1);
            var c = Inspect(zipPath);
            Log("Zip: " + Path.GetFileName(zipPath) + ", " + c.Files + " files" + (c.Version != null ? ", " + c.Version : "") + ".");
            if (c.ModFilesOnly)
            {
                Log("It is the ready-made mod files only: installing them (nothing else changes, nothing to build).");
                return RepairModFiles(zipPath);
            }
            if (c.HasModFiles) Log("It carries the ready-made mod files: installing them from it (nothing to build or download).");
            return Transaction("Installing " + (c.Version ?? Path.GetFileName(zipPath)), reason, c.Version, () =>
            {
                int n = Extract(zipPath, c, Root, c.HasModFiles ? ModFiles.Parts : null);
                Log("Expanded " + n + " files into " + Root + ".");
            }, null, c.HasModFiles ? zipPath : null);
        }

        /// <summary>A full package (from an older release) made the install exactly: like a backup restore.</summary>
        public InstallResult InstallFull(string zipPath, bool restoreSettings, string reason)
        {
            Progress("Checking the package...", -1);
            var c = Inspect(zipPath);
            if (!c.LooksFull) return InstallZip(zipPath, reason);
            string tmp = Path.Combine(Backups.TxnFolder(Root), "package");
            return Transaction("Installing " + (c.Version ?? "the package"), reason, c.Version, () =>
            {
                if (Directory.Exists(tmp)) Directory.Delete(tmp, true);
                Directory.CreateDirectory(tmp);
                Extract(zipPath, c, tmp, c.HasModFiles ? ModFiles.Parts : null);
                Backups.Restore(Root, tmp, Settings.PreservePatterns, restoreSettings, Log);
                try { Directory.Delete(tmp, true); } catch { }
            }, null, c.HasModFiles ? zipPath : null);
        }

        /// <summary>REVERT: the chosen backup made the install exactly (settings kept unless restoreSettings).</summary>
        public InstallResult Revert(BackupInfo b, bool restoreSettings)
        {
            if (!Directory.Exists(b.Folder)) throw new DirectoryNotFoundException("The backup folder is gone: " + b.Folder);
            return Transaction("Reverting to " + b.Version, "before reverting to " + b.Version, b.Version, () =>
            {
                Log("Restoring " + b.Version + " from " + b.Folder + (restoreSettings ? " (with that version's settings)" : " (keeping your current settings)") + ".");
                Backups.Restore(Root, b.Folder, Settings.PreservePatterns, restoreSettings, Log);
            }, new[] { b.Folder });
        }

        /// <summary>After a crash or power cut mid-update: put back the backup the update started from.</summary>
        public InstallResult FinishRollback(string folder)
        {
            using (FolderLock.Take(Root, "finishing a rollback"))
                return FinishRollbackLocked(folder);
        }

        InstallResult FinishRollbackLocked(string folder)
        {
            Progress("Finishing an interrupted update's rollback...", -1);
            Backups.FinishDeferred(Root, Log);
            int later = Backups.Restore(Root, folder, new string[0], true, Log, true);
            Backups.UnparkBuild(Root, Log);
            Backups.ClearMarker(Root);
            string v = SystemInfo.PortVersion(Root);
            return new InstallResult { Ok = true, Version = v, Message = "The interrupted update was rolled back: " + v + " is installed again." +
                (later > 0 ? " (" + later + " file(s) open in another program are put back the next time the launcher starts.)" : "") };
        }
    }
}
