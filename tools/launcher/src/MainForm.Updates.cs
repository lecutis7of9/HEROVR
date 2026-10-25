// SM2 VR Launcher - the Updates sheet: GitHub Releases (version.json), Downloads zips, build, revert.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    partial class MainForm
    {
        LogBox buildLog;
        StatusRow upInstalled, upLatest, upDownloads;
        AccentButton btnCheck, btnUpdateNow, btnInstallZip, btnBuildMod, btnRevert, btnBackups, btnSaveRepo;
        ProgressStrip progress;
        TextLabel progressText, backupsNote;
        ToggleRow checkOnStart;
        Field repoField;
        Manifest latest;
        CancellationTokenSource cancel;
        FileInfo downloadZip;

        void BuildUpdates()
        {
            var p = pages[2];
            var card = Add(p, new GlassPanel("Updates"), 0, 0, 1108, 556);
            upInstalled = Add(card, new StatusRow("Installed"), 20, 44, 500, 54);
            upLatest = Add(card, new StatusRow("Latest on GitHub"), 20, 98, 500, 54);
            btnCheck = Add(card, new AccentButton("Check for update", ButtonKind.Secondary) { Glyph = "refresh" }, 20, 162, 240, 44);
            btnUpdateNow = Add(card, new AccentButton("Update now", ButtonKind.Primary) { Glyph = "download", TextFont = Theme.Display(17) }, 270, 162, 250, 44);
            progress = Add(card, new ProgressStrip(), 20, 216, 500, 6);
            progressText = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub) { Wrap = false }, 20, 226, 500, 18);
            upDownloads = Add(card, new StatusRow("Update zip in Downloads"), 20, 248, 500, 58);
            btnInstallZip = new AccentButton("Install", ButtonKind.Secondary) { Glyph = "download" };
            upDownloads.AddAction(btnInstallZip);
            checkOnStart = Add(card, new ToggleRow("Check for updates when the launcher starts", "Looks at the GitHub release once; nothing installs without you pressing Update now.",
                settings.GetBool("CheckOnStart", false)), 20, 310, 500, 58);
            Add(card, new TextLabel("GITHUB REPOSITORY (OWNER/NAME)", Theme.UiBold(11), Theme.Silver) { Wrap = false }, 20, 374, 300, 18);
            repoField = Add(card, new Field(), 20, 394, 390, 34);
            repoField.Text = settings.UpdateRepo;
            btnSaveRepo = Add(card, new AccentButton("Save", ButtonKind.Secondary), 420, 394, 100, 34);
            btnBuildMod = Add(card, new AccentButton("Build mod", ButtonKind.Secondary) { Glyph = "build" }, 20, 442, 160, 40);
            btnRevert = Add(card, new AccentButton("Revert", ButtonKind.Danger) { Glyph = "revert" }, 190, 442, 160, 40);
            btnBackups = Add(card, new AccentButton("Open backups", ButtonKind.Secondary) { Glyph = "folder" }, 360, 442, 160, 40);
            backupsNote = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub), 20, 490, 500, 54);

            // The build & update log shares the sheet, on the right.
            var logCard = card;
            Add(card, new TextLabel("Build & update log", Theme.F("ui", 13, FontStyle.Bold), Theme.Text) { Wrap = false }, 556, 18, 300, 22);
            buildLog = Add(logCard, new LogBox(), 556, 48, 532, 488);
            var copy = Add(logCard, new AccentButton("Copy", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, 960, 18, 50, 22);
            var logs = Add(logCard, new AccentButton("Logs", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, 1024, 18, 64, 22);
            copy.Click += (s, e) => { try { Clipboard.SetText(buildLog.AllText.Length > 0 ? buildLog.AllText : " "); } catch { } };
            logs.Click += (s, e) => SystemInfo.OpenFolder(Path.Combine(Settings.Folder, "logs"), null);

            btnCheck.Click += (s, e) => { if (job == Job.Update && cancel != null) { cancel.Cancel(); SetProgress("Cancelling...", -1); } else CheckForUpdate(false); };
            btnUpdateNow.Click += (s, e) => UpdateNow();
            btnInstallZip.Click += (s, e) => InstallFromDownloads();
            btnBuildMod.Click += (s, e) => StartBuild(null);
            btnRevert.Click += (s, e) => Revert();
            btnBackups.Click += (s, e) => { string b = Backups.BackupRoot(root); Directory.CreateDirectory(b); SystemInfo.OpenFolder(b, null); };
            checkOnStart.Changed += (s, e) => { settings.SetBool("CheckOnStart", checkOnStart.Checked); settings.Save(); };
            btnSaveRepo.Click += (s, e) =>
            {
                string r = repoField.Text.Trim().Trim('/');
                if (r.StartsWith("https://github.com/", StringComparison.OrdinalIgnoreCase)) r = r.Substring(19);
                if (r.EndsWith(".git")) r = r.Substring(0, r.Length - 4);
                if (r.Split('/').Length != 2) { MessageBox.Show(this, "Write the repository as owner/name, e.g. someone/sm2-vr.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
                settings.UpdateRepo = r; settings.Save(); repoField.Text = r;
                latest = null; updatePill.Visible = false;
                upLatest.Set(CheckState.Info, "Saved. Press Check for update.");
                RefreshCredits();
            };
            tip.SetToolTip(btnCheck, "Reads version.json from the latest GitHub release (falls back to the GitHub API)");
            tip.SetToolTip(btnUpdateNow, "Downloads the update, checks its SHA-256, backs up this version, installs, builds; puts everything back if any step fails");
            tip.SetToolTip(btnRevert, "Go back to an earlier version: from the backups the launcher keeps, or an older GitHub release");
            upLatest.Set(CheckState.Info, settings.RepoConfigured ? "Not checked yet." : "No GitHub repository set yet (below). Update zips in Downloads still work.");
            SetProgress("", 0);
        }

        void SetProgress(string text, int pct)
        {
            Ui(() =>
            {
                if (progressText == null) return;
                progressText.Text = text ?? "";
                if (pct < 0) progress.Busy(); else progress.Set(pct / 100.0);
            });
        }

        void SetUpdateButtons()
        {
            if (btnCheck == null) return;
            bool idle = job == Job.None;
            bool busy = job == Job.Update;
            btnCheck.Text = busy && cancel != null ? "Cancel" : "Check for update";
            btnCheck.Enabled = (idle && settings.RepoConfigured) || (busy && cancel != null);
            btnUpdateNow.Enabled = idle && settings.RepoConfigured && (latest == null || IsNewer(latest));
            btnUpdateNow.Text = latest != null && IsNewer(latest) ? "Update to " + ShortVersion(latest.Version) : "Update now";
            btnInstallZip.Enabled = idle && downloadZip != null;
            btnBuildMod.Enabled = idle && (preview || SystemInfo.IsPortRoot(root));
            btnRevert.Enabled = idle;
            btnSaveRepo.Enabled = idle;
        }

        /// <summary>"Version 0.0.2" to "0.0.2" (and legacy "Update 38.5" to "38.5").</summary>
        static string ShortVersion(string v) { var p = ModVersion.Parse(v); return p != null ? p.Number : (v ?? ""); }

        bool IsNewer(Manifest m)
        {
            // Semver order; any legacy "Update NN.N" install is older than 0.0.1.
            var have = ModVersion.Read(root);
            var got = m.Ver;
            if (got == null) return false;
            return have == null || got.CompareTo(have) > 0;
        }

        void RefreshDownloads()
        {
            if (preview) return;
            downloadZip = SystemInfo.NewestUpdateZip();
            if (downloadZip == null)
                upDownloads.Set(CheckState.Info, "No sm2-vr-*.zip (or older sm2-update-*.zip) in " + SystemInfo.Downloads() + ".");
            else
            {
                ZipCheck c = null;
                try { c = Installer.Inspect(downloadZip.FullName); } catch { }
                string v = c != null && c.Version != null ? c.Version : "an update";
                bool same = c != null && c.Version != null && ModVersion.Same(c.Version, SystemInfo.PortVersion(root));
                upDownloads.Set(same ? CheckState.Ok : CheckState.Warn, downloadZip.Name + " (" + v + ", " + downloadZip.LastWriteTime.ToString("d MMM HH:mm") + ")" + (same ? ": already installed." : ": press Install."));
            }
            SetUpdateButtons();
        }

        void RefreshBackupsUi()
        {
            if (preview) return;
            var list = Backups.List(root);
            string where = Backups.BackupRoot(root);
            backupsNote.Text = "Keeps the last " + settings.KeepBackups + " versions in " + where + "." +
                (list.Count == 0 ? " No backups yet: one is made before every update or revert." : " Now: " + string.Join(", ", list.Take(4).Select(b => b.Version)) + ".") +
                " Your settings (live.txt, next-launch.txt, gestures) are kept across updates.";
        }

        // ---- Checking ---------------------------------------------------------------------------------------

        void CheckForUpdate(bool silent)
        {
            if (!settings.RepoConfigured)
            {
                if (!silent) MessageBox.Show(this, "Set the GitHub repository first (owner/name) and press Save.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            string repo = settings.UpdateRepo;
            upLatest.Set(CheckState.Busy, "Checking " + repo + "...");
            btnCheck.Enabled = false;
            ThreadPool.QueueUserWorkItem(_ =>
            {
                Manifest m = null; string error = null;
                try { Release rel; m = Manifest.Latest(repo, out rel, l => Ui(() => buildLog.Line(l, Theme.Dim))); }
                catch (Exception ex) { error = ex.Message; }
                Ui(() =>
                {
                    SetUpdateButtons();
                    if (m == null)
                    {
                        upLatest.Set(CheckState.Bad, "Could not check: " + error);
                        buildLog.Line("Update check failed: " + error, Theme.Bad);
                        return;
                    }
                    latest = m;
                    string date = string.IsNullOrEmpty(m.ReleaseDate) ? "" : ", released " + m.ReleaseDate;
                    if (IsNewer(m))
                    {
                        upLatest.Set(CheckState.Warn, m.Version + date + ": newer than yours. Press Update now.");
                        updatePill.Text = "Version " + ShortVersion(m.Version) + " available"; updatePill.FitWidth(); updatePill.Visible = true;
                        buildLog.Line("Update available: " + m.Version + " (" + (m.ManifestUrl ?? "GitHub API") + ").", Theme.Warn);
                        tabs.Badges[2] = "NEW";
                    }
                    else
                    {
                        upLatest.Set(CheckState.Ok, m.Version + date + ": you are up to date.");
                        updatePill.Visible = false; tabs.Badges.Remove(2);
                        if (!silent) buildLog.Line("Up to date (" + m.Version + ").", Theme.Good);
                    }
                    tabs.Invalidate();
                    SetUpdateButtons();
                });
            });
        }

        /// <summary>The manifest's game-version rule, checked before anything is downloaded.</summary>
        bool GameVersionAllows(Manifest m)
        {
            if (m == null || string.IsNullOrEmpty(m.MinGameVersion) || gamePath == null) return true;
            var have = SystemInfo.ParseVersion(SystemInfo.FileVersion(gamePath));
            var need = SystemInfo.ParseVersion(m.MinGameVersion);
            if (have == null || need == null || have >= need) return true;
            return MessageBox.Show(this, m.Version + " needs the game at v" + m.MinGameVersion + " or newer; yours is v" + have + ".\n\nUpdate the game in Steam first. Install anyway?",
                LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes;
        }

        /// <summary>Downloads one file listed in a manifest and checks it against the manifest's SHA-256.</summary>
        static string DownloadVerified(ManifestFile f, Installer inst, Func<bool> allowUnverified)
        {
            string dir = Path.Combine(Settings.Folder, "downloads");
            Directory.CreateDirectory(dir);
            string name = string.Concat((f.Name ?? "update.zip").Split(Path.GetInvalidFileNameChars()));
            string path = Path.Combine(dir, name);
            if (File.Exists(path) && !string.IsNullOrEmpty(f.Sha256) && GitHub.Sha256(path) == f.Sha256)
            {
                inst.Log("Already downloaded and verified: " + path);
                return path;
            }
            inst.Log("Downloading " + f.Url);
            GitHub.Download(f.Url, path, (got, total) => inst.Progress("Downloading " + name + ": " + (got >> 20) + " / " + (total > 0 ? (total >> 20) + " MB" : "? MB"),
                total > 0 ? (int)(100 * got / total) : -1), inst.Cancel);
            inst.Progress("Checking the SHA-256...", -1);
            string sha = GitHub.Sha256(path);
            if (string.IsNullOrEmpty(f.Sha256))
            {
                inst.Log("WARNING: the release publishes no SHA-256 for " + name + " (its SHA-256 is " + sha + ").");
                if (!allowUnverified()) { try { File.Delete(path); } catch { } throw new OperationCanceledException(); }
            }
            else if (sha != f.Sha256)
            {
                try { File.Delete(path); } catch { }
                throw new InvalidDataException("The download is damaged or not the published file: its SHA-256 is " + sha + " but the release says " + f.Sha256 + ". Nothing was changed; try again.");
            }
            else inst.Log("SHA-256 OK: " + sha);
            return path;
        }

        Func<bool> AskUnverified()
        {
            return () =>
            {
                bool ok = false;
                Invoke((Action)(() => ok = MessageBox.Show(this, "This release has no published SHA-256 checksum, so the download can't be verified.\n\nInstall it anyway?",
                    LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes));
                return ok;
            };
        }

        void UpdateNow()
        {
            if (!GuardIdle()) return;
            if (!settings.RepoConfigured) { CheckForUpdate(false); return; }
            if (latest == null)
            {
                SetProgress("Checking for the latest version...", -1);
                try { Release rel; latest = Manifest.Latest(settings.UpdateRepo, out rel, l => buildLog.Line(l, Theme.Dim)); }
                catch (Exception ex) { SetProgress("", 0); MessageBox.Show(this, "Could not check GitHub: " + ex.Message, LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
                SetProgress("", 0);
            }
            var m = latest;
            if (!IsNewer(m)) { MessageBox.Show(this, "You already have the latest version (" + m.Version + ").", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Information); SetUpdateButtons(); return; }
            var file = m.File("update") ?? m.File("source");
            if (file == null) { MessageBox.Show(this, m.Version + " on GitHub has no update zip attached (sm2-vr-*.zip).", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
            if (!GameVersionAllows(m)) return;
            if (MessageBox.Show(this, "Update " + (SystemInfo.PortVersion(root) ?? "the mod") + " to " + m.Version + "?\n\nThe launcher backs up this version first, then downloads, verifies, installs and builds. If anything fails it puts this version back.",
                LauncherInfo.Title, MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK) return;
            var ask = AskUnverified();
            RunInstall("Updating to " + m.Version, inst =>
            {
                string zip = DownloadVerified(file, inst, ask);
                return file.Kind == "source" ? inst.InstallFull(zip, false, "before updating to " + m.Version) : inst.InstallZip(zip, "before updating to " + m.Version);
            });
        }

        void InstallFromDownloads()
        {
            if (!GuardIdle()) return;
            RefreshDownloads();
            if (downloadZip == null) { MessageBox.Show(this, "No sm2-vr-*.zip (or older sm2-update-*.zip) was found in " + SystemInfo.Downloads() + ".", LauncherInfo.Title); return; }
            string zip = downloadZip.FullName;
            if (MessageBox.Show(this, "Install " + downloadZip.Name + " into " + root + " and build?\n\n(Like Expand-Archive -Force; this version is backed up first and put back if anything fails.)",
                LauncherInfo.Title, MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK) return;
            RunInstall("Installing " + downloadZip.Name, inst => inst.InstallZip(zip, "before installing " + Path.GetFileName(zip)));
        }

        void Revert()
        {
            if (!GuardIdle()) return;
            using (var d = new RevertDialog(root, settings, preview))
            {
                if (d.ShowDialog(this) != DialogResult.OK) return;
                bool rs = d.RestoreSettings;
                if (d.Backup != null)
                {
                    var b = d.Backup;
                    if (MessageBox.Show(this, "Revert to " + b.Version + " (backed up " + b.Created.ToString("d MMM yyyy HH:mm") + ")?\n\nThe current version is backed up first. " +
                        (rs ? "That version's settings files are restored too." : "Your current settings files are kept."), LauncherInfo.Title, MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK) return;
                    RunInstall("Reverting to " + b.Version, inst => inst.Revert(b, rs));
                }
                else if (d.Release != null)
                {
                    var m = d.Release;
                    var file = m.File("source") ?? m.File("update");
                    if (file == null) { MessageBox.Show(this, m.Version + " has no zip attached.", LauncherInfo.Title); return; }
                    if (file.Kind != "source" && MessageBox.Show(this, m.Version + " only has an update zip (the files that changed in it), not a full package. Installing it over a newer version puts back only those files, " +
                        "which can mix versions.\n\nInstall it anyway?", LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
                    var ask = AskUnverified();
                    RunInstall("Installing " + m.Version, inst =>
                    {
                        string zip = DownloadVerified(file, inst, ask);
                        return inst.InstallFull(zip, rs, "before going back to " + m.Version);
                    });
                }
            }
        }

        void RunInstall(string title, Func<Installer, InstallResult> work)
        {
            if (job != Job.None) { GuardIdle(); return; }
            if (SystemInfo.GameRunning()) { MessageBox.Show(this, "Close Spider-Man 2 first: the files it uses can't be replaced while it runs.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            if (FolderLock.Busy(root)) { MessageBox.Show(this, "Another launcher is updating or building " + root + " right now. Wait for it to finish.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            job = Job.Update;
            cancel = new CancellationTokenSource();
            tabs.Select(2);
            UpdatePlayState();
            buildLog.Line("> " + title, Theme.Info);
            var inst = new Installer
            {
                Root = root, Settings = settings, Cancel = cancel.Token,
                Log = l => buildLog.Line(l, l.StartsWith("FAILED") || l.StartsWith("ROLLBACK") ? Theme.Bad : l.StartsWith("WARNING") ? Theme.Warn : Theme.Silver),
                BuildOutput = (l, e) => buildLog.Line(l, e && !l.StartsWith("WARNING") ? Theme.Bad : LogBox.ColorFor(l)),
                Progress = (t, pct) => SetProgress(t, pct)
            };
            var th = new Thread(() =>
            {
                InstallResult r;
                try { r = work(inst); }
                catch (OperationCanceledException) { r = new InstallResult { Ok = false, Message = title + " was cancelled. Nothing was changed." }; }
                catch (Exception ex) { r = new InstallResult { Ok = false, Message = title + " failed: " + ex.Message + "\n\nNothing was changed." }; }
                Ui(() =>
                {
                    job = Job.None;
                    cancel = null;
                    buildLog.Line(r.Message.Replace("\n\n", " "), r.Ok ? Theme.Good : Theme.Bad);
                    if (!r.Ok) SetProgress(progressText.Text.StartsWith("Rolled back") ? progressText.Text : "Not installed.", 0);
                    RefreshAll();
                    if (latest != null && !IsNewer(latest)) { updatePill.Visible = false; upLatest.Set(CheckState.Ok, latest.Version + ": you are up to date."); }
                    UpdatePlayState();
                    MessageBox.Show(this, r.Message + (r.Ok ? "\n\nInstalled: " + (SystemInfo.PortVersion(root) ?? "?") : ""), LauncherInfo.Title, MessageBoxButtons.OK, r.Ok ? MessageBoxIcon.Information : MessageBoxIcon.Error);
                });
            }) { IsBackground = false, Name = "Install" };
            th.Start();
        }
    }
}
