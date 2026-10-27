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
        AccentButton btnCheck, btnUpdateNow, btnInstallZip, btnBuildMod, btnRevert, btnBackups, btnSaveRepo, btnDevBuild;
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
            btnBuildMod = Add(card, new AccentButton("Repair mod", ButtonKind.Secondary) { Glyph = "build" }, 20, 442, 160, 40);
            btnRevert = Add(card, new AccentButton("Revert", ButtonKind.Danger) { Glyph = "revert" }, 190, 442, 160, 40);
            btnBackups = Add(card, new AccentButton("Open backups", ButtonKind.Secondary) { Glyph = "folder" }, 360, 442, 160, 40);
            backupsNote = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub), 20, 490, 500, 54);

            // The build & update log shares the sheet, on the right.
            var logCard = card;
            // The label is narrowed in the new design so it never covers Build (dev) and Copy, which sit further left there.
            Add(card, new TextLabel("Build & update log", Theme.F("ui", 13, FontStyle.Bold), Theme.Text) { Wrap = false }, 556, 18, Theme.HeroStyle ? 156 : 300, 22);
            buildLog = Add(logCard, new LogBox(), 556, 48, 532, 488);
            // The new design draws BACK  ESC in the sheet's top right corner: these move left of it there.
            bool hs = Theme.HeroStyle;
            var copy = Add(logCard, new AccentButton("Copy", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, hs ? 830 : 960, 18, 50, 22);
            var logs = Add(logCard, new AccentButton("Logs", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, hs ? 890 : 1024, 18, 64, 22);
            // Developers only (Visual Studio C++ and git found): compile the mod from source. Players never see it.
            btnDevBuild = Add(logCard, new AccentButton(hs ? "Build (dev)" : "Build from source (dev)", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, hs ? 716 : 760, 18, hs ? 104 : 190, 22);
            btnDevBuild.Visible = !preview && ModFiles.DevToolsPresent();
            btnDevBuild.Click += (s, e) => StartBuild(null);
            copy.Click += (s, e) => { try { Clipboard.SetText(buildLog.AllText.Length > 0 ? buildLog.AllText : " "); } catch { } };
            logs.Click += (s, e) => SystemInfo.OpenFolder(Path.Combine(Settings.Folder, "logs"), null);

            btnCheck.Click += (s, e) => { if (job == Job.Update && cancel != null) { cancel.Cancel(); SetProgress("Cancelling...", -1); } else CheckForUpdate(false); };
            btnUpdateNow.Click += (s, e) => UpdateNow();
            btnInstallZip.Click += (s, e) => InstallFromDownloads();
            btnBuildMod.Click += (s, e) => RepairMod(null);
            btnRevert.Click += (s, e) => Revert();
            btnBackups.Click += (s, e) => { string b = Backups.BackupRoot(root); Directory.CreateDirectory(b); SystemInfo.OpenFolder(b, null); };
            checkOnStart.Changed += (s, e) => { settings.SetBool("CheckOnStart", checkOnStart.Checked); settings.Save(); };
            btnSaveRepo.Click += (s, e) =>
            {
                string r = repoField.Text.Trim().Trim(new[] { '/' });
                if (r.StartsWith("https://github.com/", StringComparison.OrdinalIgnoreCase)) r = r.Substring(19);
                if (r.EndsWith(".git")) r = r.Substring(0, r.Length - 4);
                if (r.Split(new[] { '/' }).Length != 2) { MessageBox.Show(this, "Write the repository as owner/name, e.g. someone/sm2-vr.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
                settings.UpdateRepo = r; settings.Save(); repoField.Text = r;
                latest = null; updatePill.Visible = false;
                upLatest.Set(CheckState.Info, "Saved. Press Check for update.");
                RefreshCredits();
            };
            tip.SetToolTip(btnCheck, "Reads version.json from the latest GitHub release (falls back to the GitHub API)");
            tip.SetToolTip(btnUpdateNow, "Downloads the update, checks its SHA-256, backs up this version, installs it with its ready-made mod files; puts everything back if any step fails");
            tip.SetToolTip(btnBuildMod, "Downloads this version's ready-made mod files from GitHub again, checks their SHA-256 and reinstalls them. Nothing to compile.");
            tip.SetToolTip(btnDevBuild, "Developers only: tools\\build.ps1 -Observer -FromSource (needs Visual Studio 2022 C++ and git)");
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
            if (btnDevBuild != null) btnDevBuild.Enabled = idle && SystemInfo.IsPortRoot(root);
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
            if (have == null || got.CompareTo(have) > 0) return true;
            // a launcher-only hotfix: same mod version, newer launcher build. The build counted is the newer of this
            // launcher and the one installed in the mod folder (tools\launcher), so a launcher started from somewhere
            // else (an older copy, a shortcut to a dev build) never offers the same version again after installing it.
            return got.CompareTo(have) == 0 && m.LauncherBuild > Math.Max(LauncherInfo.Build, SystemInfo.InstalledLauncherBuild(root));
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

        bool CanCheck { get { return settings.RepoConfigured || !string.IsNullOrEmpty(Manifest.OverrideUrl); } }

        // ---- Self-update: checked first thing on start ---------------------------------------------------

        bool autoUpdate;
        string whatsNew;   // this install was started by the start-up prompt: restart quietly when done

        void StartupUpdateCheck()
        {
            try
            {
                Manifest.OverrideUrl = settings.Get("UpdateManifestUrl", null);
                if (Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_USE_PREVIOUS") == "1")   // automated tests only
                {
                    string testTag = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_PREVIOUS_TAG");
                    if (string.IsNullOrEmpty(testTag) || !LauncherInfo.PreviousTags.Contains(testTag)) testTag = LauncherInfo.PreviousTags[LauncherInfo.PreviousTags.Length - 1];
                    if (RevertDialog.OfferPrevious(root, testTag)) { UsePreviousVersion(false, testTag); return; }
                }
                if (!settings.GetBool("CheckOnStart", false) || !CanCheck) { FirstRunModFiles(); return; }
                CheckForUpdate(true, m =>
                {
                    try { OfferUpdate(m); } catch (Exception ex) { Program.LogError(ex); buildLog.Line("Update prompt failed: " + ex.Message, Theme.Bad); }
                    FirstRunModFiles();
                }, error => FirstRunModFiles());
            }
            catch (Exception ex) { Program.LogError(ex); }
        }

        bool quietModFiles;   // the first-start install: a failure goes to the log and the Status sheet, not a pop-up

        /// <summary>First start (or a folder whose mod files are missing): installs the ready-made mod files by itself,
        /// so PLAY VR is ready without the player doing anything. Never compiles.</summary>
        void FirstRunModFiles()
        {
            try
            {
                if (preview || job != Job.None || !SystemInfo.IsPortRoot(root)) return;
                string why = ModFiles.RepairReason(root);
                if (why == null) return;
                if (FolderLock.Busy(root)) return;
                if (ModFiles.Installed(root))
                    buildLog.Line("The mod files for " + (SystemInfo.PortVersion(root) ?? "this version") + " are " + why + ": downloading the ready-made files from GitHub (nothing to install by hand).", Theme.Info);
                else
                    buildLog.Line("The mod files for " + (SystemInfo.PortVersion(root) ?? "this version") + " are not installed yet: downloading the ready-made files from GitHub.", Theme.Info);
                quietModFiles = true;
                RepairMod(null, true);
            }
            catch (Exception ex) { Program.LogError(ex); }
        }

        /// <summary>REPAIR MOD / PLAY VR on a folder without mod files: downloads this version's ready-made mod files,
        /// checks their SHA-256 and installs them. <paramref name="then"/> gets true when they are ready.</summary>
        void RepairMod(Action<bool> then, bool noConfirm = false)
        {
            if (!GuardIdle()) return;
            if (!SystemInfo.IsPortRoot(root)) { MessageBox.Show(this, "Pick the mod folder first (Status > Mod folder).", LauncherInfo.Title); return; }
            if (!noConfirm && then == null && MessageBox.Show(this, "Download the ready-made mod files for " + (SystemInfo.PortVersion(root) ?? "this version") + " from GitHub again and reinstall them?\n\n" +
                "Their SHA-256 is checked first; your current files are put back if anything fails.", LauncherInfo.Title, MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK) return;
            modFilesThen = then;
            RunInstall("Installing the mod files", inst => inst.RepairModFiles());
        }

        Action<bool> modFilesThen;   // PLAY VR waiting for the mod files

        static string UpdateKey(Manifest m) { return (m.VersionNumber ?? m.Version) + "#" + m.LauncherBuild; }
        /// <summary>Revert's skip key when the live version.json can't be read: releases carry no launcher_build any more
        /// (update-loop fix), so older launchers compute "&lt;number&gt;#0".</summary>
        internal static string RevertSkipKey(string number) { return number + "#0"; }

        void OfferUpdate(Manifest m)
        {
            if (m == null || !IsNewer(m) || job != Job.None) return;
            if (settings.Get("SkipUpdate", "") == UpdateKey(m))
            {
                buildLog.Line("You chose to skip " + m.Version + ". Updates > Update now installs it any time.", Theme.Dim);
                return;
            }
            string have = SystemInfo.PortVersion(root) ?? "no mod";
            string news = string.IsNullOrEmpty(m.WhatsNew) ? "" : "\n\nWhat's new: " + m.WhatsNew;
            string test = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_UPDATE_ANSWER");   // automated tests only
            DialogResult r = test == "yes" ? DialogResult.Yes : test == "skip" ? DialogResult.No : test == "later" ? DialogResult.Cancel :
                MessageBox.Show(this, LauncherInfo.Title + " " + m.Version + (m.LauncherBuild > LauncherInfo.Build ? " (launcher build " + m.LauncherBuild + ")" : "") +
                    " is available. You have " + have + ", launcher build " + LauncherInfo.Build + "." + news +
                    "\n\nUpdate now? The launcher backs up your version, downloads the update and checks its SHA-256, installs it, keeps your settings, " +
                    "installs the ready-made mod files (nothing to compile) and restarts itself. It never touches the game's own files. If anything fails, your current version is put back." +
                    "\n\nYes: update now.    No: skip this version.    Cancel: ask me next time.",
                    LauncherInfo.Title + " - update available", MessageBoxButtons.YesNoCancel, MessageBoxIcon.Information);
            if (r == DialogResult.No) { settings.Set("SkipUpdate", UpdateKey(m)); settings.Save(); buildLog.Line("Skipped " + m.Version + ".", Theme.Dim); return; }
            if (r != DialogResult.Yes) return;
            autoUpdate = true;
            UpdateNow(true);
        }

        /// <summary>After a self-update the new launcher says what happened (and the what's-new line) once.</summary>
        void ShowJustUpdated()
        {
            string done = settings.Get("JustUpdated", "");
            if (done.Length == 0) return;
            done += " Launcher " + LauncherInfo.Version + " (build " + LauncherInfo.Build + ") is running.";
            buildLog.Line(done, Theme.Good);
            SetProgress(done, 100);
            settings.Set("JustUpdated", ""); settings.Save();
        }

        void RestartLauncher(string message) { RestartLauncher(message, null); }

        /// <summary>Restarts into <paramref name="target"/> (the launcher installed in the mod folder) or, when null, this exe.</summary>
        void RestartLauncher(string message, string target)
        {
            string exe = target ?? System.Reflection.Assembly.GetExecutingAssembly().Location;
            try
            {
                settings.Set("JustUpdated", message); settings.Save();
                var psi = Type.GetType("Mono.Runtime") != null
                    ? new System.Diagnostics.ProcessStartInfo("mono", "\"" + exe + "\" --after-update")
                    : new System.Diagnostics.ProcessStartInfo(exe, "--after-update");
                psi.UseShellExecute = false;
                psi.WorkingDirectory = Path.GetDirectoryName(exe);
                buildLog.Line("Restarting the launcher (new version)...", Theme.Info);
                StdHandles.ClearDead();   // 0.0.70.1 hotfix: never hand a dead standard handle to the new launcher
                System.Diagnostics.Process.Start(psi);
                Close();
            }
            catch (Exception ex)
            {
                Program.LogError(ex);
                MessageBox.Show(this, message + "\n\nClose the launcher and open it again to use the new version (" + ex.Message + ").", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }

        void CheckForUpdate(bool silent, Action<Manifest> then = null, Action<string> failed = null)
        {
            if (!CanCheck)
            {
                if (failed != null) failed("no repository");
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
                        if (failed != null) failed(error);
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
                    if (then != null) then(m);
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

        void UpdateNow(bool noConfirm = false)
        {
            if (!GuardIdle()) return;
            if (!CanCheck) { CheckForUpdate(false); return; }
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
            if (!noConfirm && MessageBox.Show(this, "Update " + (SystemInfo.PortVersion(root) ?? "the mod") + " to " + m.Version + "?\n\nThe launcher backs up this version first, then downloads, verifies and installs it with its ready-made mod files. If anything fails it puts this version back.",
                LauncherInfo.Title, MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK) return;
            var ask = AskUnverified();
            whatsNew = m.WhatsNew;
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
            if (downloadZip == null) { MessageBox.Show(this, "No sm2-vr-*.zip, HERO-VR-*-mod-win64.zip (or older sm2-update-*.zip) was found in " + SystemInfo.Downloads() + ".", LauncherInfo.Title); return; }
            string zip = downloadZip.FullName;
            if (MessageBox.Show(this, "Install " + downloadZip.Name + " into " + root + "?\n\n(Like Expand-Archive -Force; this version is backed up first and put back if anything fails.)",
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
                if (d.UsePrevious) { UsePreviousVersion(rs, d.PreviousTag); return; }
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

        /// <summary>REVERT > USE PREVIOUS VERSION: installs the picked earlier release (build 9: v0.0.6 or v0.0.5) straight from its GitHub release,
        /// with no backup needed: its version.json (releases/download/&lt;tag&gt;/version.json, no API limit), its full
        /// package ("source") downloaded and SHA-256 checked, installed like any update (this version backed up first and
        /// put back if anything fails), then that version's ready-made mod files (also SHA-256 checked). The newer
        /// release is then skipped (SkipUpdate), so the older launcher does not offer it again at once; Updates >
        /// Update now installs it any time.</summary>
        void UsePreviousVersion(bool restoreSettings, string tag)
        {
            if (string.IsNullOrEmpty(tag)) tag = LauncherInfo.PreviousTag;
            string prev = RevertDialog.PreviousNumber(tag);
            if (!settings.RepoConfigured) { MessageBox.Show(this, "Set the GitHub repository in Updates first.", LauncherInfo.Title); return; }
            string have = SystemInfo.PortVersion(root) ?? "this version";
            string test = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_UPDATE_ANSWER");   // automated tests only
            if (test != "yes" && MessageBox.Show(this, "Go back to Version " + prev + "?\n\nThe launcher downloads " + prev + " from GitHub, checks its SHA-256 and installs it, " +
                "with its ready-made mod files. " + have + " is backed up first, so Revert can bring it back too, and it is put back if anything fails. Your settings are kept." +
                "\n\nYou then stay on " + prev + ": the update is not offered again until you press Updates > Update now.",
                LauncherInfo.Title, MessageBoxButtons.OKCancel, MessageBoxIcon.Question) != DialogResult.OK) return;
            string url = "https://github.com/" + settings.UpdateRepo.Trim().Trim(new[] { '/' }) + "/releases/download/" + tag + "/version.json";
            // tests: a local server (PreviousManifestUrl.<tag>, else PreviousManifestUrl for the oldest, as build 8)
            string skip = settings.Get("PreviousManifestUrl." + tag, "").Trim();
            if (skip.Length == 0 && tag == LauncherInfo.PreviousTags[LauncherInfo.PreviousTags.Length - 1]) skip = settings.Get("PreviousManifestUrl", "").Trim();
            if (skip.Length > 0 && Manifest.SafeUrl(skip)) url = skip;   // tests: a local server
            // The skip must be the exact key the older launcher computes from the live version.json (UpdateKey: number
            // + "#" + its launcher_build). 0.0.62 dropped launcher_build (update-loop fix), so a guess with this launcher's
            // own build ("0.0.62#9") never matched "0.0.62#0" and the older launcher offered 0.0.62 again at once. When
            // the start-up check has not finished yet, the live version.json is read here; failing that, "#0".
            string newer = latest != null ? UpdateKey(latest) : null;
            string repo = settings.UpdateRepo;
            string ownNumber = ModVersion.Parse(SystemInfo.PortVersion(root)) != null ? ModVersion.Parse(SystemInfo.PortVersion(root)).Number : "";
            var ask = AskUnverified();
            RunInstall("Going back to Version " + prev, inst =>
            {
                if (newer == null)
                {
                    try { Release rel; newer = UpdateKey(Manifest.Latest(repo, out rel, inst.Log)); }
                    catch (Exception ex) { inst.Log("Could not read the latest version.json (" + ex.Message + "); skipping " + ownNumber + "."); }
                    if (newer == null) newer = RevertSkipKey(ownNumber);
                }
                inst.Log("Reading " + url);
                var m = Manifest.Parse(Manifest.Fetch(url));
                m.ManifestUrl = url;
                var file = m.File("source");
                if (file == null) throw new InvalidDataException("The " + prev + " release lists no full package (\"source\") in its version.json.");
                if (string.IsNullOrEmpty(file.Sha256)) throw new InvalidDataException("The " + prev + " release gives no SHA-256 for " + file.Name + ", so it is not installed.");
                string zip = DownloadVerified(file, inst, ask);
                inst.Release = m;   // its mod files: the "prebuilt" entry of the same version.json
                var r = inst.InstallFull(zip, restoreSettings, "before going back to " + prev);
                if (r.Ok)
                {
                    settings.Set("SkipUpdate", newer); settings.Save();
                    inst.Log("Staying on " + prev + ": the newer version is skipped (" + newer + "). Updates > Update now installs it again.");
                }
                return r;
            });
        }

        void RunInstall(string title, Func<Installer, InstallResult> work)
        {
            if (job != Job.None) { GuardIdle(); return; }
            if (SystemInfo.GameRunning()) { MessageBox.Show(this, "Close Spider-Man 2 first: the files it uses can't be replaced while it runs.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            if (FolderLock.Busy(root)) { MessageBox.Show(this, "Another launcher is updating or building " + root + " right now. Wait for it to finish.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            job = Job.Update;
            Backups.SelfReplaced = false;
            cancel = new CancellationTokenSource();
            tabs.Select(2);
            UpdatePlayState();
            buildLog.Line("> " + title, Theme.Info);
            var inst = new Installer
            {
                Root = root, Settings = settings, Cancel = cancel.Token,
                Log = l => buildLog.Line(l, l.StartsWith("FAILED") || l.StartsWith("ROLLBACK") ? Theme.Bad : l.StartsWith("WARNING") ? Theme.Warn : Theme.Silver),
                BuildOutput = (l, e) => buildLog.Line(l, e && !l.StartsWith("WARNING") ? Theme.Bad : LogBox.ColorFor(l)),
                Progress = (t, pct) => SetProgress(t, pct),
                Release = latest
            };
            var askMod = AskUnverified();
            inst.Download = f => DownloadVerified(f, inst, askMod);
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
                    bool quiet = autoUpdate; autoUpdate = false;
                    bool quietMod = quietModFiles; quietModFiles = false;
                    var thenMod = modFilesThen; modFilesThen = null;
                    if (thenMod != null) { if (!r.Ok) MessageBox.Show(this, r.Message, LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Error); thenMod(r.Ok); return; }
                    if (quietMod) { if (r.Ok) { SetProgress(r.Message, 100); tabs.Select(SheetStatus); } else buildLog.Line("PLAY VR (or Updates > Repair mod) tries again.", Theme.Dim); return; }
                    string news = whatsNew; whatsNew = null;
                    // The update installed a newer launcher in the mod folder but this exe runs from somewhere else
                    // (so it was not replaced): switch to the installed one instead of offering the update again.
                    string installedExe = r.Ok && !Backups.SelfReplaced ? SystemInfo.NewerInstalledLauncher(root) : null;
                    if (installedExe != null)
                    {
                        string msg = "Updated to " + (SystemInfo.PortVersion(root) ?? "the new version") + ". Switched to the launcher installed in " + root + "." + (string.IsNullOrEmpty(news) ? "" : " What's new: " + news);
                        buildLog.Line("This launcher runs from outside the install, so it was not replaced. Starting the new launcher from " + installedExe + ".", Theme.Info);
                        RestartLauncher(msg, installedExe);
                        return;
                    }
                    if (r.Ok && Backups.SelfReplaced)
                    {
                        string msg = "Updated to " + (SystemInfo.PortVersion(root) ?? "the new version") + "." + (string.IsNullOrEmpty(news) ? "" : " What's new: " + news);
                        if (!quiet) MessageBox.Show(this, r.Message + "\n\nThe launcher restarts now to use its new version.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
                        RestartLauncher(msg);
                        return;
                    }
                    if (r.Ok && quiet) { buildLog.Line("Updated. " + (string.IsNullOrEmpty(news) ? "" : "What's new: " + news), Theme.Good); return; }
                    MessageBox.Show(this, r.Message + (r.Ok ? "\n\nInstalled: " + (SystemInfo.PortVersion(root) ?? "?") : ""), LauncherInfo.Title, MessageBoxButtons.OK, r.Ok ? MessageBoxIcon.Information : MessageBoxIcon.Error);
                });
            }) { IsBackground = false, Name = "Install" };
            th.Start();
        }
    }
}
