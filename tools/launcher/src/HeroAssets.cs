// HERO VR launcher - the new design's assets, embedded in the exe: the orb animation (frames from the concept video,
// orb.bin), its intro and ambient loop sound (16-bit PCM WAV) and the Barlow font (SIL OFL). Each is checked as it
// loads; anything missing or damaged throws HeroAssetException and the launcher opens the classic design instead.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Text;
using System.IO;
using System.Text;

namespace Sm2VrLauncher
{
    class HeroAssetException : Exception
    {
        public HeroAssetException(string message) : base(message) { }
        public HeroAssetException(string message, Exception inner) : base(message, inner) { }
    }

    /// <summary>One orb frame: where it sits in the 1280x720 design (x, y, w, h) and its JPEG.</summary>
    class OrbFrame
    {
        public int X, Y, W, H;
        public float Cx, Scale;
        public byte[] Jpeg;
        public Rectangle Box { get { return new Rectangle(X, Y, W, H); } }
    }

    class OrbData
    {
        public int Fps, IntroCount, LoopCount, Width, Height;
        public OrbFrame[] Frames;

        /// <summary>orb.bin: "HORB", 7 int32 (version, fps, intro, loop, width, height, 0), then per frame
        /// x, y, w, h (int32), cx, scale (float32), length (int32) and the JPEG bytes.</summary>
        public static OrbData Parse(byte[] data)
        {
            if (data == null || data.Length < 40) throw new HeroAssetException("orb.bin is missing or too short");
            if (Encoding.ASCII.GetString(data, 0, 4) != "HORB") throw new HeroAssetException("orb.bin has the wrong header");
            int pos = 4;
            Func<int> I = () => { if (pos + 4 > data.Length) throw new HeroAssetException("orb.bin is cut short"); int v = BitConverter.ToInt32(data, pos); pos += 4; return v; };
            Func<float> F = () => { if (pos + 4 > data.Length) throw new HeroAssetException("orb.bin is cut short"); float v = BitConverter.ToSingle(data, pos); pos += 4; return v; };
            int version = I();
            var o = new OrbData { Fps = I(), IntroCount = I(), LoopCount = I(), Width = I(), Height = I() };
            I();
            if (version != 1) throw new HeroAssetException("orb.bin version " + version + " is not supported");
            if (o.Fps < 1 || o.Fps > 120 || o.IntroCount < 0 || o.LoopCount < 1 || o.IntroCount + o.LoopCount > 2000 || o.Width < 16 || o.Height < 16)
                throw new HeroAssetException("orb.bin has impossible sizes");
            o.Frames = new OrbFrame[o.IntroCount + o.LoopCount];
            for (int i = 0; i < o.Frames.Length; i++)
            {
                var f = new OrbFrame { X = I(), Y = I(), W = I(), H = I(), Cx = F(), Scale = F() };
                int len = I();
                if (f.W < 1 || f.H < 1 || f.W > 8 * o.Width || f.H > 8 * o.Height || len < 100 || pos + len > data.Length)
                    throw new HeroAssetException("orb.bin frame " + i + " is damaged");
                f.Jpeg = new byte[len];
                Buffer.BlockCopy(data, pos, f.Jpeg, 0, len);
                if (f.Jpeg[0] != 0xFF || f.Jpeg[1] != 0xD8) throw new HeroAssetException("orb.bin frame " + i + " is not a JPEG");
                pos += len;
                o.Frames[i] = f;
            }
            if (pos != data.Length) throw new HeroAssetException("orb.bin has " + (data.Length - pos) + " unexpected bytes at the end");
            return o;
        }

        /// <summary>Decodes a frame (used once at load for the first and a loop frame, so a bad JPEG is found early).</summary>
        public static Bitmap Decode(OrbFrame f)
        {
            using (var ms = new MemoryStream(f.Jpeg))
            using (var img = Image.FromStream(ms))
                return new Bitmap(img);
        }
    }

    /// <summary>16-bit PCM sound.</summary>
    class Pcm
    {
        public int Rate, Channels;
        public short[] Samples;   // interleaved
        public int Frames { get { return Samples.Length / Channels; } }

        public static Pcm ParseWav(byte[] d)
        {
            if (d == null || d.Length < 44) throw new HeroAssetException("sound file is missing or too short");
            if (Encoding.ASCII.GetString(d, 0, 4) != "RIFF" || Encoding.ASCII.GetString(d, 8, 4) != "WAVE") throw new HeroAssetException("sound file is not a WAV");
            int pos = 12; Pcm p = null; int bits = 0;
            while (pos + 8 <= d.Length)
            {
                string id = Encoding.ASCII.GetString(d, pos, 4);
                int len = BitConverter.ToInt32(d, pos + 4);
                if (len < 0 || pos + 8 + (long)len > d.Length) throw new HeroAssetException("sound file is cut short");
                if (id == "fmt ")
                {
                    if (len < 16 || BitConverter.ToInt16(d, pos + 8) != 1) throw new HeroAssetException("sound file is not PCM");
                    p = new Pcm { Channels = BitConverter.ToInt16(d, pos + 10), Rate = BitConverter.ToInt32(d, pos + 12) };
                    bits = BitConverter.ToInt16(d, pos + 22);
                }
                else if (id == "data")
                {
                    if (p == null || bits != 16 || p.Channels < 1 || p.Channels > 2 || p.Rate < 8000 || p.Rate > 192000) throw new HeroAssetException("sound file format is not supported");
                    if (len < p.Channels * 2 * 100) throw new HeroAssetException("sound file is empty");
                    p.Samples = new short[len / 2];
                    Buffer.BlockCopy(d, pos + 8, p.Samples, 0, p.Samples.Length * 2);
                    return p;
                }
                pos += 8 + len + (len & 1);
            }
            throw new HeroAssetException("sound file has no sound data");
        }
    }

