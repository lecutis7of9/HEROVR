// HERO VR launcher, new design: checks of the pieces that do not need Windows (compiled together with the
// launcher's sources by tests/launcher_hero_tests.py; -main:Sm2VrLauncher.HeroSelfTest). Prints PASS/FAIL lines.
using System;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Threading;

namespace Sm2VrLauncher
{
    static class HeroSelfTest
    {
        static int failed;
        static void Check(bool ok, string what) { Console.WriteLine((ok ? "PASS " : "FAIL ") + what); if (!ok) failed++; }
        static void Throws<T>(Action a, string what) where T : Exception
        {
            try { a(); Check(false, what + " (no exception)"); }
            catch (T) { Check(true, what); }
            catch (Exception ex) { Check(false, what + " (" + ex.GetType().Name + ": " + ex.Message + ")"); }
        }

        static int Main(string[] args)
        {
            foreach (var culture in new[] { "en-US", "de-DE", "tr-TR", "ar-SA", "ja-JP" })
            {
                // Non-English Windows: the same results whatever the culture (Turkish dotless i, comma decimals).
                Thread.CurrentThread.CurrentCulture = Thread.CurrentThread.CurrentUICulture = new CultureInfo(culture);
                Console.WriteLine("-- culture " + culture);
                Assets();
                Overlay();
                Modes();
                Sizes();
                Audio();
            }
            Console.WriteLine(failed == 0 ? "ALL PASS" : failed + " FAILED");
            return failed == 0 ? 0 : 1;
        }

        /// <summary>Mean absolute difference of two same-size pictures' green channel (0..255).</summary>
        static double Diff(Bitmap a, Bitmap b)
        {
            if (a.Width != b.Width || a.Height != b.Height) return 255;
            double sum = 0; int n = 0;
            for (int y = 0; y < a.Height; y += 3) for (int x = 0; x < a.Width; x += 3) { sum += Math.Abs(a.GetPixel(x, y).G - b.GetPixel(x, y).G); n++; }
            return sum / n;
        }

