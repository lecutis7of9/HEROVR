// SM2 VR Launcher - REVERT: pick a backed-up version (or an older GitHub release) to go back to.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Threading;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    class RevertDialog : Form
    {
        public BackupInfo Backup;
        public Manifest Release;
        public bool RestoreSettings { get { return restore.Checked; } }
        /// <summary>"Use previous version" was pressed: install LauncherInfo.PreviousTag from GitHub (no backup needed).</summary>
        public bool UsePrevious;

        readonly ListBox list = new ListBox();
        readonly CheckBox restore = new CheckBox();
        readonly Button ok = new Button(), cancelBtn = new Button(), fromGitHub = new Button(), previous = new Button();
        readonly Label info = new Label();
        readonly string root;
        readonly Settings settings;
        readonly bool preview;

        class Item
        {
            public BackupInfo Backup; public Release Rel;
            public override string ToString()
            {
                if (Backup != null) return "\u21BA  " + Backup.ToString();
                return "\u2193  " + Rel.Display + "  \u00B7  from GitHub" + (Rel.FullZip != null ? "" : " (update zip only)");
            }
        }

        public RevertDialog(string root, Settings settings, bool preview)
        {
            this.root = root; this.settings = settings; this.preview = preview;
            Text = LauncherInfo.Title + " - Revert to a previous version";
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = MinimizeBox = false; ShowInTaskbar = false;
            StartPosition = FormStartPosition.CenterParent;
            BackColor = Color.FromArgb(16, 17, 24); ForeColor = Theme.Text; Font = Theme.Ui(12.5f);
            AutoScaleMode = AutoScaleMode.None;
            ClientSize = new Size(Theme.S(640), Theme.S(522));

            var head = new Label { Text = "Revert to a previous version", Font = Theme.F("ui", 18, FontStyle.Bold), ForeColor = Theme.Text, AutoSize = false, Bounds = Theme.R(24, 18, 590, 34) };
            var sub = new Label
            {
                Text = "Pick a version. The one you have now is backed up first, so you can come back to it. Then the launcher restores, builds and checks the version; if anything fails, nothing changes.",
                ForeColor = Theme.Sub, AutoSize = false, Bounds = Theme.R(24, 54, 590, 44)
            };
            // One click back to the previous release (e.g. 0.0.6 -> 0.0.5): works with no backup at all.
            string prevNumber = PreviousNumber();
            Style(previous, "USE PREVIOUS VERSION (" + prevNumber + ")", true); previous.Bounds = Theme.R(24, 104, 300, 38);
            var prevNote = new Label { Text = "Downloads " + prevNumber + " from GitHub, checks its SHA-256 and installs it. You stay on it until you press Update now.", ForeColor = Theme.Sub, AutoSize = false, Bounds = Theme.R(334, 104, 282, 42) };
            previous.Enabled = (settings.RepoConfigured || preview) && OfferPrevious(root);
            previous.Click += (s, e) => { UsePrevious = true; DialogResult = preview ? DialogResult.Cancel : DialogResult.OK; };
            list.Bounds = Theme.R(24, 158, 592, 210);
            list.BackColor = Theme.Field; list.ForeColor = Theme.Text; list.BorderStyle = BorderStyle.FixedSingle;
            list.DrawMode = DrawMode.OwnerDrawFixed; list.ItemHeight = Theme.S(30);
            list.DrawItem += DrawItem;
            list.SelectedIndexChanged += (s, e) => Selected();
            list.DoubleClick += (s, e) => { if (ok.Enabled) Accept(); };
            restore.Text = "Also restore that version's settings files (live.txt, next-launch.txt, gestures)";
            restore.ForeColor = Theme.Silver; restore.Bounds = Theme.R(24, 378, 592, 26);
            info.ForeColor = Theme.Sub; info.AutoSize = false; info.Bounds = Theme.R(24, 408, 592, 52);
            Style(fromGitHub, "Download an older release from GitHub", false); fromGitHub.Bounds = Theme.R(24, 468, 300, 36);
            Style(ok, "REVERT", true); ok.Bounds = Theme.R(392, 468, 110, 36);
            Style(cancelBtn, "CANCEL", false); cancelBtn.Bounds = Theme.R(508, 468, 108, 36);
            cancelBtn.DialogResult = DialogResult.Cancel; CancelButton = cancelBtn;
            ok.Click += (s, e) => Accept();
            fromGitHub.Click += (s, e) => LoadReleases();
            fromGitHub.Enabled = settings.RepoConfigured || preview;
            Controls.AddRange(new Control[] { head, sub, previous, prevNote, list, restore, info, fromGitHub, ok, cancelBtn });

            var backups = preview ? FakeBackups() : Backups.List(root);
            foreach (var b in backups) list.Items.Add(new Item { Backup = b });
            if (list.Items.Count > 0) list.SelectedIndex = 0;
            else info.Text = "No backups yet (one is made before every update and revert). " + (settings.RepoConfigured ? "Press Download an older release from GitHub." : "Set the GitHub repository in Updates to download older releases.");
            Selected();
        }

        /// <summary>"0.0.5" from LauncherInfo.PreviousTag.</summary>
        public static string PreviousNumber()
        {
            var v = ModVersion.Parse(LauncherInfo.PreviousTag);
            return v == null ? LauncherInfo.PreviousTag.TrimStart(new[] { 'v', 'V' }) : v.Number;
        }

        /// <summary>Only when the installed version is newer than the previous release (on 0.0.5 already there is nothing to go back to).</summary>
        public static bool OfferPrevious(string root)
        {
            var have = ModVersion.Parse(SystemInfo.PortVersion(root));
            var prev = ModVersion.Parse(LauncherInfo.PreviousTag);
            return prev != null && (have == null || have > prev);
        }

        static List<BackupInfo> FakeBackups()
        {
            return new List<BackupInfo>
            {
                new BackupInfo { Version = "Update 38.4", Created = DateTime.Now.AddDays(-2), Reason = "before updating to Version 0.0.1", Folder = @"C:\sm2-port-backup\Update-38.4" },
                new BackupInfo { Version = "Update 38.3", Created = DateTime.Now.AddDays(-6), Reason = "before updating to Update 38.4", Folder = @"C:\sm2-port-backup\Update-38.3" },
                new BackupInfo { Version = "Update 38.2", Created = DateTime.Now.AddDays(-11), Reason = "before updating to Update 38.3", Folder = @"C:\sm2-port-backup\Update-38.2" },
            };
        }

        static void Style(Button b, string text, bool primary)
        {
            b.Text = text; b.FlatStyle = FlatStyle.Flat; b.Font = Theme.UiBold(12);
            b.BackColor = primary ? Theme.Red : Color.FromArgb(34, 36, 46); b.ForeColor = Color.White;
            b.FlatAppearance.BorderColor = primary ? Theme.RedHot : Theme.PanelBorder;
            b.Cursor = Cursors.Hand;
        }

        void DrawItem(object sender, DrawItemEventArgs e)
        {
            if (e.Index < 0) return;
            bool sel = (e.State & DrawItemState.Selected) != 0;
            using (var b = new SolidBrush(sel ? Color.FromArgb(90, Theme.Red) : Theme.Field)) e.Graphics.FillRectangle(b, e.Bounds);
            var it = (Item)list.Items[e.Index];
            TextRenderer.DrawText(e.Graphics, it.ToString(), Theme.Ui(12.5f), new Rectangle(e.Bounds.X + Theme.S(8), e.Bounds.Y, e.Bounds.Width - Theme.S(12), e.Bounds.Height),
                it.Backup != null ? Theme.Text : Theme.Silver, TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
        }

        void Selected()
        {
            var it = list.SelectedItem as Item;
            ok.Enabled = it != null;
            if (it == null) return;
            if (it.Backup != null)
                info.Text = "Backup: " + it.Backup.Folder + ". Restores exactly those files" + (restore.Checked ? ", with that version's settings." : "; your current settings are kept.");
            else if (it.Rel.FullZip != null)
                info.Text = "Downloads " + it.Rel.FullZip.Name + " (the full package), checks its SHA-256 from the release's version.json, and installs it.";
            else
                info.Text = "This release has only an update zip, so going back to it may mix files from two versions. A backup is safer when there is one.";
        }

        void LoadReleases()
        {
            fromGitHub.Enabled = false;
            info.Text = "Loading releases from " + settings.UpdateRepo + "...";
            if (preview)
            {
                info.Text = "No older GitHub releases: Version 0.0.1 is the first published one. Use a backup above.";
                return;
            }
            string repo = settings.UpdateRepo;
            ThreadPool.QueueUserWorkItem(_ =>
            {
                List<Release> rels = null; string err = null;
                try { rels = GitHub.All(repo); } catch (Exception ex) { err = ex.Message; }
                try
                {
                    BeginInvoke((Action)(() =>
                    {
                        if (rels == null) { info.Text = "Could not load releases: " + err; fromGitHub.Enabled = true; return; }
                        var have = new HashSet<string>(list.Items.Cast<Item>().Where(i => i.Backup != null).Select(i => Backups.SafeName(i.Backup.Version)), StringComparer.OrdinalIgnoreCase);
                        int n = 0;
                        foreach (var r in rels.Where(r => !r.Prerelease && (r.UpdateZip != null || r.FullZip != null)))
                        {
                            list.Items.Add(new Item { Rel = r }); n++;
                        }
                        info.Text = n == 0 ? "No releases with a zip were found." : n + " GitHub releases added below the backups. A backup is quicker and exact when the version has one.";
                    }));
                }
                catch { }
            });
        }

        void Accept()
        {
            var it = list.SelectedItem as Item;
            if (it == null) return;
            if (it.Backup != null) { Backup = it.Backup; DialogResult = DialogResult.OK; return; }
            if (preview) { DialogResult = DialogResult.Cancel; return; }
            Cursor = Cursors.WaitCursor;
            try { Release = Manifest.ForRelease(it.Rel, s => { }); DialogResult = DialogResult.OK; }
            catch (Exception ex) { info.Text = "Could not read that release: " + ex.Message; }
            finally { Cursor = Cursors.Default; }
        }
    }
}
