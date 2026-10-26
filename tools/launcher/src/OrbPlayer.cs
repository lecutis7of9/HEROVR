// HERO VR launcher - plays the orb from orb.bin: 140 intro frames, then a 48-frame idle loop (24 fps). Frames are
// decoded and scaled to the window's size on a background thread, one or two ahead, so the UI thread only draws.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Threading;

namespace Sm2VrLauncher
{
    class OrbPlayer : IDisposable
    {
        readonly OrbData data;
        readonly object gate = new object();
        readonly Dictionary<int, Bitmap> ready = new Dictionary<int, Bitmap>();
        readonly AutoResetEvent wake = new AutoResetEvent(false);
        readonly Thread worker;
        volatile bool stop;
        int wanted = -1, generation;
        float k = 1f; PointF origin;
        Bitmap current; int currentIndex = -1; Rectangle currentRect;
        public Exception Failure;

        public OrbPlayer(OrbData d)
        {
            data = d;
            worker = new Thread(Work) { IsBackground = true, Name = "orb", Priority = ThreadPriority.BelowNormal };
            worker.Start();
        }

        public int FrameCount { get { return data.Frames.Length; } }

        /// <summary>The frame shown at `seconds` from the start: the intro once, then the idle loop forever.</summary>
        public int IndexAt(double seconds)
        {
            long f = (long)Math.Floor(seconds * data.Fps);
            if (f < 0) f = 0;
            if (f < data.IntroCount) return (int)f;
            return data.IntroCount + (int)((f - data.IntroCount) % data.LoopCount);
        }

        public int Next(int i) { return i + 1 < data.Frames.Length ? i + 1 : data.IntroCount; }

        /// <summary>Where frame i goes in window pixels.</summary>
        public Rectangle RectOf(int i)
        {
            var f = data.Frames[i];
            return Rectangle.Round(new RectangleF(origin.X + f.X * k, origin.Y + f.Y * k, f.W * k, f.H * k));
        }

        /// <summary>The design (1280x720) is drawn at scale k with its corner at origin. Clears the decoded frames.</summary>
        public void Layout(float scale, PointF at)
        {
            lock (gate)
            {
                if (Math.Abs(scale - k) < 0.0001f && at == origin) return;
                k = scale; origin = at; generation++;
                foreach (var b in ready.Values) if (b != current) b.Dispose();
                ready.Clear();
            }
        }

        /// <summary>Asks for frame i (and the next); returns true when the shown frame changed.</summary>
        public bool Show(int i)
        {
            Bitmap b = null;
            lock (gate)
            {
                wanted = i;
                if (ready.TryGetValue(i, out b)) ready.Remove(i);
                // Drop anything that is no longer needed (never the shown frame).
                var drop = new List<int>();
                foreach (var key in ready.Keys) if (key != Next(i) && key != Next(Next(i))) drop.Add(key);
                foreach (var key in drop) { ready[key].Dispose(); ready.Remove(key); }
            }
            wake.Set();
            if (b == null) return false;
            if (current != null) current.Dispose();
            current = b; currentIndex = i; currentRect = RectOf(i);
            return true;
        }

        public int ShownIndex { get { return currentIndex; } }
        public Rectangle ShownRect { get { return currentRect; } }

        public void Draw(Graphics g)
        {
            if (current == null) return;
            var mode = g.CompositingMode;
            g.CompositingMode = CompositingMode.SourceCopy;   // frames are opaque (the orb on black)
            g.DrawImageUnscaled(current, currentRect.Location);
            g.CompositingMode = mode;
        }

        /// <summary>Decodes frame i at the current size (used directly by the first paint and by tests).</summary>
        public Bitmap Render(int i)
        {
            var f = data.Frames[i];
            float kk; lock (gate) kk = k;
            int w = Math.Max(1, (int)Math.Round(f.W * kk)), h = Math.Max(1, (int)Math.Round(f.H * kk));
            var bmp = new Bitmap(w, h, PixelFormat.Format32bppPArgb);
            using (var src = OrbData.Decode(f))
            using (var g = Graphics.FromImage(bmp))
            {
                g.CompositingMode = CompositingMode.SourceCopy;
                g.InterpolationMode = InterpolationMode.Bilinear;
                g.PixelOffsetMode = PixelOffsetMode.Half;
                g.DrawImage(src, new Rectangle(0, 0, w, h), 0, 0, src.Width, src.Height, GraphicsUnit.Pixel);
            }
            return bmp;
        }

        void Work()
        {
            try
            {
                while (!stop)
                {
                    wake.WaitOne(200);
                    if (stop) break;
                    int want, gen;
                    lock (gate) { want = wanted; gen = generation; }
                    if (want < 0) continue;
                    // The wanted frame, then the two after it.
                    int idx = want;
                    for (int n = 0; n < 3 && !stop; n++, idx = Next(idx))
                    {
                        bool have;
                        lock (gate) have = ready.ContainsKey(idx) || (idx == currentIndex && gen == generation && current != null && n == 0);
                        if (have) continue;
                        var b = Render(idx);
                        lock (gate)
                        {
                            if (gen != generation || ready.ContainsKey(idx)) { b.Dispose(); break; }
                            ready[idx] = b;
                        }
                    }
                }
            }
            catch (Exception ex) { Failure = ex; }
        }

        public void Dispose()
        {
            stop = true; wake.Set();
            try { worker.Join(500); } catch { }
            lock (gate)
            {
                foreach (var b in ready.Values) b.Dispose();
                ready.Clear();
            }
            if (current != null) { current.Dispose(); current = null; }
        }
    }
}
