// SM2 VR Launcher - version backups in C:\sm2-port-backup\<version>, and restoring one exactly (mirror copy).
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace Sm2VrLauncher
{
    class BackupInfo
    {
        public string Folder, Version, Reason;
        public DateTime Created;
        public int Files;
        public override string ToString() { return Version + "  \u00B7  " + Created.ToString("ddd d MMM yyyy, HH:mm") + (string.IsNullOrEmpty(Reason) ? "" : "  \u00B7  " + Reason); }
    }

    static class Backups
    {
        /// <summary>0.0.74 test 10 MUSIC KEPT: the install's own songs folder (the songs now live in Documents\HERO VR\Music;
        /// this one is still read): never written by an install, copied into a backup or deleted by a restore.</summary>
        public const string MusicFolder = "Music";
        /// <summary>Top folders that are never copied into a backup or deleted by a restore: the build (rebuilt
        /// every time), reports, the user's music, and development-only folders.</summary>
        public static readonly string[] SkipTop = { "build", "reports", "music", ".git", ".vs", "dist", "third_party", ".research", "test logs", "__pycache__", ".sm2vr.lock" };

        public static string BackupRoot(string portRoot)
        {
            string full = Path.GetFullPath(portRoot).TrimEnd(new[] { '\\', '/' });
            return full + "-backup";   // C:\sm2-port -> C:\sm2-port-backup
        }
        public static string TxnFolder(string portRoot) { return Path.Combine(BackupRoot(portRoot), "_txn"); }
        public static string MarkerFile(string portRoot) { return Path.Combine(BackupRoot(portRoot), "_txn", "IN-PROGRESS.txt"); }

        static bool Skipped(string rel)
        {
            string top = rel.Split(new[] { '\\', '/' })[0].ToLowerInvariant();
            if (SkipTop.Contains(top)) return true;
            if (rel.IndexOf("__pycache__", StringComparison.OrdinalIgnoreCase) >= 0) return true;
            if (rel.IndexOf("launcher-data", StringComparison.OrdinalIgnoreCase) >= 0) return true;   // a portable launcher's own logs and downloads
            if (IsLauncherLeftover(rel)) return true;
            return false;
        }

        /// <summary>SM2VRLauncher.exe.old / SM2VRLauncher.exe.123.old: the previous launcher exe moved aside by a self-update,
        /// possibly still running. Never backed up, restored or deleted by a restore (CleanOldSelf removes it on start).</summary>
        public static bool IsLauncherLeftover(string rel)
        {
            string name = Path.GetFileName(rel.Replace('/', '\\').Replace('\\', Path.DirectorySeparatorChar));
            return name.StartsWith("SM2VRLauncher", StringComparison.OrdinalIgnoreCase) && name.EndsWith(".old", StringComparison.OrdinalIgnoreCase);
        }

        public static bool MatchesAny(string rel, string[] patterns)
        {
            string name = Path.GetFileName(rel);
            foreach (var p in patterns)
            {
                string rx = "^" + Regex.Escape(p.Trim()).Replace("\\*", ".*").Replace("\\?", ".") + "$";
                if (Regex.IsMatch(name, rx, RegexOptions.IgnoreCase) || Regex.IsMatch(rel.Replace('/', '\\'), rx, RegexOptions.IgnoreCase)) return true;
            }
            return false;
        }

        /// <summary>Relative paths of the install's own files (not build, reports, music...).</summary>
        public static List<string> InstallFiles(string root)
        {
            var list = new List<string>();
            string full = Path.GetFullPath(root).TrimEnd(new[] { '\\', '/' }) + Path.DirectorySeparatorChar;
            if (!Directory.Exists(root)) return list;
            var stack = new Stack<string>();
            stack.Push(root);
            while (stack.Count > 0)
            {
                string d = stack.Pop();
                foreach (var f in Directory.GetFiles(d))
                {
                    string rel = f.Substring(full.Length);
                    if (!Skipped(rel)) list.Add(rel);
                }
                foreach (var sub in Directory.GetDirectories(d))
                {
                    string rel = sub.Substring(full.Length);
                    if (Skipped(rel)) continue;
                    if ((new DirectoryInfo(sub).Attributes & FileAttributes.ReparsePoint) != 0) continue;  // never follow links
                    stack.Push(sub);
                }
            }
            return list;
        }

        public static string SafeName(string version)
        {
            string s = Regex.Replace(version ?? "unknown", @"[^\w.\- ]", "_").Trim();
            return s.Length == 0 ? "unknown" : s;
        }

        /// <summary>Copies the install's files to <backup>\<version>; replaces an older backup of the same version.</summary>
        public static BackupInfo Create(string root, string version, string reason, Action<string> log)
        {
            return CreateAt(Path.Combine(BackupRoot(root), SafeName(version)), root, version, reason, log);
        }

        public static BackupInfo CreateAt(string dest, string root, string version, string reason, Action<string> log)
        {
            string tmp = dest + ".partial";
            if (Directory.Exists(tmp)) Directory.Delete(tmp, true);
            var files = InstallFiles(root);
            log("Backing up " + version + " (" + files.Count + " files) to " + dest);
            foreach (var rel in files)
            {
                string to = Path.Combine(tmp, rel);
                Directory.CreateDirectory(Path.GetDirectoryName(to));
                File.Copy(Path.Combine(root, rel), to, true);
            }
            var info = new BackupInfo { Folder = dest, Version = version, Reason = reason, Created = DateTime.Now, Files = files.Count };
            File.WriteAllText(Path.Combine(tmp, "_backup.ini"),
                "Version=" + version + "\r\nCreated=" + info.Created.ToString("o", CultureInfo.InvariantCulture) + "\r\nReason=" + reason + "\r\nFiles=" + files.Count + "\r\n",
                new UTF8Encoding(false));
            if (Directory.Exists(dest)) Directory.Delete(dest, true);
            Directory.Move(tmp, dest);
            return info;
        }

        public static List<BackupInfo> List(string root)
        {
            var result = new List<BackupInfo>();
            string br = BackupRoot(root);
            if (!Directory.Exists(br)) return result;
            foreach (var d in Directory.GetDirectories(br))
            {
                string name = Path.GetFileName(d);
                if (name.StartsWith("_") || name.EndsWith(".partial")) continue;
                var b = new BackupInfo { Folder = d, Version = name, Created = Directory.GetCreationTime(d) };
                try
                {
                    string ini = Path.Combine(d, "_backup.ini");
                    if (File.Exists(ini))
                        foreach (var l in File.ReadAllLines(ini))
                        {
                            int eq = l.IndexOf('='); if (eq < 0) continue;
                            string k = l.Substring(0, eq), v = l.Substring(eq + 1);
                            if (k == "Version") b.Version = v;
                            else if (k == "Reason") b.Reason = v;
                            else if (k == "Files") int.TryParse(v, out b.Files);
                            else if (k == "Created") { DateTime t; if (DateTime.TryParse(v, CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind, out t)) b.Created = t.ToLocalTime(); }
                        }
                    else if (SystemInfo.PortVersion(d) != null) b.Version = SystemInfo.PortVersion(d);
                }
                catch { }
                result.Add(b);
            }
            return result.OrderByDescending(b => b.Created).ToList();
        }

        /// <summary>Keeps the newest `keep` backups (never `protect`, the one just made).</summary>
        public static void Prune(string root, int keep, string[] protect, Action<string> log)
        {
            var all = List(root);
            int kept = 0;
            foreach (var b in all)
            {
                bool prot = protect != null && protect.Any(p => string.Equals(b.Folder, p, StringComparison.OrdinalIgnoreCase));
                if (prot || kept < keep) { kept++; continue; }
                try { Directory.Delete(b.Folder, true); log("Removed old backup " + b.Version + " (keeping the newest " + keep + ")."); }
                catch (Exception ex) { log("Could not remove old backup " + b.Folder + ": " + ex.Message); }
            }
        }

        /// <summary>Makes the install exactly the backup's files: files not in the backup are removed (except build,
        /// reports, music and the preserved settings), every backed-up file is copied back. Preserved settings files
        /// keep the user's current copy unless restoreSettings. A file another program holds open is tried again for a
        /// few seconds, then deferred: saved in the backups' _deferred folder and finished the next time the launcher
        /// starts (FinishDeferred). The running launcher's own files are left as they are. Returns the deferred count.</summary>
        public static int Restore(string root, string backupFolder, string[] preserve, bool restoreSettings, Action<string> log, bool deferLocked = false)
        {
            var wanted = new HashSet<string>(InstallFiles(backupFolder).Where(r => !r.Equals("_backup.ini", StringComparison.OrdinalIgnoreCase)), StringComparer.OrdinalIgnoreCase);
            int removed = 0, copied = 0, deferred = 0, kept = 0;
            foreach (var rel in InstallFiles(root))
            {
                if (wanted.Contains(rel)) continue;
                if (MatchesAny(rel, preserve)) continue;
                string path = Path.Combine(root, rel);
                if (IsSelf(path) || InSelfFolder(path)) continue;   // never delete the running launcher's own files
                if (TryIo(rel, () => { File.SetAttributes(path, FileAttributes.Normal); File.Delete(path); })) removed++;
                else if (deferLocked) { Defer(root, rel, null, log); deferred++; }
                else throw Locked(rel);
            }
            foreach (var rel in wanted)
            {
                string to = Path.Combine(root, rel), from = Path.Combine(backupFolder, rel);
                if (!restoreSettings && MatchesAny(rel, preserve) && File.Exists(to)) continue;
                Directory.CreateDirectory(Path.GetDirectoryName(to));
                bool self = IsSelf(to);
                bool ok = TryIo(rel, () =>
                {
                    if (self) MoveSelfAside(to);
                    if (File.Exists(to)) File.SetAttributes(to, FileAttributes.Normal);
                    File.Copy(from, to, true);
                });
                if (ok) { copied++; continue; }
                if (self || InSelfFolder(to)) { kept++; log("Kept " + rel + " as it is (the running launcher is using it)."); continue; }
                if (!deferLocked) throw Locked(rel);
                Defer(root, rel, from, log); deferred++;
            }
            RemoveEmptyFolders(root);
            log("Restored " + copied + " files" + (removed > 0 ? ", removed " + removed + " that the version did not have" : "") +
                (deferred > 0 ? "; " + deferred + " in use by another program, finished when the launcher next starts" : "") + ".");
            return deferred;
        }

        /// <summary>Runs a file operation, trying again for about 3 s while another program holds the file. False when it
        /// is still locked (sharing violation or access denied); other errors (disk full, missing backup file) throw.</summary>
        static bool TryIo(string rel, Action io)
        {
            string test = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_LOCKED");   // automated tests only: pretend these are open
            for (int i = 0; ; i++)
            {
                try
                {
                    if (!string.IsNullOrEmpty(test) && MatchesAny(rel, test.Split(new[] { ';' })))
                        throw new IOException("The process cannot access the file '" + rel + "' because it is being used by another process. (test)");
                    io();
                    return true;
                }
                catch (FileNotFoundException) { throw; }
                catch (DirectoryNotFoundException) { throw; }
                catch (Exception ex)
                {
                    if (!(ex is IOException) && !(ex is UnauthorizedAccessException)) throw;
                    if (i >= (string.IsNullOrEmpty(test) ? 15 : 1)) return false;
                    System.Threading.Thread.Sleep(200);
                }
            }
        }

        static IOException Locked(string rel)
        {
            return new IOException(rel + " is open in another program, so it can't be replaced. Close that program (or restart the PC) and try again.");
        }

        public static string DeferredFolder(string root) { return Path.Combine(BackupRoot(root), "_deferred"); }
        static string DeferredList(string root) { return Path.Combine(DeferredFolder(root), "deferred.txt"); }

        /// <summary>Remembers one file a restore could not change: copy (the backup's copy is saved) or delete (from == null).</summary>
        static void Defer(string root, string rel, string from, Action<string> log)
        {
            string dir = DeferredFolder(root);
            Directory.CreateDirectory(dir);
            if (from != null)
            {
                string keep = Path.Combine(dir, "files", rel);
                Directory.CreateDirectory(Path.GetDirectoryName(keep));
                File.Copy(from, keep, true);
            }
            File.AppendAllText(DeferredList(root), (from != null ? "copy|" : "delete|") + rel + "\r\n", new UTF8Encoding(false));
            log("In use by another program: " + rel + ". It is put back when the launcher next starts (nothing is lost).");
        }

        public static bool HasDeferred(string root) { return File.Exists(DeferredList(root)); }

        /// <summary>Finishes the files an earlier rollback deferred. Returns how many are still in use (0 = all done).</summary>
        public static int FinishDeferred(string root, Action<string> log)
        {
            string list = DeferredList(root);
            if (!File.Exists(list)) return 0;
            var left = new List<string>();
            int done = 0;
            foreach (var line in File.ReadAllLines(list).Where(l => l.Contains("|")).Distinct())
            {
                string op = line.Substring(0, line.IndexOf('|')), rel = line.Substring(line.IndexOf('|') + 1);
                string to = Path.Combine(root, rel), from = Path.Combine(DeferredFolder(root), "files", rel);
                bool ok;
                try
                {
                    ok = TryIo(rel, () =>
                    {
                        if (op == "copy")
                        {
                            if (!File.Exists(from)) return;
                            Directory.CreateDirectory(Path.GetDirectoryName(to));
                            if (File.Exists(to)) File.SetAttributes(to, FileAttributes.Normal);
                            File.Copy(from, to, true);
                        }
                        else if (File.Exists(to)) { File.SetAttributes(to, FileAttributes.Normal); File.Delete(to); }
                    });
                }
                catch (Exception ex) { log("Could not finish " + rel + ": " + ex.Message); ok = false; }
                if (ok) done++; else left.Add(line);
            }
            if (left.Count == 0) { try { Directory.Delete(DeferredFolder(root), true); } catch { } }
            else File.WriteAllLines(list, left.ToArray());
            if (done > 0) log("Finished the last rollback: put back " + done + (done == 1 ? " file that was" : " files that were") + " in use.");
            return left.Count;
        }

        static void RemoveEmptyFolders(string root)
        {
            foreach (var d in Directory.GetDirectories(root))
            {
                string name = Path.GetFileName(d).ToLowerInvariant();
                if (SkipTop.Contains(name)) continue;
                try { RemoveEmptyFolders(d); if (!Directory.EnumerateFileSystemEntries(d).Any()) Directory.Delete(d); } catch { }
            }
        }

        // ---- The launcher's own exe may live inside the install (tools\launcher) ---------------------------

        public static bool IsSelf(string path)
        {
            try { return string.Equals(Path.GetFullPath(path), Path.GetFullPath(System.Reflection.Assembly.GetExecutingAssembly().Location), StringComparison.OrdinalIgnoreCase); }
            catch { return false; }
        }
        /// <summary>True for files in the running launcher's own folder (launcher.ini, launcher-data) when that folder is inside an install.</summary>
        public static bool InSelfFolder(string path)
        {
            try
            {
                string dir = Path.GetFullPath(AppDomain.CurrentDomain.BaseDirectory).TrimEnd(new[] { '\\', '/' }) + Path.DirectorySeparatorChar;
                return Path.GetFullPath(path).StartsWith(dir, StringComparison.OrdinalIgnoreCase);
            }
            catch { return false; }
        }
        /// <summary>Windows lets a running exe be renamed but not overwritten: rename it, the new copy runs next time.</summary>
        /// <summary>True once this run has moved its own exe aside for a new copy: the launcher must restart.</summary>
        public static bool SelfReplaced;
        public static void MoveSelfAside(string path)
        {
            SelfReplaced = true;
            string old = path + ".old";
            try { if (File.Exists(old)) File.Delete(old); } catch { old = path + "." + DateTime.Now.Ticks + ".old"; }
            if (File.Exists(path)) File.Move(path, old);
        }

        /// <summary>Deletes SM2VRLauncher.exe*.old left by an earlier self-update (the old exe could not be deleted while it ran).</summary>
        public static void CleanOldSelf()
        {
            try
            {
                string dir = AppDomain.CurrentDomain.BaseDirectory;
                foreach (var f in Directory.GetFiles(dir, "*.old")) try { File.Delete(f); } catch { }
            }
            catch { }
        }

        // ---- The build folder during a transaction ---------------------------------------------------------

        /// <summary>Moves build\ aside (same drive: a rename) so a failed update can put the working build back.</summary>
        public static void ParkBuild(string root, Action<string> log)
        {
            string build = Path.Combine(root, "build"), parked = Path.Combine(TxnFolder(root), "build");
            if (Directory.Exists(parked)) Directory.Delete(parked, true);
            if (!Directory.Exists(build)) return;
            Directory.CreateDirectory(TxnFolder(root));
            try { Directory.Move(build, parked); log("Moved the current build aside (it comes back if anything fails)."); }
            catch (Exception ex) { throw new IOException("Could not move the build folder aside (is a file in it open? close the game and any terminal in it): " + ex.Message); }
        }
        public static void UnparkBuild(string root, Action<string> log)
        {
            string build = Path.Combine(root, "build"), parked = Path.Combine(TxnFolder(root), "build");
            if (!Directory.Exists(parked)) return;
            try { if (Directory.Exists(build)) Directory.Delete(build, true); } catch { }
            Directory.Move(parked, build);
            log("Put the previous working build back.");
        }
        public static void DropParkedBuild(string root)
        {
            try { string parked = Path.Combine(TxnFolder(root), "build"); if (Directory.Exists(parked)) Directory.Delete(parked, true); } catch { }
        }

        public static void WriteMarker(string root, string rollbackFolder, string what)
        {
            Directory.CreateDirectory(TxnFolder(root));
            File.WriteAllText(MarkerFile(root), "Rollback=" + rollbackFolder + "\r\nWhat=" + what + "\r\nStarted=" + DateTime.Now.ToString("o") + "\r\n");
        }
        public static void ClearMarker(string root) { try { File.Delete(MarkerFile(root)); } catch { } }
        /// <summary>The rollback folder of an update the launcher did not finish (crash, power cut), or null.</summary>
        public static string PendingRollback(string root, out string what)
        {
            what = null;
            try
            {
                string m = MarkerFile(root);
                if (!File.Exists(m)) return null;
                string folder = null;
                foreach (var l in File.ReadAllLines(m))
                {
                    if (l.StartsWith("Rollback=")) folder = l.Substring(9);
                    else if (l.StartsWith("What=")) what = l.Substring(5);
                }
                return folder;
            }
            catch { return null; }
        }
    }
}
