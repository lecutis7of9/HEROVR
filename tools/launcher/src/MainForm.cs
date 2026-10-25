// SM2 VR Launcher - the window: a full-bleed picture, the SM2 VR wordmark (by Agent Mulder), a frosted action bar with
// PLAY VR in the middle, and the sections (Status, Options, Reports, Updates, Credits) as slide-up sheets.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    enum Job { None, Vr, Build, Update, Reports }

    partial class MainForm : Form, IBackdrop
    {
        readonly Settings settings;
        string root;
        string gamePath;
        Dictionary<string, string> scriptParams = new Dictionary<string, string>();
        Job job = Job.None;
        PsRunner vr;
        DateTime stopAsked = DateTime.MinValue;
        readonly bool preview = SystemInfo.Preview;
        Bitmap backdrop;
        Image background;
        readonly System.Windows.Forms.Timer poll = new System.Windows.Forms.Timer { Interval = 3000 };
        readonly ToolTip tip = new ToolTip();

        // Header
        Chip versionChip, updatePill;
        AccentButton btnReportsTop;
        IconButton btnFolder, btnTikTok, btnDiscord;
        Wordmark wordmark;
        ActionBar tabs;
        Page[] pages;
        Rectangle[] sheetBounds;
        RichLine footer, footerRight;
        readonly System.Windows.Forms.Timer slide = new System.Windows.Forms.Timer { Interval = 15 };
        int slideStep; Page slidePage;
        public const string Byline = "Made by Agent Mulder";

        // Sheet indexes (the old tab numbers are kept: SM2VR_LAUNCHER_TAB=0..3 still opens the same section).
        const int SheetStatus = 0, SheetOptions = 1, SheetUpdates = 2, SheetCredits = 3, SheetReports = 4;

        // PLAY
        TextLabel stateTitle, stateSub, optionsSummary;
        StatusRow step1, step2, step3;
        AccentButton btnPlay, btnFlat, btnStop, btnChangeOptions;
        StatusRow rowGame, rowGameVer, rowVr, rowBuild;
        LogBox sessionLog;

        public MainForm()
        {
            settings = Settings.Load();
            Text = LauncherInfo.Title + " Launcher " + LauncherInfo.Version + "  \u2014  " + Byline;
            using (var g = CreateGraphics()) Theme.Scale = Math.Max(1f, g.DpiX / 96f);
            AutoScaleMode = AutoScaleMode.None;
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Theme.Bg;
            ForeColor = Theme.Text;
            ClientSize = new Size(Theme.S(1180), Theme.S(760));
            DoubleBuffered = true;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint, true);
            try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
            try
            {
                var s = typeof(MainForm).Assembly.GetManifestResourceStream("background.jpg");
                if (s != null) background = Image.FromStream(s);
            }
            catch { }
            if (background == null)
            {
                string f = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "background.jpg");
                if (File.Exists(f)) background = Image.FromFile(f);
            }
            try { if (Environment.OSVersion.Platform == PlatformID.Win32NT) Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
            root = SystemInfo.FindPortRoot(settings.Get("PortRoot", null));
            BuildHeader();
            BuildPlay();
            BuildOptions();
            BuildUpdates();
            BuildCredits();
            BuildReports();
            BuildFooter();
            BuildBar();
            foreach (var pg in pages)
                foreach (Control c in pg.Controls) { var gp = c as GlassPanel; if (gp != null) gp.HandleClicked += (s, e) => tabs.Select(-1); }
            tabs.SelectedChanged += (s, e) => ShowPage(tabs.Selected);
            slide.Tick += (s, e) => SlideStep();
            KeyPreview = true;
            tabs.Selected = SheetStatus;
            ShowPage(SheetStatus);
            poll.Tick += (s, e) => { if (!preview) RefreshLive(); };
            sessionLog.FilePath = LogFile("session");
            buildLog.FilePath = LogFile("build-update");
        }

        static string LogFile(string name)
        {
            try
            {
                string d = Path.Combine(Settings.Folder, "logs");
                Directory.CreateDirectory(d);
                foreach (var old in new DirectoryInfo(d).GetFiles("*.log").OrderByDescending(f => f.LastWriteTime).Skip(20)) try { old.Delete(); } catch { }
                return Path.Combine(d, name + "-" + DateTime.Now.ToString("yyyyMMdd-HHmmss") + ".log");
            }
            catch { return null; }
        }

        // ---- Backdrop: the user's picture, scaled to cover, darkened with gradients --------------------------

        void EnsureBackdrop()
        {
            if (backdrop != null && backdrop.Size == ClientSize) return;
            if (backdrop != null) backdrop.Dispose();
            int w = Math.Max(1, ClientSize.Width), h = Math.Max(1, ClientSize.Height);
            backdrop = new Bitmap(w, h);
            using (var g = Graphics.FromImage(backdrop))
            {
                Theme.Hq(g);
                g.Clear(Theme.Bg);
                if (background != null)
                {
                    float k = Math.Max(w / (float)background.Width, h / (float)background.Height);
                    float iw = background.Width * k, ih = background.Height * k;
                    g.DrawImage(background, (w - iw) / 2f, (h - ih) / 2f, iw, ih);
                }
                // Full bleed: a light overall dim, darker top and bottom bands, and a soft vignette towards every edge.
                using (var b = new SolidBrush(Color.FromArgb(36, 4, 5, 10))) g.FillRectangle(b, 0, 0, w, h);
                using (var b = new LinearGradientBrush(new Rectangle(0, 0, w, h), Color.Black, Color.Black, 90f))
                {
                    b.InterpolationColors = new ColorBlend
                    {
                        Positions = new[] { 0f, 0.16f, 0.45f, 0.70f, 1f },
                        Colors = new[] { Color.FromArgb(190, 5, 6, 10), Color.FromArgb(40, 5, 6, 10), Color.FromArgb(0, 5, 6, 10), Color.FromArgb(70, 5, 6, 10), Color.FromArgb(235, 5, 6, 10) }
                    };
                    g.FillRectangle(b, 0, 0, w, h);
                }
                using (var path = new GraphicsPath())
                {
                    path.AddEllipse(-w * 0.21f, -h * 0.21f, w * 1.42f, h * 1.42f);
                    using (var b = new PathGradientBrush(path))
                    {
                        b.CenterColor = Color.FromArgb(0, 4, 5, 9);
                        b.SurroundColors = new[] { Color.FromArgb(255, 4, 5, 9) };
                        b.Blend = new Blend { Positions = new[] { 0f, 0.22f, 0.42f, 0.7f, 1f }, Factors = new[] { 0f, 0.35f, 0.75f, 1f, 1f } };
                        g.FillRectangle(b, 0, 0, w, h);
                    }
                }
                // The frosted action bar is part of the backdrop, so PLAY VR and the bar buttons sit on real glass.
                var bar = BarRect;
                using (var behind = new Bitmap(bar.Width, bar.Height))
                {
                    using (var bg = Graphics.FromImage(behind)) bg.DrawImage(backdrop, new Rectangle(0, 0, bar.Width, bar.Height), bar, GraphicsUnit.Pixel);
                    var state = g.Save();
                    g.TranslateTransform(bar.X, bar.Y);
                    Frost.Pane(g, behind, new RectangleF(0.5f, 0.5f, bar.Width - 1.5f, bar.Height - 1.5f), bar.Height / 2f,
                        Color.FromArgb(150, 16, 17, 25), Color.FromArgb(178, 9, 10, 16));
                    g.Restore(state);
                }
            }
        }
        Rectangle BarRect { get { return Theme.R(36, 662, 1108, 72); } }
        protected override void OnPaintBackground(PaintEventArgs e) { EnsureBackdrop(); e.Graphics.DrawImageUnscaled(backdrop, 0, 0); }
        public void DrawBackdrop(Graphics g, Rectangle src, Point dest)
        {
            EnsureBackdrop();
            g.DrawImage(backdrop, new Rectangle(dest, src.Size), src, GraphicsUnit.Pixel);
        }

        [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);
        protected override void OnHandleCreated(EventArgs e)
        {
            base.OnHandleCreated(e);
            try { int on = 1; DwmSetWindowAttribute(Handle, 20, ref on, 4); } catch { }   // dark title bar (Windows 10 2004+)
        }

        // ---- Header, tabs, footer -------------------------------------------------------------------------

        T Add<T>(Control parent, T c, int x, int y, int w, int h) where T : Control
        {
            c.Bounds = Theme.R(x, y, w, h);
            parent.Controls.Add(c);
            return c;
        }

        void BuildHeader()
        {
            // Top left: the small mark and the installed version. Top right: the update pill and round icons.
            var mark = Add(this, new RichLine(), 36, 26, 96, 28);
            mark.Add("HERO", Theme.UiBold(13), Theme.Text).Add("\u00B7", Theme.UiBold(13), Theme.Red).Add("VR", Theme.UiBold(13), Theme.Text);
            versionChip = Add(this, new Chip("") { Plain = true, Fore = Theme.Sub, TextFont = Theme.Ui(12.5f) }, 130, 26, 120, 28);
            btnFolder = Add(this, new IconButton("folder"), 1110, 24, 34, 34);
            btnDiscord = Add(this, new IconButton("discord"), 1068, 24, 34, 34);
            btnTikTok = Add(this, new IconButton("tiktok"), 1026, 24, 34, 34);
            updatePill = Add(this, new Chip("Update available") { Visible = false, Cursor = Cursors.Hand, Dot = true, RightEdge = Theme.S(1014) }, 820, 26, 190, 30);
            updatePill.Click += (s, e) => tabs.Select(SheetUpdates);
            btnFolder.Click += (s, e) => SystemInfo.OpenFolder(root, null);
            btnTikTok.Click += (s, e) => SystemInfo.OpenUrl(Credits.TikTokUrl);
            btnDiscord.Click += (s, e) => SystemInfo.OpenUrl(Credits.DiscordUrl);
            tip.SetToolTip(btnFolder, "Open the mod folder");
            tip.SetToolTip(btnTikTok, "Agent Mulder on TikTok (@agentmulder)");
            tip.SetToolTip(btnDiscord, "The SM2 VR Discord server");
            tip.SetToolTip(updatePill, "Open Updates");

            wordmark = Add(this, new Wordmark(), 444, 80, 700, 190);

            // The sections are sheets that slide up above the bar. Status and Reports are compact; the rest are tall.
            sheetBounds = new[]
            {
                Theme.R(210, 286, 760, 346),   // Status
                Theme.R(36, 76, 1108, 556),    // Options
                Theme.R(36, 76, 1108, 556),    // Updates (and Build, Revert)
                Theme.R(36, 76, 1108, 556),    // Credits
                Theme.R(230, 286, 720, 346),   // Reports and the session log
            };
            pages = new Page[sheetBounds.Length];
            for (int i = 0; i < pages.Length; i++)
            {
                pages[i] = new Page { Bounds = sheetBounds[i], Visible = false };
                Controls.Add(pages[i]);
                pages[i].BringToFront();
            }
        }

        void BuildBar()
        {
            // Bottom: Status, Options, Reports | Flatscreen, PLAY VR, (Stop VR) | Updates, Revert, Credits.
            tabs = Add(this, new ActionBar(), 36, 662, 1108, 72);
            tabs.Add("Status", "status", SheetStatus, null, false);
            tabs.Add("Options", "options", SheetOptions, null, false);
            tabs.Add("Reports", "reports", SheetReports, null, false);
            tabs.Add("Updates", "download", SheetUpdates, null, true);
            tabs.Add("Revert", "revert", -1, () => Revert(), true);
            tabs.Add("Credits", "credits", SheetCredits, null, true);
            btnFlat = Add(this, new AccentButton("Flatscreen", ButtonKind.Secondary) { Glyph = "monitor", TextFont = Theme.UiBold(12) }, 320, 678, 114, 40);
            btnPlay = Add(this, new AccentButton("Play VR", ButtonKind.Primary) { Hero = true, TextFont = Theme.Heavy(27) }, 435, 640, 310, 100);
            btnStop = Add(this, new AccentButton("Stop VR", ButtonKind.Danger) { Glyph = "stop", TextFont = Theme.UiBold(12), Visible = false }, 756, 678, 106, 40);
            btnPlay.Click += (s, e) => PlayVr();
            btnFlat.Click += (s, e) => PlayFlat();
            btnStop.Click += (s, e) => StopVr();
            btnFlat.BringToFront(); btnPlay.BringToFront(); btnStop.BringToFront();
            tip.SetToolTip(btnFlat, "Starts the game normally, without VR (launch-game-vr.ps1 has no flatscreen mode). In VR, Settings > SPIDY VR > VIEW also switches to a flat screen.");
            tip.SetToolTip(btnStop, "Ends VR like Ctrl+C: the game's settings are put back and the game keeps running.");
        }

        void ShowPage(int i)
        {
            slide.Stop();
            for (int k = 0; k < pages.Length; k++) if (k != i) pages[k].Visible = false;
            if (i < 0 || i >= pages.Length) { if (slidePage != null) slidePage.Bounds = sheetBounds[Array.IndexOf(pages, slidePage)]; return; }
            var p = pages[i];
            p.Bounds = sheetBounds[i];
            p.BringToFront();
            if (preview || !IsHandleCreated || !Visible)
            {
                p.Visible = true;
                Backdrop.InvalidateAll(p);
                return;
            }
            // Slide up from behind the bar: the sheet grows upwards from a short strip to its full height.
            slidePage = p; slideStep = 0;
            SlideStep();
            p.Visible = true;
            slide.Start();
        }

        void SlideStep()
        {
            const int steps = 7;
            if (slidePage == null) { slide.Stop(); return; }
            int i = Array.IndexOf(pages, slidePage);
            var home = sheetBounds[i];
            slideStep++;
            double t = Math.Min(1.0, slideStep / (double)steps);
            double ease = 1 - Math.Pow(1 - t, 3);
            int lift = (int)Math.Round(Theme.S(48) * (1 - ease));
            slidePage.Bounds = new Rectangle(home.X, home.Y + lift, home.Width, home.Height - lift);
            Backdrop.InvalidateAll(slidePage);
            if (t >= 1) { slide.Stop(); slidePage.Bounds = home; }
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            if (keyData == Keys.Escape && tabs != null && tabs.Selected >= 0) { tabs.Select(-1); return true; }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        void BuildFooter()
        {
            var f = Theme.Ui(11); var fb = Theme.UiBold(11); string dot = "  \u00B7  ";
            footer = Add(this, new RichLine { Tip = tip }, 40, 739, 640, 20);
            footer.Add("Made by Agent Mulder", fb, Theme.Silver)
                  .Add(dot, f, Theme.Dim)
                  .Act("License & credits", f, () => tabs.Select(SheetCredits));
            footerRight = Add(this, new RichLine { Align = StringAlignment.Far }, 640, 739, 500, 20);
            footerRight.Add("Launcher " + LauncherInfo.Version + dot + "GPLv3" + dot + "not affiliated with Insomniac, Sony, Marvel or Valve", Theme.Ui(10.5f), Theme.Dim);
        }

        // ---- STATUS sheet ----------------------------------------------------------------------------------

        void BuildPlay()
        {
            var card = Add(pages[SheetStatus], new GlassPanel("Status"), 0, 0, 760, 346);
            stateTitle = Add(card, new TextLabel("CHECKING...", Theme.F("ui", 14, FontStyle.Bold), Theme.Text) { Wrap = false, Align = StringAlignment.Far }, 390, 17, 348, 24);
            stateSub = Add(card, new TextLabel("", Theme.Ui(12), Theme.Sub), 22, 44, 716, 22);
            // The three step rows of the old PLAY card fed the same checks; they are kept (not shown) so the logic is unchanged.
            step1 = new StatusRow("1  Headset connected") { Separator = false };
            step2 = new StatusRow("2  VR mod built") { Separator = false };
            step3 = new StatusRow("3  Press PLAY VR") { Separator = false };
            step3.Set(CheckState.Info, "The game starts; put on the headset when it says VR started.");
            rowGame = Add(card, new StatusRow("Game"), 18, 68, 724, 58);
            rowGameVer = Add(card, new StatusRow("Game version"), 18, 126, 724, 58);
            rowVr = Add(card, new StatusRow("VR runtime"), 18, 184, 724, 58);
            rowBuild = Add(card, new StatusRow("VR mod"), 18, 242, 724, 58);
            optionsSummary = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub), 22, 306, 536, 34);
            btnChangeOptions = Add(card, new AccentButton("Change launch options \u203A", ButtonKind.Ghost) { TextFont = Theme.UiBold(12.5f) }, 568, 309, 176, 22);
            btnChangeOptions.Click += (s, e) => tabs.Select(SheetOptions);
            var browse = new AccentButton("Browse", ButtonKind.Secondary) { Glyph = "folder" };
            rowGame.AddAction(browse);
            browse.Click += (s, e) => BrowseGame();
            var recheck = new AccentButton("Re-check", ButtonKind.Secondary) { Glyph = "refresh" };
            rowVr.AddAction(recheck);
            recheck.Click += (s, e) => RefreshAll();
            var build = new AccentButton("Build", ButtonKind.Secondary) { Glyph = "build" };
            rowBuild.AddAction(build);
            build.Click += (s, e) => StartBuild(null);
            var modFolder = new AccentButton("Mod folder", ButtonKind.Secondary) { Glyph = "folder" };
            rowGameVer.AddAction(modFolder);
            modFolder.Click += (s, e) => BrowseRoot();
            modFolder.Text = "Mod folder";
            tip.SetToolTip(modFolder, "Pick the folder the mod is installed in (the one with tools\\launch-game-vr.ps1)");
            rowBuild.Separator = false;
        }

        // ---- REPORTS sheet: the session log and collect-reports -------------------------------------------

        void BuildReports()
        {
            var card = Add(pages[SheetReports], new GlassPanel("Reports & session log"), 0, 0, 720, 346);
            sessionLog = Add(card, new LogBox(), 20, 50, 680, 226);
            var copy = Add(card, new AccentButton("Copy log", ButtonKind.Ghost) { TextFont = Theme.UiBold(12) }, 628, 18, 72, 22);
            btnReportsTop = Add(card, new AccentButton("Collect reports", ButtonKind.Secondary) { Glyph = "zip" }, 20, 290, 172, 38);
            var rep = Add(card, new AccentButton("Open reports folder", ButtonKind.Secondary) { Glyph = "folder" }, 202, 290, 196, 38);
            Add(card, new TextLabel("Collect reports runs tools\\collect-reports.ps1: it makes reports\\send-me.zip and shows it in Explorer.", Theme.Ui(11.5f), Theme.Sub), 412, 288, 290, 44);
            copy.Click += (s, e) => { try { Clipboard.SetText(sessionLog.AllText.Length > 0 ? sessionLog.AllText : " "); } catch { } };
            rep.Click += (s, e) => SystemInfo.OpenFolder(Path.Combine(root, "reports"), null);
            btnReportsTop.Click += (s, e) => CollectReports();
            tip.SetToolTip(btnReportsTop, "tools\\collect-reports.ps1: makes reports\\send-me.zip and shows it in Explorer");
        }

        void SetState(string title, Color color, string sub)
        {
            stateTitle.Text = title; stateTitle.ForeColor = color; stateSub.Text = sub;
        }

        // ---- Status ----------------------------------------------------------------------------------------

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            if (preview) { RefreshAll(); FillPreview(); return; }
            // The update check goes first and on its own: no problem in the status checks below can stop it
            // (the 0.0.5 launcher's Trim(Char) crash in the status check also stopped its update check).
            string what = null, pending = null;
            try { pending = Backups.PendingRollback(root, out what); } catch { }
            bool rollback = pending != null && Directory.Exists(pending);
            if (!rollback) StartupUpdateCheck();
            try { RefreshAll(); }
            catch (Exception ex) { Program.LogError(ex); try { buildLog.Line("A status check failed: " + ex.Message + " (details in launcher-error.log). The launcher keeps working.", Theme.Bad); } catch { } }
            try { ShowJustUpdated(); } catch { }
            poll.Start();
            if (rollback)
            {
                var r = MessageBox.Show(this, "The launcher was closed or the PC stopped while it was busy (" + what + ").\n\nPut back the version from before (" +
                    Path.GetFileName(pending) + ")? This is recommended: the half-installed files may not work.", LauncherInfo.Title + " - unfinished update",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
                if (r == DialogResult.Yes) RunInstall("Finishing the rollback", inst => inst.FinishRollback(pending));
            }
        }

        void RefreshAll()
        {
            if (preview) return;
            scriptParams = SystemInfo.ScriptParams(Path.Combine(root, "tools", "launch-game-vr.ps1"));
            ApplyScriptParams();
            string where = "not searched";
            string saved = settings.Get("GamePath", null);
            gamePath = !string.IsNullOrEmpty(saved) && File.Exists(saved) ? saved : SystemInfo.FindGame(out where);
            if (gamePath == null)
                rowGame.Set(CheckState.Bad, "Marvel's Spider-Man 2 was not found in Steam (" + where + "). Press Browse and pick Spider-Man2.exe.");
            else
                rowGame.Set(CheckState.Ok, gamePath);
            if (gamePath != null)
            {
                string v = SystemInfo.FileVersion(gamePath);
                if (v == SystemInfo.SupportedVersion) rowGameVer.Set(CheckState.Ok, "v" + v + ", the version this mod supports. Mod folder: " + root);
                else rowGameVer.Set(CheckState.Bad, "v" + (v ?? "?") + ": the mod is made for v" + SystemInfo.SupportedVersion + "; it will refuse a different game version. Mod folder: " + root);
            }
            else rowGameVer.Set(CheckState.Warn, "Unknown until the game is found. Mod folder: " + root);
            RefreshLive();
            UpdateVersionUi();
            RefreshDownloads();
            RefreshBackupsUi();
        }

        void RefreshLive()
        {
            string vrText;
            var vrState = SystemInfo.VrStatus(out vrText);
            rowVr.Set(vrState, vrText);
            step1.Set(vrState == CheckState.Ok ? CheckState.Ok : CheckState.Warn, vrState == CheckState.Ok ? "VR is running." : "Open Virtual Desktop or SteamVR and connect the headset.");
            DateTime? built; string stale;
            SystemInfo.BuildState(root, out built, out stale);
            if (!SystemInfo.IsPortRoot(root))
            {
                rowBuild.Set(CheckState.Bad, "No mod in " + root + ". Pick its folder (Mod folder) or install an update zip (Updates).");
                step2.Set(CheckState.Bad, "The mod folder was not found.");
            }
            else if (built == null)
            {
                rowBuild.Set(CheckState.Warn, "Not built yet. Press Build (tools\\build.ps1 -Observer); the first build takes a few minutes.");
                step2.Set(CheckState.Warn, "Not built yet: PLAY VR builds it first.");
            }
            else if (stale != null)
            {
                rowBuild.Set(CheckState.Warn, "Built " + built.Value.ToString("d MMM HH:mm") + ", but " + stale + " is newer. Build again.");
                step2.Set(CheckState.Warn, "Out of date: press Build.");
            }
            else
            {
                rowBuild.Set(CheckState.Ok, (SystemInfo.PortVersion(root) ?? "") + ", built " + built.Value.ToString("d MMM yyyy HH:mm") + ".");
                step2.Set(CheckState.Ok, "Built and up to date.");
            }
            UpdatePlayState();
        }

        void UpdateVersionUi()
        {
            // "Version 0.0.1" (VERSION, else PORT-VERSION); an install from before 0.0.1 shows its legacy "Update NN.N".
            string v = preview ? "Version 0.0.5" : (SystemInfo.PortVersion(root) ?? "not installed");
            string vLong = preview ? "Version 0.0.5 (build 38.9)" : (SystemInfo.PortVersionLong(root) ?? "not installed");
            versionChip.Text = v;
            versionChip.FitWidth();
            if (upInstalled != null) upInstalled.Set(SystemInfo.PortVersion(root) != null || preview ? CheckState.Ok : CheckState.Bad, vLong + "  \u00B7  " + root);
        }

        void UpdatePlayState()
        {
            bool running = SystemInfo.GameRunning();
            bool idle = job == Job.None;
            btnPlay.Enabled = idle;
            btnFlat.Enabled = idle && !running;
            btnStop.Visible = job == Job.Vr;
            btnStop.Enabled = job == Job.Vr;
            btnReportsTop.Enabled = job != Job.Update && job != Job.Reports;
            SetUpdateButtons();
            optionsSummary.Text = "Launch options: " + LaunchOptions.Summary(OptValue);
            if (job == Job.Vr) return;
            if (job == Job.Build) { SetState("BUILDING...", Theme.Silver, "tools\\build.ps1 -Observer is running (see Updates)."); return; }
            if (job == Job.Update) { SetState("UPDATING...", Theme.Silver, "Installing; see Updates."); return; }
            if (job == Job.Reports) { SetState("COLLECTING...", Theme.Silver, "Making reports\\send-me.zip."); return; }
            if (lastError != null) { SetState("VR STOPPED", Theme.Bad, lastError); return; }
            if (!SystemInfo.IsPortRoot(root)) SetState("MOD NOT FOUND", Theme.Bad, "Pick the mod folder or install an update zip.");
            else if (gamePath == null) SetState("GAME NOT FOUND", Theme.Warn, "Press Browse and pick Spider-Man2.exe.");
            else if (running) SetState("GAME RUNNING", Theme.Warn, "PLAY VR attaches VR to the running game.");
            else SetState("READY", Theme.Good, "Everything checks out. Put the headset on after the game opens.");
        }
        string lastError;

        void BrowseGame()
        {
            using (var d = new OpenFileDialog { Title = "Pick Spider-Man2.exe", Filter = "Spider-Man 2|Spider-Man2.exe|Programs|*.exe", FileName = "Spider-Man2.exe" })
            {
                if (gamePath != null) d.InitialDirectory = Path.GetDirectoryName(gamePath);
                if (d.ShowDialog(this) != DialogResult.OK) return;
                settings.Set("GamePath", d.FileName); settings.Save();
                RefreshAll();
            }
        }

        void BrowseRoot()
        {
            using (var d = new FolderBrowserDialog { Description = "Pick the mod folder (the one with PORT-VERSION and tools\\launch-game-vr.ps1), e.g. C:\\sm2-port", SelectedPath = root })
            {
                if (d.ShowDialog(this) != DialogResult.OK) return;
                if (!SystemInfo.IsPortRoot(d.SelectedPath) &&
                    MessageBox.Show(this, d.SelectedPath + " has no tools\\launch-game-vr.ps1. Use it anyway (for installing an update into it)?", LauncherInfo.Title,
                        MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
                root = d.SelectedPath;
                settings.Set("PortRoot", root); settings.Save();
                RefreshAll();
            }
        }

        // ---- Running things --------------------------------------------------------------------------------

        void Ui(Action a) { if (IsDisposed) return; if (InvokeRequired) { try { BeginInvoke(a); } catch { } } else a(); }

        bool GuardIdle()
        {
            if (job == Job.None) return true;
            MessageBox.Show(this, "Wait for the current task to finish first (" + job.ToString().ToLower() + ").", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Information);
            return false;
        }

        void PlayVr()
        {
            if (!GuardIdle()) return;
            RefreshAll();
            lastError = null;
            if (!SystemInfo.IsPortRoot(root)) { MessageBox.Show(this, "The mod was not found in " + root + ". Use Mod folder to pick it, or install an update zip in Updates.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
            var extra = new List<string>();
            if (FolderLock.Busy(root)) { MessageBox.Show(this, "Another launcher is updating or building " + root + " right now. Wait for it to finish, then press PLAY VR.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            if (SystemInfo.GameRunning())
            {
                if (!scriptParams.ContainsKey("AttachOnly")) { MessageBox.Show(this, "Spider-Man 2 is already running. Close it first, then press PLAY VR.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
                if (MessageBox.Show(this, "Spider-Man 2 is already running. Start VR in the running game (-AttachOnly)?", LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
                extra.Add("-AttachOnly");
            }
            if (gamePath != null)
            {
                string gv = SystemInfo.FileVersion(gamePath);
                if (gv != null && gv != SystemInfo.SupportedVersion &&
                    MessageBox.Show(this, "The game is v" + gv + "; the mod is made for v" + SystemInfo.SupportedVersion + " and will most likely refuse to start VR.\n\nTry anyway?", LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
            }
            string vrText;
            if (SystemInfo.VrStatus(out vrText) != CheckState.Ok &&
                MessageBox.Show(this, "No VR app is running yet (Virtual Desktop, SteamVR or Quest Link).\n\nConnect the headset first, or press Yes to start anyway.", LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
            DateTime? built; string stale;
            SystemInfo.BuildState(root, out built, out stale);
            if (built == null || stale != null)
            {
                string why = built == null ? "The VR mod isn't built yet." : "The VR mod build is older than its files (" + stale + ").";
                var r = MessageBox.Show(this, why + "\n\nBuild it now (tools\\build.ps1 -Observer) and then start VR?", LauncherInfo.Title, MessageBoxButtons.YesNoCancel, MessageBoxIcon.Question);
                if (r == DialogResult.Cancel) return;
                if (r == DialogResult.Yes) { StartBuild(ok => { if (ok) LaunchVr(extra); }); return; }
            }
            LaunchVr(extra);
        }

        void LaunchVr(List<string> extra)
        {
            var args = LaunchOptions.Arguments(OptValue, scriptParams);
            args.AddRange(extra);
            string more = settings.Get("ExtraArgs", "").Trim();
            if (more.Length > 0) args.Add(more);
            sessionLog.Line("> tools\\launch-game-vr.ps1 " + string.Join(" ", args), Theme.Info);
            vr = new PsRunner { Name = "VR" };
            vr.Output += (line, err) => sessionLog.Line(line, err && !line.StartsWith("WARNING") ? Theme.Bad : LogBox.ColorFor(line));
            var lines = new List<string>();
            vr.Output += (line, err) => { lock (lines) { lines.Add(line); if (lines.Count > 60) lines.RemoveAt(0); } };
            vr.Exited += code => Ui(() =>
            {
                job = Job.None;
                stopAsked = DateTime.MinValue;
                if (code == 0) { sessionLog.Line("VR session ended.", Theme.Good); lastError = null; }
                else
                {
                    string last;
                    lock (lines) last = lines.LastOrDefault(l => l.StartsWith("ERROR", StringComparison.OrdinalIgnoreCase) || l.IndexOf("failed", StringComparison.OrdinalIgnoreCase) >= 0 || l.IndexOf("refus", StringComparison.OrdinalIgnoreCase) >= 0) ?? lines.LastOrDefault();
                    lastError = (last ?? "It stopped with exit code " + code + ".").Replace("ERROR: ", "") + " Collect reports to send it.";
                    sessionLog.Line("VR stopped (exit code " + code + "). The game's settings are put back by the session itself when it ends.", Theme.Bad);
                    if (last != null && last.IndexOf("Build first", StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        if (MessageBox.Show(this, "The mod needs a build first. Build now?", LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes) StartBuild(null);
                    }
                }
                RefreshLive();
            });
            job = Job.Vr;
            SetState("VR RUNNING", Theme.Good, "Starting the game and VR. Stop VR ends it cleanly; the game keeps running.");
            UpdatePlayState();
            if (!vr.Start(root, Path.Combine(root, "tools", "launch-game-vr.ps1"), args)) { job = Job.None; UpdatePlayState(); }
        }

        void StopVr()
        {
            if (vr == null || !vr.Running) return;
            if (stopAsked != DateTime.MinValue && (DateTime.Now - stopAsked).TotalSeconds > 45)
            {
                var r = MessageBox.Show(this, "VR has not stopped after " + (int)(DateTime.Now - stopAsked).TotalSeconds + " seconds.\n\nForce stop ends the VR session's PowerShell and Python (never the game). " +
                    "The game's picture settings might then stay as VR set them until the next VR session ends normally.\n\nForce stop now?", LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
                if (r == DialogResult.Yes) { vr.ForceStop(); sessionLog.Line("Force stopped.", Theme.Warn); }
                return;
            }
            sessionLog.Line("Stopping VR (as Ctrl+C)...", Theme.Info);
            if (!vr.RequestStop()) sessionLog.Line("Could not send Ctrl+C. Press Stop VR again in a minute to force it.", Theme.Warn);
            if (stopAsked == DateTime.MinValue) stopAsked = DateTime.Now;
            SetState("STOPPING...", Theme.Silver, "Putting the game's settings back. Press Stop VR again later to force it.");
        }

        void PlayFlat()
        {
            if (!GuardIdle()) return;
            if (SystemInfo.GameRunning()) { MessageBox.Show(this, "The game is already running.", LauncherInfo.Title); return; }
            sessionLog.Line("> Starting Spider-Man 2 without VR.", Theme.Info);
            try
            {
                bool steam = gamePath != null && gamePath.IndexOf("steamapps", StringComparison.OrdinalIgnoreCase) >= 0;
                if (steam || gamePath == null) Process.Start(new ProcessStartInfo("steam://rungameid/" + SystemInfo.SteamAppId) { UseShellExecute = true });
                else Process.Start(new ProcessStartInfo(gamePath) { UseShellExecute = true, WorkingDirectory = Path.GetDirectoryName(gamePath) });
            }
            catch (Exception ex) { MessageBox.Show(this, "Could not start the game: " + ex.Message, LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Error); }
        }

        void StartBuild(Action<bool> then)
        {
            if (!GuardIdle()) return;
            if (SystemInfo.GameRunning()) { MessageBox.Show(this, "Close the game before building: it holds the mod's files.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            FolderLock buildLock;
            try { buildLock = FolderLock.Take(root, "building"); }
            catch (Exception ex) { MessageBox.Show(this, ex.Message, LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            job = Job.Build;
            UpdatePlayState();
            if (then == null) tabs.Select(2);
            buildLog.Line("> tools\\build.ps1 -Observer", Theme.Info);
            sessionLog.Line("> Building the VR mod (log in Updates)...", Theme.Info);
            SetProgress("Building the VR mod...", -1);
            var r = new PsRunner { Name = "Build" };
            r.Output += (l, e) => buildLog.Line(l, e && !l.StartsWith("WARNING") ? Theme.Bad : LogBox.ColorFor(l));
            r.Exited += code => Ui(() =>
            {
                buildLock.Dispose();
                job = Job.None;
                bool ok = code == 0;
                buildLog.Line(ok ? "Build finished." : "Build failed (exit code " + code + "). The first red line above says why.", ok ? Theme.Good : Theme.Bad);
                sessionLog.Line(ok ? "Build finished." : "Build failed: see Updates.", ok ? Theme.Good : Theme.Bad);
                SetProgress(ok ? "Build finished." : "Build failed.", ok ? 100 : 0);
                RefreshLive();
                if (then != null) then(ok);
            });
            if (!r.Start(root, Path.Combine(root, "tools", "build.ps1"), new[] { "-Observer" })) { buildLock.Dispose(); job = Job.None; UpdatePlayState(); }
        }

        void CollectReports()
        {
            if (job == Job.Update || job == Job.Reports) return;
            string script = Path.Combine(root, "tools", "collect-reports.ps1");
            if (!File.Exists(script)) { MessageBox.Show(this, "tools\\collect-reports.ps1 was not found in " + root + ".", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
            var before = job;
            if (job == Job.None) job = Job.Reports;
            UpdatePlayState();
            sessionLog.Line("> tools\\collect-reports.ps1", Theme.Info);
            string last = null;
            var r = new PsRunner();
            r.Output += (l, e) => { sessionLog.Line(l, e ? Theme.Bad : LogBox.ColorFor(l)); if (l.Trim().Length > 0) last = l.Trim(); };
            r.Exited += code => Ui(() =>
            {
                if (job == Job.Reports) job = Job.None;
                string zip = Path.Combine(root, "reports", "send-me.zip");
                if (last != null && last.EndsWith(".zip", StringComparison.OrdinalIgnoreCase) && File.Exists(last)) zip = last;
                if (File.Exists(zip))
                {
                    sessionLog.Line("Reports ready: " + zip + " (shown in Explorer). Send that file.", Theme.Good);
                    SystemInfo.OpenFolder(Path.GetDirectoryName(zip), zip);
                }
                else sessionLog.Line("Collecting reports failed (exit code " + code + ").", Theme.Bad);
                UpdatePlayState();
            });
            r.Start(root, script, null);
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (job == Job.Update)
            {
                MessageBox.Show(this, "An update is being installed. Closing now could leave the mod half installed; please wait for it to finish.", LauncherInfo.Title, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                e.Cancel = true; return;
            }
            if (job == Job.Vr && vr != null && vr.Running)
            {
                var r = MessageBox.Show(this, "VR is running. Stop VR and close the launcher?\n\n(The game keeps running; VR ends and the game's settings are put back.)", LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Question);
                if (r != DialogResult.Yes) { e.Cancel = true; return; }
                vr.RequestStop();
                var until = DateTime.Now.AddSeconds(30);
                Cursor = Cursors.WaitCursor;
                while (vr.Running && DateTime.Now < until) { Application.DoEvents(); Thread.Sleep(100); }
            }
            if (job == Job.Build &&
                MessageBox.Show(this, "A build is running. Close anyway? (The build stops; press Build next time.)", LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
            { e.Cancel = true; return; }
            settings.Save();
            base.OnFormClosing(e);
        }
    }
}
