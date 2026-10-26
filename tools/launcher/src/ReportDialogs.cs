// HERO VR launcher - the crash prompt, Report a bug and the one-time play stats question (Version 0.0.6).
// Each shows exactly what would be sent; sending opens a filled-in GitHub issue the player submits themselves.
using System;
using System.Drawing;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    class ReportDialog : Form
    {
        public const string PrivacyLine = "Nothing is sent until you press Submit on GitHub. No names, PC name or folders are included.";
        protected readonly TextBox preview = new TextBox();
        protected readonly Button primary = new Button(), secondary = new Button(), third = new Button();

        protected ReportDialog(string title, string heading, string sub, int height)
        {
            Text = LauncherInfo.Title + " - " + title;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = MinimizeBox = false; ShowInTaskbar = false;
            StartPosition = FormStartPosition.CenterParent;
            BackColor = Color.FromArgb(16, 17, 24); ForeColor = Theme.Text; Font = Theme.Ui(12.5f);
            AutoScaleMode = AutoScaleMode.None;
            ClientSize = new Size(Theme.S(640), Theme.S(height));
            Controls.Add(new Label { Text = heading, Font = Theme.F("ui", 17, FontStyle.Bold), ForeColor = Theme.Text, AutoSize = false, Bounds = Theme.R(24, 16, 592, 32) });
            Controls.Add(new Label { Text = sub, ForeColor = Theme.Sub, AutoSize = false, Bounds = Theme.R(24, 50, 592, 44) });
            preview.Multiline = true; preview.ReadOnly = true; preview.ScrollBars = ScrollBars.Vertical; preview.WordWrap = true;
            preview.BackColor = Theme.Field; preview.ForeColor = Theme.Silver; preview.BorderStyle = BorderStyle.FixedSingle; preview.Font = Theme.Mono(10.5f);
            preview.TabStop = false;
            Shown += (s, e) => { preview.SelectionStart = 0; preview.SelectionLength = 0; primary.Focus(); };
        }

        protected static void Style(Button b, string text, bool main)
        {
            b.Text = text; b.FlatStyle = FlatStyle.Flat; b.Font = Theme.UiBold(12);
            b.BackColor = main ? Theme.Red : Color.FromArgb(34, 36, 46); b.ForeColor = Color.White;
            b.FlatAppearance.BorderColor = main ? Theme.RedHot : Theme.PanelBorder;
            b.Cursor = Cursors.Hand;
        }
    }

    /// <summary>"Send this crash report to help fix it?" - asked once per crash, at the next launch.</summary>
    class CrashDialog : ReportDialog
    {
        public bool DontAskAgain;
        public CrashDialog(CrashFinding c, string root) : base("Crash report", "Send this crash report to help fix it?",
            c.Kind + " One click opens it on GitHub, filled in; you press Submit there.", 430)
        {
            preview.Text = c.Body.Replace("\r", "").Replace("\n", "\r\n");
            preview.Bounds = Theme.R(24, 100, 592, 230);
            Controls.Add(preview);
            Controls.Add(new Label { Text = PrivacyLine, ForeColor = Theme.Dim, AutoSize = false, Bounds = Theme.R(24, 336, 592, 22), Font = Theme.Ui(11) });
            Style(primary, "SEND REPORT", true); primary.Bounds = Theme.R(24, 372, 170, 38);
            Style(secondary, "NOT NOW", false); secondary.Bounds = Theme.R(204, 372, 120, 38);
            Style(third, "DON'T ASK AGAIN", false); third.Bounds = Theme.R(458, 372, 158, 38);
            primary.Click += (s, e) => { Telemetry.Open("[crash] " + c.Kind, c.Body); DialogResult = DialogResult.OK; };
            secondary.DialogResult = DialogResult.Cancel; CancelButton = secondary;
            third.Click += (s, e) => { DontAskAgain = true; DialogResult = DialogResult.Abort; };
            Controls.AddRange(new Control[] { primary, secondary, third });
            AcceptButton = primary;
        }
    }

    /// <summary>Report a bug: a short description, and the mod version, GPU and headset if ticked.</summary>
    class BugDialog : ReportDialog
    {
        readonly TextBox what = new TextBox();
        readonly CheckBox system = new CheckBox();
        readonly string root;
        public const int MaxChars = 1200;

        public BugDialog(string root) : base("Report a bug", "Report a bug", "Say in a line or two what happened and what you expected; it opens on GitHub, filled in.", 470)
        {
            this.root = root;
            Shown += (s, e) => what.Focus();
            what.Multiline = true; what.MaxLength = MaxChars; what.ScrollBars = ScrollBars.Vertical;
            what.BackColor = Theme.Field; what.ForeColor = Theme.Text; what.BorderStyle = BorderStyle.FixedSingle;
            what.Bounds = Theme.R(24, 98, 592, 100);
            system.Text = "Add the mod version, GPU and VR runtime (no personal data)"; system.Checked = true;
            system.ForeColor = Theme.Silver; system.Bounds = Theme.R(24, 204, 592, 24);
            preview.Bounds = Theme.R(24, 232, 592, 130);
            Controls.Add(new Label { Text = PrivacyLine, ForeColor = Theme.Dim, AutoSize = false, Bounds = Theme.R(24, 368, 592, 22), Font = Theme.Ui(11) });
            Style(primary, "OPEN ON GITHUB", true); primary.Bounds = Theme.R(24, 410, 180, 38);
            Style(secondary, "CANCEL", false); secondary.Bounds = Theme.R(508, 410, 108, 38);
            secondary.DialogResult = DialogResult.Cancel; CancelButton = secondary;
            what.TextChanged += (s, e) => Refresh2();
            system.CheckedChanged += (s, e) => Refresh2();
            primary.Click += (s, e) =>
            {
                if (what.Text.Trim().Length < 3) { MessageBox.Show(this, "Write a few words about the bug first.", Text); return; }
                string first = what.Text.Trim().Replace("\r", " ").Replace("\n", " ");
                Telemetry.Open("[bug] " + (first.Length > 70 ? first.Substring(0, 70) + "..." : first), Body());
                DialogResult = DialogResult.OK;
            };
            Controls.AddRange(new Control[] { what, system, preview, primary, secondary });
            Refresh2();
        }

        string Body()
        {
            string body = "**What happened**\n" + what.Text.Trim().Replace("\r", "") + "\n";
            if (system.Checked) body += "\n```\n" + Telemetry.Header(root, true) + "```\n";
            return Privacy.Scrub(body);
        }
        void Refresh2() { preview.Text = Body().Replace("\n", "\r\n"); }
    }

    static class StatsQuestion
    {
        public const string Text =
            "Share play stats to help make HERO VR run better?\n\n" +
            "After each VR session the launcher keeps a short summary: frame rate (average and low), stutters, " +
            "which heroes and features were on, the mod version, your GPU and headset model.\n\n" +
            "Never included: your name, PC name, folders, files, IP address or anything you type.\n\n" +
            "Nothing is sent by itself: Reports > Send play stats opens it on GitHub for you to submit. " +
            "You can change this any time in Options > Privacy (no restart needed).\n\n" +
            "Share play stats?";

        /// <summary>Asked once (the answer is remembered; No is the default button).</summary>
        public static void AskOnce(IWin32Window owner, Settings settings)
        {
            if (settings.GetBool(Telemetry.KeyAsked, false)) return;
            var r = MessageBox.Show(owner, Text, LauncherInfo.Title + " - Play stats", MessageBoxButtons.YesNo, MessageBoxIcon.Question, MessageBoxDefaultButton.Button2);
            settings.SetBool(Telemetry.KeyStats, r == DialogResult.Yes);
            settings.SetBool(Telemetry.KeyAsked, true);
            settings.Save();
        }
    }
}