        static void Assets()
        {
            Environment.SetEnvironmentVariable("SM2VR_LAUNCHER_TEST_HERO_FAIL", null);
            try { HeroAssets.Load(); Check(true, "assets load (orb, intro, loop, Barlow)"); }
            catch (Exception ex) { Check(false, "assets load: " + ex.Message); return; }
            var o = HeroAssets.Orb;
            // launcher-orb-fix: the idle loop is a seamless 6 s "boil" at 30 fps; the orb's turn is done as it plays, at the
            // concept video's own speed (its core turns ~6.5 deg/s), so it never wraps and never turns back.
            Check(o.Fps == 24 && o.IntroCount == 140 && o.LoopFps == 30 && o.LoopCount == 180 && o.Frames.Length == 320, "orb: 140 intro frames at 24 fps + 180 loop frames at 30 fps (6 s)");
            Check(Math.Abs(o.Spin - 6.5) < 0.01 && Math.Abs(o.SpinStart - 3.25) < 0.01 && Math.Abs(o.CentreY - 357) < 0.01 && o.DiscIn > 200 && o.DiscOut > o.DiscIn && o.DiscOut < 240,
                "orb: turns 6.5 deg/s (the concept video's speed), the loop starting where the intro's spin-up ends");
            using (var p = new OrbPlayer(o))
            {
                double intro = 140 / 24.0;
                Check(p.IndexAt(0) == 0 && p.IndexAt(intro - 0.001) == 139 && p.IndexAt(intro) == 140 && p.IndexAt(intro + 1.0 / 30) == 141,
                    "orb: the intro plays at 24 fps and the loop starts straight after it at 30 fps");
                Check(p.IndexAt(intro + 6 - 0.001) == 319 && p.IndexAt(intro + 6) == 140 && p.IndexAt(intro + 120.5) == 140 + 15 && p.FrameOf(p.Next(319)) == 140,
                    "orb: the loop's last frame is followed by its first, forever");
                long s0 = p.StepAt(intro), s1 = p.StepAt(intro + 6), s2 = p.StepAt(intro + 600);
                Check(p.AngleOf(139) == 0 && Math.Abs(p.AngleOf(s0) - 3.25) < 0.01 && s1 == s0 + 180 && Math.Abs(p.AngleOf(s1) - (3.25 + 39)) < 0.01
                    && Math.Abs(p.AngleOf(s0 + 181) - p.AngleOf(s0 + 180) - 6.5 / 30) < 0.001 && p.AngleOf(s2) >= 0 && p.AngleOf(s2) < 360,
                    "orb: the turn keeps going across the loop's wrap (6.5 deg/s, no step back) and stays within 0..360");
                using (var a0 = p.Render(s0 + 179)) using (var a1 = p.Render(s0 + 180)) using (var a2 = p.Render(s0 + 181))
                    Check(Diff(a0, a1) < 2 * Diff(a1, a2) + 0.5, "orb: frames across the loop's wrap change no more than the ones after it");
            }
            {
                var full = HeroAssets.Resource("hero.orb.bin");
                var v2 = new byte[full.Length - 20];   // version 2: the header without the turn
                Array.Copy(full, v2, 32); Array.Copy(full, 52, v2, 32, full.Length - 52); BitConverter.GetBytes(2).CopyTo(v2, 4);
                var o2 = OrbData.Parse(v2);
                Check(o2.LoopFps == 30 && o2.Spin == 0 && o2.LoopCount == 180, "orb.bin version 2 (no turn) still reads: it plays without turning");
                var v1 = (byte[])v2.Clone();
                BitConverter.GetBytes(1).CopyTo(v1, 4); BitConverter.GetBytes(0).CopyTo(v1, 28);
                var o1 = OrbData.Parse(v1);
                Check(o1.LoopFps == o1.Fps, "orb.bin version 1 (no loop frame rate) still reads: its loop plays at its fps");
                var v4 = (byte[])full.Clone(); BitConverter.GetBytes(4).CopyTo(v4, 4);
                Throws<Exception>(() => OrbData.Parse(v4), "orb.bin version 4 is refused");
                var bad = (byte[])v2.Clone(); BitConverter.GetBytes(0).CopyTo(bad, 28);
                Throws<Exception>(() => OrbData.Parse(bad), "orb.bin version 2 with no loop frame rate is refused");
                var fast = (byte[])full.Clone(); BitConverter.GetBytes(500000).CopyTo(fast, 32);
                Throws<Exception>(() => OrbData.Parse(fast), "orb.bin with an impossible turn (500 deg/s) is refused");
                var off = (byte[])full.Clone(); BitConverter.GetBytes(900000).CopyTo(off, 40);
                Throws<Exception>(() => OrbData.Parse(off), "orb.bin with its turn centre off the canvas is refused");
            }
            Check(o.Width == 1280 && o.Height == 720, "orb canvas 1280x720");
            bool all = true;
            for (int i = 0; i < o.Frames.Length; i += 11) using (var b = OrbData.Decode(o.Frames[i])) { var f = o.Frames[i]; bool ok = b.Width <= f.W && b.Height <= f.H && b.Width * 2 >= f.W && Math.Abs((double)b.Width / b.Height - (double)f.W / f.H) < 0.02; if (!ok) Console.WriteLine("FAIL  frame " + i + ": " + b.Width + "x" + b.Height + " for " + f.W + "x" + f.H); all &= ok; }
            Check(all, "orb frames decode (stored at reduced size, same shape as their place on the canvas)");
            Check(HeroAssets.Intro.Rate == 48000 && HeroAssets.Loop.Rate == 48000 && HeroAssets.Intro.Channels == HeroAssets.Loop.Channels, "intro and loop: same format, 48 kHz");
            Check(Math.Abs(HeroAssets.Intro.Frames / 48000.0 - 5.0) < 0.05, "intro is 5.0 s");
            Check(HeroAssets.Loop.Frames / 48000.0 > 3.0, "ambient loop longer than 3 s");
            Check(HeroAssets.Regular != null && HeroAssets.Medium != null, "Barlow Regular and Medium");

            byte[] orb = HeroAssets.Resource("hero.orb.bin");
            Throws<Exception>(() => OrbData.Parse(new byte[0]), "empty orb data is refused");
            Throws<Exception>(() => { var b = (byte[])orb.Clone(); b[0] = (byte)'X'; OrbData.Parse(b); }, "orb data with a bad header is refused");
            Throws<Exception>(() => { var b = new byte[orb.Length / 2]; Array.Copy(orb, b, b.Length); OrbData.Parse(b); }, "truncated orb data is refused");
            Throws<Exception>(() => Pcm.ParseWav(new byte[] { 1, 2, 3 }), "a broken WAV is refused");
            Throws<Exception>(() => { var w = (byte[])HeroAssets.Resource("hero.loop.wav").Clone(); w[8] = (byte)'X'; Pcm.ParseWav(w); }, "a WAV that is not WAVE is refused");
            foreach (var kind in new[] { "orb", "audio", "fonts" })
            {
                Environment.SetEnvironmentVariable("SM2VR_LAUNCHER_TEST_HERO_FAIL", kind);
                Throws<HeroAssetException>(() => HeroAssets.Load(), "a damaged " + kind + " asset gives a plain HeroAssetException (classic opens)");
            }
            Environment.SetEnvironmentVariable("SM2VR_LAUNCHER_TEST_HERO_FAIL", null);
            HeroAssets.Load();
        }

