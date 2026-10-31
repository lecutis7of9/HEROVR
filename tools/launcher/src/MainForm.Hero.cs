// HERO VR launcher - the new design (build 10): a borderless window over most of the screen, 16:9 content on black,
// the orb from the concept video, five slanted tiles and its sound. It drives exactly the same functions as the
// classic design (PLAY VR, Flatscreen, Updates, Options, Revert); the classic sheets open over it in the same
// black and neon green. If drawing fails it switches to a plain picture of the same tiles, never stopping PLAY VR.
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    partial class MainForm
    {
        readonly bool hero;
        HeroScene scene;
        OrbPlayer orb;
        WaveAudio audio;
        readonly Stopwatch heroClock = new Stopwatch(), shownWatch = new Stopwatch();
        // 10 ms, not 16: a Windows timer fires on the system tick (15.6 ms), so 16 ms meant every other tick (32 Hz).
        const int TickMs = 10;
        readonly System.Windows.Forms.Timer heroTimer = new System.Windows.Forms.Timer { Interval = TickMs };
        Rectangle content;
        float heroK = 1f;
        Bitmap overlay, sheetSnapshot;
        string[] overlayKeys;   // what each part of the overlay (HeroScene.Regions) was drawn with
        bool overlayDirty = true, heroBroken, heroStarted, heroPainted, soundOffForGame, gameOn, dragging;
        Point dragFrom, dragWindow;
        double lastTick, heroOffset;
        double HeroNow { get { return heroOffset + heroClock.Elapsed.TotalSeconds; } }
        int padPrev;
        Panel classicChrome;
        ToggleRow designToggle, soundToggle;
        bool designLoading;

        public bool IsHero { get { return hero; } }

        /// <summary>The window: 88% of the screen it opens on (all of a small screen), centred.</summary>
        public static Rectangle HeroWindowBounds(Rectangle work)
        {
            bool small = work.Height < 900 || work.Width < 1500;
            int w = small ? work.Width : (int)Math.Round(work.Width * 0.88);
            int h = small ? work.Height : (int)Math.Round(work.Height * 0.88);
            return new Rectangle(work.X + (work.Width - w) / 2, work.Y + (work.Height - h) / 2, w, h);
        }

        /// <summary>The largest 16:9 rectangle in the client area, centred (the rest is black).</summary>
        public static Rectangle HeroContent(Size client)
        {
            int w = client.Width, h = (int)Math.Round(client.Width * 9 / 16.0);
            if (h > client.Height) { h = client.Height; w = (int)Math.Round(client.Height * 16 / 9.0); }
            return new Rectangle((client.Width - w) / 2, (client.Height - h) / 2, Math.Max(1, w), Math.Max(1, h));
        }

        /// <summary>The sheets' scale in the new design: they grow with the window (0.9 at least, so text stays readable),
        /// but the largest sheet (1108 x 556 at scale 1) always fits inside the picture, even on a very small screen.</summary>
        public static float HeroSheetScale(Rectangle contentRect)
        {
            float grow = Math.Max(0.9f, contentRect.Height / 800f);
            float fit = Math.Min(contentRect.Width / 1140f, contentRect.Height / 580f);
            return Math.Max(0.5f, Math.Min(grow, fit));
        }

        void HeroSetupWindow()
        {
            FormBorderStyle = FormBorderStyle.None;
            StartPosition = FormStartPosition.Manual;
            Rectangle work;
            try { work = Screen.FromPoint(Cursor.Position).WorkingArea; } catch { work = new Rectangle(0, 0, 1920, 1040); }
            string forced = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_SCREEN");   // tests: WxH of the working area
            if (!string.IsNullOrEmpty(forced))
            {
                var p = forced.ToLowerInvariant().Split(new[] { 'x' });
                int fw, fh;
                if (p.Length == 2 && int.TryParse(p[0], out fw) && int.TryParse(p[1], out fh)) work = new Rectangle(0, 0, fw, fh);
            }
            Bounds = HeroWindowBounds(work);
            content = HeroContent(ClientSize);
            heroK = content.Height / 720f;
            Theme.Scale = HeroSheetScale(content);
            BackColor = Color.Black;
            Text = LauncherInfo.Title + " Launcher " + LauncherInfo.Version + "  \u2014  " + Byline;
            if (Environment.OSVersion.Platform == PlatformID.Win32NT && !preview) Opacity = 0;   // faded in with the sound
        }

        protected override CreateParams CreateParams
        {
            get
            {
                var cp = base.CreateParams;
                // Borderless, but still minimisable from the taskbar and with its system menu (Alt+Space, Alt+F4).
                if (hero) cp.Style |= 0x00020000 | 0x00080000;
                return cp;
            }
        }

        /// <summary>After every section is built: the classic header, bar and footer are parked out of sight (they keep
        /// working, so every existing function is untouched), the sheets are centred and the scene starts.</summary>
        void HeroFinishBuild()
        {
            classicChrome = new Panel { Visible = false };
            var keep = new System.Collections.Generic.List<Control>();
            foreach (Control c in Controls) if (Array.IndexOf(pages, c) < 0) keep.Add(c);
            foreach (var c in keep) { Controls.Remove(c); classicChrome.Controls.Add(c); }
            Controls.Add(classicChrome);
            HeroLayoutSheets();
            scene = new HeroScene();
            scene.T = 0;   // nothing but black until the window is shown and the intro starts
            scene.Version = ShortVersion(SystemInfo.PortVersion(root) ?? LauncherInfo.Version).ToUpperInvariant();
            orb = new OrbPlayer(HeroAssets.Orb);
            orb.Layout(heroK, content.Location);
            orb.Show(0);
            heroTimer.Tick += (s, e) => HeroTick();
            MouseMove += (s, e) => HeroMouseMove(e);
            MouseDown += (s, e) => HeroMouseDown(e);
            MouseUp += (s, e) => HeroMouseUp(e);
            MouseLeave += (s, e) => { if (scene != null && (scene.CloseHot || scene.MinHot || scene.StatusHot)) { scene.CloseHot = scene.MinHot = scene.StatusHot = false; HeroOverlayChanged(); } };
            double off;   // preview stills and tests: start this many seconds in (e.g. 6 = idle) and on tile n
            if (double.TryParse(Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_HERO_T"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out off) && off > 0) heroOffset = off;
            int sel;
            if (int.TryParse(Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_HERO_SEL"), out sel) && sel >= 0 && sel < scene.Tiles.Length) { scene.Sel = sel; scene.Target = sel; }
            HeroSync();
        }


        void HeroLayoutSheets()
        {
            var home = new[] { Theme.R(210, 286, 760, 346), Theme.R(36, 76, 1108, 556), Theme.R(36, 76, 1108, 556), Theme.R(36, 76, 1108, 556), Theme.R(230, 286, 720, 346), MultiplayerBounds() };
            for (int i = 0; i < home.Length && i < sheetBounds.Length; i++)
            {
                var r = home[i];
                sheetBounds[i] = new Rectangle(content.X + (content.Width - r.Width) / 2, content.Y + (content.Height - r.Height) / 2, r.Width, r.Height);
                if (!pages[i].Visible) pages[i].Bounds = sheetBounds[i];
            }
        }

        protected override void OnResize(EventArgs e)
        {
            base.OnResize(e);
            if (!hero || scene == null) return;
            if (WindowState == FormWindowState.Minimized) { HeroPauseCheck(); return; }
            var c = HeroContent(ClientSize);
            if (c != content)
            {
                content = c; heroK = content.Height / 720f;
                orb.Layout(heroK, content.Location);
                overlayDirty = true;
                HeroLayoutSheets();
                foreach (var p in pages) if (p.Visible) { p.Bounds = sheetBounds[Array.IndexOf(pages, p)]; }
                if (sheetSnapshot != null) HeroTakeSnapshot();
                Invalidate();
            }
            HeroPauseCheck();
        }

        // ---- Start, sound, pause ----

        void HeroShown()
        {
            if (HeroAssets.TestFail == "hang") { LauncherMode.Log("test: leaving without finishing the start (hang)"); Environment.Exit(9); }
            heroClock.Restart(); shownWatch.Restart();
            lastTick = heroOffset;
            scene.T = Math.Min(heroOffset, HeroScene.IntroEnd);
            heroTimer.Start();
            if (!preview && settings.GetBool(LauncherMode.KeySound, true)) HeroStartSound(false);
        }

        void HeroStartSound(bool skipIntro)
        {
            if (audio != null && audio.Playing) return;
            try
            {
                audio = new WaveAudio(HeroAssets.Intro, HeroAssets.Loop, skipIntro);
                if (!audio.Start(skipIntro ? 1.2 : 0.6)) { LauncherMode.Log("sound off: " + audio.Error); audio = null; }
            }
            catch (Exception ex) { LauncherMode.Log("sound off: " + ex.Message); audio = null; }
        }

        void HeroStopSound(int fadeMs)
        {
            if (audio == null) return;
            try { audio.FadeOut(fadeMs); } catch { }
            audio = null;
        }

        /// <summary>A game was started (VR or flat): the sound fades out and stays off; the orb stops while it runs.</summary>
        void HeroGameStarting()
        {
            if (!hero) return;
            soundOffForGame = true;
            HeroStopSound(400);
            gameOn = true;
            HeroPauseCheck();
        }

        bool SheetOpen { get { return tabs != null && tabs.Selected >= 0; } }

        /// <summary>The orb runs only while it can be seen: not minimised, no sheet open and no game running.</summary>
        void HeroPauseCheck()
        {
            if (!hero || scene == null) return;
            bool run = WindowState != FormWindowState.Minimized && !SheetOpen && !gameOn && !heroBroken && Visible;
            // shownWatch runs from the window's first show: a sheet the launcher opened itself before the first tick
            // (Updates while the mod files install) must not leave the clock stopped, and the home black, for good.
            if (run && !heroClock.IsRunning && (heroStarted || shownWatch.IsRunning)) { heroClock.Start(); lastTick = HeroNow; }
            if (!run && heroClock.IsRunning) heroClock.Stop();
            heroTimer.Interval = run ? TickMs : 100;   // paused: only the controller and the window fade are looked at
        }

        void HeroSheetChanged()
        {
            if (!hero || scene == null) return;
            if (SheetOpen) HeroTakeSnapshot();
            else if (sheetSnapshot != null) { sheetSnapshot.Dispose(); sheetSnapshot = null; overlayDirty = true; Invalidate(); }
            HeroPauseCheck();
        }

        /// <summary>The still, dimmed picture the sheets sit on (their glass reads it; the orb does not move behind them).</summary>
        void HeroTakeSnapshot()
        {
            if (sheetSnapshot != null) sheetSnapshot.Dispose();
            sheetSnapshot = new Bitmap(Math.Max(1, ClientSize.Width), Math.Max(1, ClientSize.Height), PixelFormat.Format32bppPArgb);
            using (var g = Graphics.FromImage(sheetSnapshot))
            {
                HeroCompose(g, new Rectangle(Point.Empty, ClientSize), false);
                using (var b = new SolidBrush(Color.FromArgb(170, 0, 0, 0))) g.FillRectangle(b, 0, 0, sheetSnapshot.Width, sheetSnapshot.Height);
            }
        }

        // ---- Painting ----

        void HeroPaint(Graphics g, Rectangle clip)
        {
            if (sheetSnapshot != null) { g.DrawImage(sheetSnapshot, clip, clip, GraphicsUnit.Pixel); heroPainted = true; return; }
            try
            {
                if (HeroAssets.TestFail == "paint" && heroStarted && heroClock.Elapsed.TotalSeconds > 0.5) throw new InvalidOperationException("test: the renderer failed");
                HeroCompose(g, clip, true);
                heroPainted = true;
            }
            catch (Exception ex) { HeroBroke(ex); HeroPaintPlain(g); }
        }

        void HeroCompose(Graphics g, Rectangle clip, bool live)
        {
            if (heroBroken) { HeroPaintPlain(g); return; }
            g.CompositingMode = CompositingMode.SourceCopy;
            using (var b = new SolidBrush(Color.Black)) g.FillRectangle(b, clip);
            g.CompositingMode = CompositingMode.SourceOver;
            if (orb.ShownRect.IntersectsWith(clip))
            {
                var st = g.Save();
                g.IntersectClip(content);   // within the paint's own clip: a small repaint draws only its part of the orb
                orb.Draw(g);
                g.Restore(st);
            }
            foreach (var r in EnsureOverlay())
                if (!clip.Contains(r)) Invalidate(r);   // a part changed outside this paint: it comes next
            g.CompositingMode = CompositingMode.SourceOver;
            foreach (var part in OverlayParts())
            {
                var r = Rectangle.Intersect(part, clip);
                if (r.Width > 0 && r.Height > 0) g.DrawImage(overlay, r, r, GraphicsUnit.Pixel);   // the overlay is clear elsewhere
            }
        }

        Rectangle[] OverlayParts() { return HeroScene.Parts(content, heroK); }

        /// <summary>Brings the overlay up to date: redraws only the parts whose state changed (all of it when new or
        /// when overlayDirty says so) and returns them in window pixels.</summary>
        System.Collections.Generic.List<Rectangle> EnsureOverlay()
        {
            var size = ClientSize;
            if (overlay == null || overlay.Size != size)
            {
                if (overlay != null) overlay.Dispose();
                overlay = new Bitmap(Math.Max(1, size.Width), Math.Max(1, size.Height), PixelFormat.Format32bppPArgb);
                overlayDirty = true;
            }
            if (overlayKeys == null || overlayKeys.Length != HeroScene.Regions.Length) { overlayKeys = new string[HeroScene.Regions.Length]; overlayDirty = true; }
            var changed = scene.UpdateOverlay(overlay, overlayKeys, overlayDirty, content, heroK);
            overlayDirty = false;
            return changed;
        }

        /// <summary>The scene's state changed (a hover, the highlight, a tile's text): repaints only the parts that changed.</summary>
        void HeroOverlayChanged()
        {
            if (!IsHandleCreated || overlay == null || heroBroken || sheetSnapshot != null) { if (IsHandleCreated) Invalidate(content); return; }
            foreach (var r in EnsureOverlay()) Invalidate(r);
        }

        /// <summary>The plain fallback: the same tiles as simple shapes and text, no orb, no effects.</summary>
        void HeroPaintPlain(Graphics g)
        {
            g.ResetClip();
            g.Clear(Color.Black);
            var st = g.Save();
            g.TranslateTransform(content.X, content.Y);
            g.ScaleTransform(heroK, heroK);
            g.SmoothingMode = SmoothingMode.AntiAlias;
            using (var big = new Font(FontFamily.GenericSansSerif, 40, FontStyle.Bold, GraphicsUnit.Pixel))
            using (var f = new Font(FontFamily.GenericSansSerif, 22, FontStyle.Bold, GraphicsUnit.Pixel))
            using (var small = new Font(FontFamily.GenericSansSerif, 13, FontStyle.Regular, GraphicsUnit.Pixel))
            using (var green = new SolidBrush(HeroScene.Neon))
            using (var white = new SolidBrush(Color.White))
            using (var grey = new SolidBrush(HeroScene.Grey))
            {
                g.DrawString("HERO VR", big, green, 64, 40);
                g.DrawString("The launcher is in its plain mode (details in launcher-error.log); everything works as usual.", small, grey, 64, 100);
                var tiles = scene != null ? scene.Tiles : new HeroScene().Tiles;
                int sel = scene != null ? scene.Target : 0;
                for (int i = 0; i < tiles.Length; i++)
                {
                    var poly = HeroScene.TilePoly(i, i == sel ? 1 : 0);
                    using (var p = new Pen(i == sel ? HeroScene.Neon : Color.FromArgb(90, 104, 236, 66), i == sel ? 2f : 1f)) g.DrawPolygon(p, poly);
                    g.DrawString(tiles[i].Title, f, tiles[i].Enabled ? white : grey, poly[0].X + 40, poly[0].Y + 20);
                    g.DrawString(tiles[i].Sub, small, grey, poly[0].X + 40, poly[0].Y + 52);
                }
                g.DrawString("VERSION " + (scene != null ? scene.Version : LauncherInfo.Version) + "   \u00B7   MADE BY AGENT MULDER", small, grey, 64, 660);
                g.DrawString("\u2013", f, grey, HeroScene.MinRect.X + 8, HeroScene.MinRect.Y);
                g.DrawString("\u00D7", f, grey, HeroScene.CloseRect.X + 8, HeroScene.CloseRect.Y);
            }
            g.Restore(st);
        }

        /// <summary>The renderer failed: plain mode for this run, and the classic design from the next start (said once).</summary>
        void HeroBroke(Exception ex)
        {
            if (heroBroken) return;
            heroBroken = true;
            Program.LogError(ex);
            LauncherMode.FallBack(settings, "The new launcher design could not draw its screen (" + ex.Message + "; details in launcher-error.log), so the classic launcher opened instead.");
            LauncherMode.ClearMarker();
            try { if (orb != null) orb.Dispose(); } catch { }
            HeroPauseCheck();
            BeginInvoke(new Action(() => Invalidate()));
        }

        // ---- The clock ----

        void HeroTick()
        {
            try
            {
                if (Opacity < 1 && !IsDisposed)
                {
                    double t0 = shownWatch.Elapsed.TotalSeconds;
                    Opacity = Math.Min(1.0, t0 / 0.6);
                }
                HeroPad();
                if (HeroAssets.TestFail == "crash" && heroClock.Elapsed.TotalSeconds > 0.8) throw new InvalidOperationException("test: the new design failed while running");
                if (heroBroken) return;
                if (orb.Failure != null) throw new InvalidOperationException("the orb could not be decoded: " + orb.Failure.Message, orb.Failure);
                if (!heroStartedOk && heroPainted && shownWatch.Elapsed.TotalSeconds > 1.0)
                {
                    // The start finished: the first frames are on screen and the sound has started (or is off).
                    // Checked even while paused (a sheet open, minimised, a game running) so the marker never lingers.
                    heroStartedOk = true;
                    LauncherMode.ClearMarker();
                    LauncherMode.Log("started (sound " + (audio != null ? "on" : "off") + ")");
                }
                if (!heroClock.IsRunning) return;
                heroStarted = true;
                double now = HeroNow, dt = Math.Max(0, Math.Min(0.1, now - lastTick));
                lastTick = now;
                if (scene.Animating)
                {
                    scene.T = Math.Min(now, HeroScene.IntroEnd);
                    scene.Step(dt);
                }
                // Only what changed is repainted: the overlay's changed parts and the orb's old and new rectangles.
                HeroOverlayChanged();
                long fi = orb.StepAt(now);
                if (fi != orb.ShownStep)
                {
                    var before = orb.ShownRect;
                    if (orb.Show(fi)) Invalidate(Rectangle.Union(before, orb.ShownRect));
                }
            }
            catch (Exception ex) { HeroBroke(ex); }
        }
        bool heroStartedOk;

        // ---- Input: mouse, keys, controller ----

        PointF ToDesign(Point p) { return new PointF((p.X - content.X) / heroK, (p.Y - content.Y) / heroK); }

        void HeroMouseMove(MouseEventArgs e)
        {
            if (!hero || scene == null) return;
            if (dragging) { var s = Cursor.Position; Location = new Point(dragWindow.X + s.X - dragFrom.X, dragWindow.Y + s.Y - dragFrom.Y); return; }
            if (SheetOpen) { Cursor = Cursors.Default; return; }
            var d = ToDesign(e.Location);
            bool ch = HeroScene.CloseRect.Contains(d), mh = HeroScene.MinRect.Contains(d);
            bool sh = scene.Status != null && HeroScene.StatusRect.Contains(d);
            int t = heroBroken || scene.T < 3.0 ? -1 : scene.HitTile(d);
            if (ch != scene.CloseHot || mh != scene.MinHot || sh != scene.StatusHot) { scene.CloseHot = ch; scene.MinHot = mh; scene.StatusHot = sh; HeroOverlayChanged(); }
            if (t >= 0 && t != scene.Target) HeroSelect(t);
            Cursor = t >= 0 || ch || mh || sh ? Cursors.Hand : Cursors.Default;
        }

        void HeroMouseDown(MouseEventArgs e)
        {
            if (!hero || scene == null || e.Button != MouseButtons.Left) return;
            if (SheetOpen) { tabs.Select(-1); return; }   // a click beside an open sheet closes it
            var d = ToDesign(e.Location);
            if (HeroScene.CloseRect.Contains(d) || HeroScene.MinRect.Contains(d) || (scene.Status != null && HeroScene.StatusRect.Contains(d))) return;
            int t = heroBroken || scene.T < 3.0 ? -1 : scene.HitTile(d);
            if (t >= 0) { scene.Pressed = t; HeroSelect(t); HeroOverlayChanged(); return; }
            dragging = true; dragFrom = Cursor.Position; dragWindow = Location;
        }

        void HeroMouseUp(MouseEventArgs e)
        {
            if (!hero || scene == null) return;
            if (dragging) { dragging = false; return; }
            var d = ToDesign(e.Location);
            if (HeroScene.CloseRect.Contains(d)) { Close(); return; }
            if (HeroScene.MinRect.Contains(d)) { WindowState = FormWindowState.Minimized; return; }
            if (scene.Status != null && HeroScene.StatusRect.Contains(d)) { tabs.Select(SheetStatus); return; }
            int pressed = scene.Pressed;
            scene.Pressed = -1; HeroOverlayChanged();
            if (pressed >= 0 && pressed == scene.HitTile(d)) HeroActivate(pressed);
        }

        void HeroSelect(int i)
        {
            if (i < 0 || i >= scene.Tiles.Length || i == scene.Target) return;
            scene.Target = i;
            if (heroBroken || !heroClock.IsRunning) scene.Sel = i;
            HeroOverlayChanged();
        }

        void HeroActivate(int i)
        {
            switch (i)
            {
                case 0: if (job == Job.Vr) StopVr(); else PlayVr(); break;
                case 1: PlayFlat(); break;
                case 2: tabs.Select(SheetUpdates); break;
                case 3: tabs.Select(SheetOptions); break;
                case 4: Revert(); break;
            }
            HeroSync();
        }

        bool HeroKey(Keys k)
        {
            if (!hero || scene == null) return false;
            if (k == Keys.Escape)
            {
                if (SheetOpen) tabs.Select(-1); else Close();
                return true;
            }
            if (SheetOpen) return false;
            switch (k)
            {
                case Keys.Up: HeroSelect(Math.Max(0, scene.Target - 1)); return true;
                case Keys.Down: HeroSelect(Math.Min(scene.Tiles.Length - 1, scene.Target + 1)); return true;
                case Keys.Enter: case Keys.Space: HeroActivate(scene.Target); return true;
            }
            return false;
        }

        void HeroPad()
        {
            if (ActiveForm != this) { padPrev = 0; return; }
            int b = Pad.Buttons();
            int pressed = b & ~padPrev;
            padPrev = b;
            if (pressed == 0) return;
            if ((pressed & Pad.B) != 0) { if (SheetOpen) tabs.Select(-1); return; }
            if (SheetOpen) return;
            if ((pressed & Pad.Up) != 0) HeroSelect(Math.Max(0, scene.Target - 1));
            if ((pressed & Pad.Down) != 0) HeroSelect(Math.Min(scene.Tiles.Length - 1, scene.Target + 1));
            if ((pressed & Pad.A) != 0) HeroActivate(scene.Target);
        }

        // ---- The tiles follow the launcher's state ----

        void HeroSync()
        {
            if (!hero || scene == null) return;
            bool running = false;
            try { running = !preview && SystemInfo.GameRunning(); } catch { }
            var t = scene.Tiles;
            if (job == Job.Vr) { t[0].Title = stateTitle.Text == "STOPPING..." ? "STOPPING VR" : "STOP VR"; t[0].Sub = "END THE VR SESSION"; t[0].Icon = "stop"; t[0].Enabled = true; }
            else { t[0].Title = "PLAY VR"; t[0].Sub = "ENTER THE GAME"; t[0].Icon = "helmet"; t[0].Enabled = job == Job.None; }
            t[1].Enabled = job == Job.None && !running;
            t[1].Sub = running ? "GAME IS RUNNING" : "PLAY ON MONITOR";
            bool newer = false;
            try { newer = latest != null && IsNewer(latest); } catch { }
            t[2].Sub = job == Job.Update ? "INSTALLING..." : newer ? "VERSION " + ShortVersion(latest.Version).ToUpperInvariant() + " AVAILABLE" : "CHECK FOR NEW VERSION";
            t[2].SubColor = newer || job == Job.Update ? HeroScene.Neon : Color.Empty;
            t[4].Enabled = job == Job.None;
            string st = stateTitle != null ? stateTitle.Text : null;
            if (string.IsNullOrEmpty(st) || st == "READY" || preview) scene.Status = null;
            else
            {
                scene.Status = st.TrimEnd(new[] { '.' }) + "  \u00B7  DETAILS";
                var c = stateTitle.ForeColor;
                scene.StatusColor = c == Theme.Bad ? Color.FromArgb(255, 92, 92) : c == Theme.Warn ? Color.FromArgb(240, 190, 80) : c == Theme.Good ? HeroScene.Neon : HeroScene.Grey;
            }
            try { scene.Version = ShortVersion(SystemInfo.PortVersion(root) ?? LauncherInfo.Version).ToUpperInvariant(); } catch { }
            bool nowGame = job == Job.Vr || running;
            if (nowGame && !gameOn) HeroGameStarting();
            else if (!nowGame && gameOn) { gameOn = false; HeroPauseCheck(); }
            HeroOverlayChanged();
        }

        // ---- Options > Launcher (both designs) ----

        void BuildLauncherSection(Control area, ref int y, int colW, int gap)
        {
            var lh = new TextLabel(GlassPanel.Spaced("Launcher"), Theme.UiBold(11), Theme.Red) { Wrap = false };
            lh.Bounds = Theme.R(0, y + 8, 1040, 20);
            area.Controls.Add(lh);
            y += 34;
            designLoading = true;
            designToggle = new ToggleRow("New launcher design", "Opens the HERO VR design with its orb and sound, which can feel slower on some PCs; off opens the classic launcher, the default (restarts the launcher).",
                LauncherMode.IsHero(settings));
            designToggle.Bounds = Theme.R(0, y, colW, 70);
            designToggle.Changed += (a, e) => DesignToggled(designToggle.Checked);
            area.Controls.Add(designToggle);
            tip.SetToolTip(designToggle, "Both designs have every feature. Hold Shift while opening the launcher (or start it with --classic) to open the classic one once.");
            if (hero)
            {
                soundToggle = new ToggleRow("Launcher sound", "Plays the intro and the ambient soundscape while the launcher is open; it always stops when a game starts.",
                    settings.GetBool(LauncherMode.KeySound, true));
                soundToggle.Bounds = Theme.R(colW + gap, y, colW, 70);
                soundToggle.Changed += (a, e) =>
                {
                    settings.SetBool(LauncherMode.KeySound, soundToggle.Checked); settings.Save();
                    if (!soundToggle.Checked) HeroStopSound(300);
                    else if (!soundOffForGame && !preview) HeroStartSound(true);
                };
                area.Controls.Add(soundToggle);
                tip.SetToolTip(soundToggle, "Takes effect at once. After a game has started the sound stays off until the launcher is opened again.");
                y += 76;
                var sw = new AccentButton("Switch to classic launcher", ButtonKind.Secondary) { Glyph = "revert" };
                sw.Bounds = Theme.R(0, y + 4, 280, 38);
                sw.Click += (a, e) => SwitchToClassic();
                area.Controls.Add(sw);
                var swSub = new TextLabel("Restarts the launcher in the original design; the toggle above brings this one back.", Theme.Ui(11.5f), Theme.Sub) { Wrap = false };
                swSub.Bounds = Theme.R(296, y + 14, 740, 20);
                area.Controls.Add(swSub);
                y += 56;
                // The classic bar's other sections, which the new design's five tiles do not show.
                int x = 0;
                foreach (var link in new[] { new { T = "Status", G = "status", S = SheetStatus }, new { T = "Reports & log", G = "reports", S = SheetReports }, new { T = "Credits", G = "credits", S = SheetCredits }, new { T = "Multiplayer", G = "play", S = SheetMultiplayer } })
                {
                    var b = new AccentButton(link.T, ButtonKind.Secondary) { Glyph = link.G };
                    int bw = link.T.Length > 8 ? 190 : 150;
                    b.Bounds = Theme.R(x, y + 4, bw, 38);
                    int sheet = link.S;
                    b.Click += (a, e) => tabs.Select(sheet);
                    area.Controls.Add(b);
                    x += bw + 12;
                }
                var moreSub = new TextLabel("Checks, reports, credits, and playing with a friend.", Theme.Ui(11.5f), Theme.Sub) { Wrap = false };
                moreSub.Bounds = Theme.R(x + 4, y + 14, 1040 - x - 4, 20);
                area.Controls.Add(moreSub);
                y += 56;
            }
            else y += 76;
            launcherArea = area as ScrollArea;
            designLoading = false;
        }
        ScrollArea launcherArea;

        /// <summary>Preview only (SM2VR_LAUNCHER_DIALOG): shows the design dialogs and the setting without changing anything.</summary>
        void PreviewDesignDialog(string kind)
        {
            if (kind == "switch")
            {
                tabs.Select(SheetOptions);
                if (launcherArea != null && designToggle != null) launcherArea.AutoScrollPosition = new Point(0, Math.Max(0, designToggle.Top - launcherArea.AutoScrollPosition.Y - Theme.S(60)));
                Backdrop.InvalidateAll(pages[SheetOptions]);
                return;
            }
            if (kind == "offer")
                NoticeDialog.Show(this, "HERO VR launcher redesign",
                    "New launcher design available. Would you like to restart now with the new design? You can safely switch back to the original anytime if you prefer it or run into issues.",
                    "Restart with new design", "Not now");
            else
                NoticeDialog.Show(this, "The classic launcher opened instead",
                    "The new launcher design did not finish starting last time (it closed or stopped responding), so the classic launcher opened instead." +
                    "\n\nEverything works the same here. You can turn the new design back on in Options > Launcher whenever you like.", "OK", null);
        }

        void DesignToggled(bool on)
        {
            if (designLoading) return;
            settings.Set(LauncherMode.KeyDesign, on ? LauncherMode.Hero : LauncherMode.Classic);
            if (on) settings.Set(LauncherMode.KeyFallbackReason, null);
            settings.Save();
            LauncherMode.Log("design set to " + (on ? "new" : "classic") + " in Options");
            if (on == hero && !LauncherMode.Forced) return;   // back to the design that is open: nothing to restart
            string which = on ? "the new design" : "the classic launcher";
            if (job != Job.None)
            {
                NoticeDialog.Show(this, "Saved", "The launcher opens with " + which + " the next time it starts (it can't restart while " + job.ToString().ToLower() + " is running).", "OK", null);
                return;
            }
            if (NoticeDialog.Show(this, on ? "Restart with the new design?" : "Switch to the classic launcher?",
                "The launcher restarts to open " + which + ". Nothing else changes: your settings, mod and game are not touched. Choose Later to switch the next time it opens.",
                on ? "Restart with new design" : "Restart in classic", "Later") == DialogResult.OK)
                LauncherMode.Restart(this, null);
        }

        void SwitchToClassic()
        {
            if (!GuardIdle()) return;
            if (NoticeDialog.Show(this, "Switch to the classic launcher?",
                "The launcher restarts in the original design. You can come back any time with Options > Launcher > New launcher design.",
                "Restart in classic", "Cancel") != DialogResult.OK) return;
            designLoading = true; designToggle.Checked = false; designLoading = false;
            settings.Set(LauncherMode.KeyDesign, LauncherMode.Classic); settings.Save();
            LauncherMode.Log("switched to classic from the new design's Options");
            LauncherMode.Restart(this, null);
        }

        /// <summary>The classic design's one-time notice: why the new design was turned off (0.0.73 test 3: no new-design offer).</summary>
        void DesignStartupNotices()
        {
            if (hero) return;
            string reason = LauncherMode.Notice;
            if (!string.IsNullOrEmpty(reason))
            {
                LauncherMode.Log("telling the player why the classic launcher opened");
                NoticeDialog.Show(this, "The classic launcher opened instead",
                    reason + "\n\nEverything works the same here. You can turn the new design back on in Options > Launcher whenever you like.", "OK", null);
                LauncherMode.NoticeShown(settings);
                return;
            }
            // 0.0.73 test 3: no "new launcher design available" question any more. Players found the new design slow, so
            // classic is the default and the new design is only the opt-in switch in Options > Launcher.
        }

        void HeroClosed()
        {
            heroTimer.Stop();
            LauncherMode.ClearMarker();
            if (audio != null) { try { audio.Close(250, 450); } catch { } audio = null; }
            if (orb != null) { try { orb.Dispose(); } catch { } }
        }
    }

    /// <summary>A short question or notice in the launcher's look (both designs).</summary>
    class NoticeDialog : Form
    {
        public static DialogResult Show(IWin32Window owner, string heading, string text, string primary, string secondary)
        {
            using (var d = new NoticeDialog(heading, text, primary, secondary)) return owner == null ? d.ShowDialog() : d.ShowDialog(owner);
        }

        NoticeDialog(string heading, string text, string primaryText, string secondaryText)
        {
            Text = LauncherInfo.Title + " - " + heading;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = MinimizeBox = false; ShowInTaskbar = false;
            StartPosition = FormStartPosition.CenterParent;
            BackColor = Theme.DialogBg; ForeColor = Theme.Text; Font = Theme.NUi(12.5f);
            AutoScaleMode = AutoScaleMode.None;
            int w = 560;
            var body = new Label { Text = text, ForeColor = Theme.Silver, AutoSize = false, Font = Theme.NUi(12.5f) };
            Size need;
            using (var g = CreateGraphics()) need = Size.Ceiling(g.MeasureString(text, body.Font, Theme.S(w - 48)));
            int bodyH = Math.Max(Theme.S(44), need.Height + Theme.S(8));
            Controls.Add(new Label { Text = heading, Font = Theme.Native("ui", 17, FontStyle.Bold), ForeColor = Theme.HeroStyle ? Theme.Neon : Theme.Text, AutoSize = false, Bounds = Theme.R(24, 18, w - 48, 32) });
            body.Bounds = new Rectangle(Theme.S(24), Theme.S(58), Theme.S(w - 48), bodyH);
            Controls.Add(body);
            int by = Theme.S(58) + bodyH + Theme.S(16);
            ClientSize = new Size(Theme.S(w), by + Theme.S(38 + 22));
            var ok = new Button { DialogResult = DialogResult.OK };
            Style(ok, primaryText, true);
            int okW = Math.Max(Theme.S(120), TextRenderer.MeasureText(primaryText.ToUpperInvariant(), ok.Font).Width + Theme.S(36));
            ok.Bounds = new Rectangle(Theme.S(24), by, okW, Theme.S(38));
            Controls.Add(ok);
            AcceptButton = ok;
            if (secondaryText != null)
            {
                var no = new Button { DialogResult = DialogResult.Cancel };
                Style(no, secondaryText, false);
                int noW = Math.Max(Theme.S(110), TextRenderer.MeasureText(secondaryText.ToUpperInvariant(), no.Font).Width + Theme.S(36));
                no.Bounds = new Rectangle(ok.Right + Theme.S(10), by, noW, Theme.S(38));
                Controls.Add(no);
                CancelButton = no;
            }
            else CancelButton = ok;
            Shown += (s, e) => ok.Focus();
        }

        static void Style(Button b, string text, bool main)
        {
            b.Text = text.ToUpperInvariant(); b.FlatStyle = FlatStyle.Flat; b.Font = Theme.NUiBold(12);
            b.BackColor = main ? Theme.Red : (Theme.HeroStyle ? Color.FromArgb(14, 20, 14) : Color.FromArgb(34, 36, 46));
            b.ForeColor = Theme.HeroStyle && main ? Color.Black : Color.White;
            b.FlatAppearance.BorderColor = main ? Theme.RedHot : Theme.PanelBorder;
            b.Cursor = Cursors.Hand;
        }
    }
}