    static class HeroAssets
    {
        public static OrbData Orb;
        public static Pcm Intro, Loop;
        static PrivateFontCollection fontCollection;
        public static FontFamily Regular, Medium;

        /// <summary>Test hook: SM2VR_LAUNCHER_TEST_HERO_FAIL=orb|audio|fonts|paint|hang breaks that part on purpose.</summary>
        public static string TestFail { get { return (Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_HERO_FAIL") ?? "").Trim().ToLowerInvariant(); } }

        public static byte[] Resource(string name)
        {
            using (var s = typeof(HeroAssets).Assembly.GetManifestResourceStream(name))
            {
                if (s == null)
                {
                    // Side-by-side files (a development build) work too.
                    string f = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, Path.Combine("hero", name.Replace("hero.", "")));
                    if (File.Exists(f)) return File.ReadAllBytes(f);
                    throw new HeroAssetException("the launcher is missing its " + name + " resource");
                }
                var buf = new byte[s.Length];
                int got = 0, n;
                while (got < buf.Length && (n = s.Read(buf, got, buf.Length - got)) > 0) got += n;
                if (got != buf.Length) throw new HeroAssetException(name + " could not be read");
                return buf;
            }
        }

        /// <summary>Loads and checks everything. Throws HeroAssetException with a plain reason.</summary>
        public static void Load()
        {
            string fail = TestFail;
            try
            {
                byte[] orb = Resource("hero.orb.bin");
                if (fail == "orb") { orb = (byte[])orb.Clone(); for (int i = 1000; i < orb.Length; i += 997) orb[i] ^= 0x5A; Array.Resize(ref orb, orb.Length - 77); }
                Orb = OrbData.Parse(orb);
                using (OrbData.Decode(Orb.Frames[0])) { }
                using (OrbData.Decode(Orb.Frames[Orb.Frames.Length - 1])) { }
            }
            catch (HeroAssetException) { throw; }
            catch (Exception ex) { throw new HeroAssetException("the orb animation could not be loaded (" + ex.Message + ")", ex); }
            try
            {
                byte[] a = Resource("hero.intro.wav"), b = Resource("hero.loop.wav");
                if (fail == "audio") a = new byte[] { 1, 2, 3 };
                Intro = Pcm.ParseWav(a);
                Loop = Pcm.ParseWav(b);
                if (Intro.Rate != Loop.Rate || Intro.Channels != Loop.Channels) throw new HeroAssetException("the intro and ambient sounds do not match");
            }
            catch (HeroAssetException ex) { throw new HeroAssetException("the launcher sound could not be loaded (" + ex.Message + ")", ex); }
            catch (Exception ex) { throw new HeroAssetException("the launcher sound could not be loaded (" + ex.Message + ")", ex); }
            try
            {
                if (fail == "fonts") throw new HeroAssetException("test: fonts");
                LoadFonts();
            }
            catch (Exception ex) { throw new HeroAssetException("the launcher font could not be loaded (" + ex.Message + ")", ex); }
        }

        static void ProbeWrite(string dir)
        {
            string p = Path.Combine(dir, ".write-test");
            File.WriteAllText(p, "ok");
            File.Delete(p);
        }

        static void LoadFonts()
        {
            // From files (written once to the settings folder): AddFontFile works the same on every Windows and on Mono.
            // The settings folder is the player's own (no admin needed); if it cannot be written, %TEMP% is used.
            string dir;
            try { dir = Path.Combine(Settings.Folder, "fonts"); Directory.CreateDirectory(dir); ProbeWrite(dir); }
            catch (Exception) { dir = Path.Combine(Path.GetTempPath(), "HERO VR Launcher fonts"); Directory.CreateDirectory(dir); }
            fontCollection = new PrivateFontCollection();
            foreach (var name in new[] { "Barlow-Regular.ttf", "Barlow-Medium.ttf" })
            {
                byte[] data = Resource("hero." + name);
                string f = Path.Combine(dir, name);
                if (!File.Exists(f) || new FileInfo(f).Length != data.Length)
                {
                    string tmp = f + "." + System.Diagnostics.Process.GetCurrentProcess().Id + ".tmp";
                    File.WriteAllBytes(tmp, data);
                    try { if (File.Exists(f)) File.Delete(f); File.Move(tmp, f); }
                    catch { try { File.Delete(tmp); } catch { } if (!File.Exists(f)) throw; }
                }
                fontCollection.AddFontFile(f);
            }
            foreach (var fam in fontCollection.Families)
            {
                if (fam.Name.Equals("Barlow", StringComparison.OrdinalIgnoreCase)) Regular = fam;
                else if (fam.Name.IndexOf("Medium", StringComparison.OrdinalIgnoreCase) >= 0) Medium = fam;
            }
            if (Regular == null && fontCollection.Families.Length > 0) Regular = fontCollection.Families[0];
            if (Regular == null) throw new HeroAssetException("Barlow did not load");
            if (Medium == null) Medium = Regular;
            using (var f = new Font(Regular, 12f, FontStyle.Regular, GraphicsUnit.Pixel)) { if (f.FontFamily == null) throw new HeroAssetException("Barlow did not load"); }
        }
    }
}
