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
                Modes();
                Sizes();
                Audio();
            }
            Console.WriteLine(failed == 0 ? "ALL PASS" : failed + " FAILED");
            return failed == 0 ? 0 : 1;
        }

        static void Assets()
        {
            Environment.SetEnvironmentVariable("SM2VR_LAUNCHER_TEST_HERO_FAIL", null);
            try { HeroAssets.Load(); Check(true, "assets load (orb, intro, loop, Barlow)"); }
            catch (Exception ex) { Check(false, "assets load: " + ex.Message); return; }
            var o = HeroAssets.Orb;
            Check(o.Fps == 24 && o.IntroCount == 140 && o.LoopCount == 48 && o.Frames.Length == 188, "orb: 24 fps, 140 intro + 48 loop frames");
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

        static Settings Fresh(string content)
        {
            Directory.CreateDirectory(Settings.Folder);
            if (content == null) { if (File.Exists(Settings.FilePath)) File.Delete(Settings.FilePath); }
            else File.WriteAllText(Settings.FilePath, content);
            LauncherMode.ClearMarker();
            return Settings.Load();
        }

        static void Modes()
        {
            var none = new string[0];
            Check(LauncherMode.Decide(Fresh(null), none, false) == Design.Classic, "no choice yet: classic (the new design is opt-in)");
            Check(LauncherMode.Decide(Fresh("LauncherDesign=hero\n"), none, false) == Design.Hero, "LauncherDesign=hero: new design");
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