        // launcher-orb-fix: the overlay is redrawn only where its state changed. Everything it draws must be inside
        // HeroScene.Regions, and a part-by-part updated overlay must look exactly like one drawn from scratch.
        static int[] Pixels(Bitmap b)
        {
            var d = b.LockBits(new Rectangle(0, 0, b.Width, b.Height), System.Drawing.Imaging.ImageLockMode.ReadOnly, System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
            var px = new int[b.Width * b.Height];
            System.Runtime.InteropServices.Marshal.Copy(d.Scan0, px, 0, px.Length);
            b.UnlockBits(d);
            return px;
        }

        static void Overlay()
        {
            Theme.ApplyHero(HeroAssets.Regular, HeroAssets.Medium);   // as Program does before the hero window opens
            foreach (var view in new[] { new { Size = new Size(1280, 720), Content = new Rectangle(0, 0, 1280, 720) }, new { Size = new Size(1690, 915), Content = new Rectangle(31, 0, 1627, 915) } })
            {
                float k = view.Content.Height / 720f;
                var scene = new HeroScene { T = 0 };
                var parts = HeroScene.Parts(view.Content, k);
                var keys = new string[HeroScene.Regions.Length];
                using (var inc = new Bitmap(view.Size.Width, view.Size.Height, System.Drawing.Imaging.PixelFormat.Format32bppPArgb))
                {
                    scene.UpdateOverlay(inc, keys, true, view.Content, k);
                    bool inside = true, same = true, idleMark = false; int steps = 0, redrawn = 0, idleRedraws = -1; string where = "";
                    // The intro, the idle picture, the highlight sliding, a press, hovers, a status line, a tile's text.
                    var script = new System.Collections.Generic.List<Action>();
                    for (int n = 0; n <= 156; n++) { double tt = n / 30.0; script.Add(() => scene.T = Math.Min(tt, HeroScene.IntroEnd)); }
                    script.Add(() => { idleMark = true; });
                    script.Add(() => { scene.Target = 3; });
                    for (int n = 0; n < 40; n++) script.Add(() => scene.Step(1 / 60.0));
                    script.Add(() => { scene.Pressed = 3; });
                    script.Add(() => { scene.Pressed = -1; scene.Target = 0; });
                    for (int n = 0; n < 40; n++) script.Add(() => scene.Step(1 / 60.0));
                    script.Add(() => { scene.CloseHot = true; });
                    script.Add(() => { scene.CloseHot = false; scene.MinHot = true; });
                    script.Add(() => { scene.Status = "UPDATE FAILED  \u00B7  DETAILS"; scene.StatusColor = Color.FromArgb(255, 92, 92); });
                    script.Add(() => { scene.StatusHot = true; });
                    script.Add(() => { scene.Tiles[2].Sub = "VERSION 0.0.7 AVAILABLE"; scene.Tiles[2].SubColor = HeroScene.Neon; });
                    script.Add(() => { scene.Tiles[0].Title = "STOP VR"; scene.Tiles[1].Enabled = false; });
                    script.Add(() => { scene.Version = "0.0.7"; });
                    script.Add(() => { scene.Target = 4; });
                    for (int n = 0; n < 40; n++) script.Add(() => scene.Step(1 / 60.0));
                    foreach (var act in script)
                    {
                        act();
                        steps++;
                        int c = scene.UpdateOverlay(inc, keys, false, view.Content, k).Count;
                        redrawn += c;
                        if (idleMark) { idleRedraws = c; idleMark = false; }   // nothing changed: nothing redrawn
                        using (var full = new Bitmap(view.Size.Width, view.Size.Height, System.Drawing.Imaging.PixelFormat.Format32bppPArgb))
                        using (var free = new Bitmap(view.Size.Width, view.Size.Height, System.Drawing.Imaging.PixelFormat.Format32bppPArgb))
                        {
                            scene.UpdateOverlay(full, new string[HeroScene.Regions.Length], true, view.Content, k);
                            // drawn from scratch with no part clips, everything lies inside the parts
                            using (var g = Graphics.FromImage(free)) { g.TranslateTransform(view.Content.X, view.Content.Y); g.ScaleTransform(k, k); g.SetClip(new RectangleF(0, 0, HeroScene.W, HeroScene.H)); scene.Draw(g); }
                            var a = Pixels(inc); var b = Pixels(full); var f = Pixels(free);
                            if (inside)
                                for (int i = 0; i < f.Length; i++)
                                {
                                    if (f[i] == 0) continue;
                                    int x = i % view.Size.Width, y = i / view.Size.Width; bool any = false;
                                    foreach (var r in parts) if (r.Contains(x, y)) { any = true; break; }
                                    if (!any) { where += " drawn outside the parts at " + x + "," + y + " (step " + steps + ")"; inside = false; break; }
                                }
                            if (same)
                                for (int i = 0; i < a.Length; i++)
                                    if (a[i] != b[i])
                                    {
                                        int d = 0;
                                        for (int sh = 0; sh < 32; sh += 8) d = Math.Max(d, Math.Abs(((a[i] >> sh) & 255) - ((b[i] >> sh) & 255)));
                                        if (d > 2) { where += " differs from a full redraw at " + (i % view.Size.Width) + "," + (i / view.Size.Width) + " (step " + steps + ")"; same = false; break; }
                                    }
                        }
                    }
                    string sz = view.Size.Width + "x" + view.Size.Height;
                    Check(inside, "overlay " + sz + ": everything it draws is inside its parts" + where);
                    Check(same, "overlay " + sz + ": redrawn part by part it is the same picture as drawn whole, through the intro, the highlight, presses, hovers and text changes" + where);
                    Check(idleRedraws == 0 && redrawn < steps * HeroScene.Regions.Length / 3, "overlay " + sz + ": an unchanged picture redraws nothing, and most steps redraw only a part or two (" + redrawn + " part redraws in " + steps + " steps)");
                }
            }
        }

        static Settings Fresh(string content)
        {
            Directory.CreateDirectory(Settings.Folder);
            if (content == null) { if (File.Exists(Settings.FilePath)) File.Delete(Settings.FilePath); }
            // past the 0.0.73 test 3 one-time move to classic (opt.migrated073classic), so the saved design under test stays
            else File.WriteAllText(Settings.FilePath, content + "\nopt.migrated073classic=1\n");
            LauncherMode.ClearMarker();
            return Settings.Load();
        }

        static void Modes()
        {
            var none = new string[0];
            Check(LauncherMode.Decide(Fresh(null), none, false) == Design.Classic, "no choice yet: classic (the new design is opt-in)");
            Check(LauncherMode.Decide(Fresh("LauncherDesign=hero\n"), none, false) == Design.Hero, "LauncherDesign=hero: new design");
            Directory.CreateDirectory(Settings.Folder); File.WriteAllText(Settings.FilePath, "LauncherDesign=hero\n"); LauncherMode.ClearMarker();
            Check(LauncherMode.Decide(Settings.Load(), none, false) == Design.Classic, "0.0.73 test 3: the new design before the one-time move opens classic");
            Check(LauncherMode.Decide(Fresh("LauncherDesign= HERO \n"), none, false) == Design.Hero, "case and spaces do not matter");
            Check(LauncherMode.Decide(Fresh("LauncherDesign=classic\n"), none, false) == Design.Classic, "LauncherDesign=classic: classic");
            foreach (var bad in new[] { "her0", "1", "hero\u0000", "\u00ff\u00fe", "h\u0131ro" })
            {
                var s = Fresh("LauncherDesign=" + bad + "\n");
                Check(LauncherMode.Decide(s, none, false) == Design.Classic && Settings.Load().Get("LauncherDesign", "") == "classic", "damaged value '" + bad.Replace("\u0000", "\\0") + "': classic, and repaired");
            }
            var rnd = new Random(7); var junk = new byte[4096]; rnd.NextBytes(junk);
            Directory.CreateDirectory(Settings.Folder); File.WriteAllBytes(Settings.FilePath, junk); LauncherMode.ClearMarker();
            Check(LauncherMode.Decide(Settings.Load(), none, false) == Design.Classic, "a corrupt settings file: classic");
            Check(LauncherMode.Decide(Fresh("LauncherDesign=hero\n"), new[] { "--classic" }, false) == Design.Classic && LauncherMode.Forced, "--classic forces classic");
            Check(LauncherMode.Decide(Fresh("LauncherDesign=hero\n"), none, true) == Design.Classic && LauncherMode.Forced, "Shift forces classic");
            Check(Settings.Load().Get("LauncherDesign", "") == "hero", "forcing classic keeps the saved choice");

            // A start that never finished (crash or hang before the first frames): the marker is still there.
            var h = Fresh("LauncherDesign=hero\n");
            LauncherMode.WriteMarker();
            Check(LauncherMode.Decide(h, none, false) == Design.Classic, "marker left by an unfinished start: classic");
            Check(LauncherMode.Notice.Length > 0 && LauncherMode.Notice.IndexOf("classic launcher", StringComparison.Ordinal) >= 0, "...with the reason to show once");
            var after = Settings.Load();
            Check(after.Get("LauncherDesign", "") == "classic" && !File.Exists(LauncherMode.MarkerPath), "...saved as classic, marker removed");
            LauncherMode.NoticeShown(after);
            Check(Settings.Load().Get("HeroFallbackReason", "") == "" && Settings.Load().Get("HeroOfferShown", "") == "1", "the reason is shown once (and the offer is not asked as well)");
            Check(LauncherMode.Decide(Settings.Load(), none, false) == Design.Classic && LauncherMode.Notice == "", "next start: classic, nothing shown again");

            var c = Fresh("LauncherDesign=hero\n");
            LauncherMode.FallBack(c, "test reason");
            var c2 = Settings.Load();
            Check(c2.Get("LauncherDesign", "") == "classic" && c2.Get("HeroFallbackReason", "") == "test reason", "a crash in the new design: classic next time with its reason");
            Fresh(null);
        }

        static void Sizes()
        {
            // Work areas in physical pixels (the launcher is per-monitor DPI aware), with taskbars, several monitors
            // (negative origins) and DPI 100-200% (the same physical size; the scale only changes the system fonts).
            var works = new[] {
                new Rectangle(0, 0, 1024, 728), new Rectangle(0, 0, 1280, 680), new Rectangle(0, 0, 1280, 720), new Rectangle(0, 0, 1366, 728),
                new Rectangle(0, 0, 1600, 860), new Rectangle(0, 0, 1920, 1040), new Rectangle(0, 0, 1920, 1080), new Rectangle(0, 0, 2560, 1400),
                new Rectangle(0, 0, 3440, 1400), new Rectangle(0, 0, 3840, 2100), new Rectangle(0, 0, 3840, 2160),
                new Rectangle(-1920, 0, 1920, 1040), new Rectangle(1920, -400, 1080, 1880), new Rectangle(3840, 0, 1280, 984) };
            foreach (var w in works)
            {
                var b = MainForm.HeroWindowBounds(w);
                string n = w.Width + "x" + w.Height + "@" + w.X + "," + w.Y;
                Check(w.Contains(b), n + ": the window is on that screen");
                Check(Math.Abs((b.X - w.X) - (w.Right - b.Right)) <= 1 && Math.Abs((b.Y - w.Y) - (w.Bottom - b.Bottom)) <= 1, n + ": centred");
                bool small = w.Height < 900 || w.Width < 1500;
                if (small) Check(b == w, n + ": a small screen gets all of its working area");
                else Check(Math.Abs(b.Width / (double)w.Width - 0.88) < 0.01 && Math.Abs(b.Height / (double)w.Height - 0.88) < 0.01, n + ": 88% of the screen");
                var c = MainForm.HeroContent(b.Size);
                Check(Math.Abs(c.Width / (double)c.Height - 16 / 9.0) < 0.01 && new Rectangle(Point.Empty, b.Size).Contains(c), n + ": 16:9 picture inside the window");
                float k = c.Height / 720f;
                Check(15.5f * k >= 11f, n + ": tile subtitles at least 11 px (" + (15.5f * k).ToString("0.0", CultureInfo.InvariantCulture) + ")");
                // The launcher is per-monitor DPI aware (app.manifest), so Windows reports physical pixels at every DPI;
                // the window first takes the DPI scale (MainForm's constructor), then the new design replaces it with
                // the scale that fits its sheets into the picture, the same at every DPI.
                float first = -1;
                foreach (int dpi in new[] { 100, 125, 150, 175, 200 })
                {
                    Theme.Scale = Math.Max(1f, dpi / 100f);
                    float s = MainForm.HeroSheetScale(c);
                    if (first < 0) first = s;
                    Theme.Scale = s;
                    Check(1108 * s <= c.Width && 556 * s <= c.Height, n + " at " + dpi + "%: the largest sheet fits (scale " + s.ToString("0.00", CultureInfo.InvariantCulture) + ")");
                    Check(Math.Abs(s - first) < 0.001f, n + " at " + dpi + "%: the same layout at every DPI");
                    Check(Theme.S(12) >= 10, n + " at " + dpi + "%: sheet body text at least 10 px (" + Theme.S(12) + ")");
                }
                Theme.Scale = 1f;
            }
        }

        static void Audio()
        {
            // The intro once, then the loop wrapped sample-exactly (no gap, no click at the seam), then a fade to silence.
            var intro = new Pcm { Rate = 1000, Channels = 1, Samples = new short[250] };
            var loop = new Pcm { Rate = 1000, Channels = 1, Samples = new short[70] };
            for (int i = 0; i < intro.Samples.Length; i++) intro.Samples[i] = (short)(1000 + i);
            for (int i = 0; i < loop.Samples.Length; i++) loop.Samples[i] = (short)(-2000 - i);
            var a = new WaveAudio(intro, loop, false);
            var t = typeof(WaveAudio);
            BindingFlags f = BindingFlags.NonPublic | BindingFlags.Instance;
            t.GetField("gain", f).SetValue(a, 1f); t.GetField("target", f).SetValue(a, 1f);
            var fill = t.GetMethod("Fill", f);
            var buf = new short[50]; bool ok = true; int n = 0;
            for (int blk = 0; blk < 40; blk++)
            {
                fill.Invoke(a, new object[] { buf });
                foreach (short s in buf)
                {
                    int want = n < 250 ? 1000 + n : -2000 - ((n - 250) % 70);
                    if (s != (short)(want * WaveAudio.Volume)) ok = false;
                    n++;
                }
            }
            Check(ok, "audio: intro once then the ambient loop, sample-exact at every seam");
            var b = new WaveAudio(intro, loop, true);
            t.GetField("gain", f).SetValue(b, 1f); t.GetField("target", f).SetValue(b, 1f);
            fill.Invoke(b, new object[] { buf });
            float vol = WaveAudio.Volume; Check(buf[0] == (short)(-2000 * vol), "audio: sound turned back on starts in the loop, not the intro (got " + buf[0] + ")");
            t.GetField("step", f).SetValue(b, 1f / 200); t.GetField("target", f).SetValue(b, 0f);
            bool silent = false; for (int i = 0; i < 10 && !silent; i++) silent = (bool)fill.Invoke(b, new object[] { buf });
            Check(silent, "audio: a fade out reaches silence (then the device is closed)");
        }
    }
}
