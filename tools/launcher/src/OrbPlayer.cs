// HERO VR launcher - plays the orb from orb.bin: 140 intro frames (24 fps), then the idle loop (180 frames at 30 fps
// that wrap without a cut) with the orb inside its ring turned forward at the video's own slow speed as each frame is
// drawn (orb.bin version 3: the angle grows with time, so the turn never wraps). Frames are decoded, scaled to the
// window's size and turned on a background thread, a few ahead, so the UI thread only draws.
// A "step" is the intro frame number, then IntroCount + the loop frames shown since the loop began (not wrapped).
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Threading;

namespace Sm2VrLauncher
{
    class OrbPlayer : IDisposable
    {
        readonly OrbData data;
        readonly object gate = new object();
        readonly Dictionary<long, Bitmap> ready = new Dictionary<long, Bitmap>();
        readonly AutoResetEvent wake = new AutoResetEvent(false);
        readonly Thread worker;
        volatile bool stop;
        long wanted = -1; int generation;
        float k = 1f; PointF origin;
        Bitmap current; long currentStep = -1; Rectangle currentRect;
        public Exception Failure;

        public OrbPlayer(OrbData d)
        {
            data = d;
            worker = new Thread(Work) { IsBackground = true, Name = "orb", Priority = ThreadPriority.Normal };
            worker.Start();
        }

        public int FrameCount { get { return data.Frames.Length; } }

        /// <summary>Frames decoded ahead of the shown one.</summary>
        const int Ahead = 4;

        /// <summary>The step shown at `seconds` from the start: the intro once (at Fps), then the idle loop forever
        /// (at LoopFps).</summary>
        public long StepAt(double seconds)
        {
            if (seconds < 0) seconds = 0;
            long f = (long)Math.Floor(seconds * data.Fps);
            if (f < data.IntroCount) return f;
            double intro = data.IntroCount / (double)data.Fps;
            long l = (long)Math.Floor((seconds - intro) * data.LoopFps + 1e-9);
            return data.IntroCount + Math.Max(0, l);
        }

        /// <summary>The frame a step shows (the loop's frames over and over).</summary>
        public int FrameOf(long step)
        {
            if (step < data.IntroCount) return (int)Math.Max(0, step);
            return data.IntroCount + (int)((step - data.IntroCount) % data.LoopCount);
        }

        /// <summary>How far (degrees, 0..360) the orb is turned at a step: 0 in the intro (its frames hold their own
        /// spin-up), then SpinStart + Spin x the loop's time.</summary>
        public double AngleOf(long step)
        {
            if (step < data.IntroCount || data.Spin <= 0) return 0;
            double a = data.SpinStart + data.Spin * ((step - data.IntroCount) / (double)data.LoopFps);
            a %= 360.0;
            return a < 0 ? a + 360.0 : a;
        }

        /// <summary>The frame shown at `seconds` (FrameOf(StepAt(seconds))).</summary>
        public int IndexAt(double seconds) { return FrameOf(StepAt(seconds)); }

        public long Next(long step) { return step + 1; }

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
        public bool Show(long i)
        {
            Bitmap b = null;
            lock (gate)
            {
                wanted = i;
                if (ready.TryGetValue(i, out b)) ready.Remove(i);
                // Drop anything that is no longer needed (never the shown frame).
                var drop = new List<long>();
                var keep = new HashSet<long>();
                long j = Next(i);
                for (int n = 0; n < Ahead; n++, j = Next(j)) keep.Add(j);
                foreach (var key in ready.Keys) if (!keep.Contains(key)) drop.Add(key);
                foreach (var key in drop) { ready[key].Dispose(); ready.Remove(key); }
            }
            wake.Set();
            if (b == null) return false;
            if (current != null) current.Dispose();
            current = b; currentStep = i; currentRect = RectOf(FrameOf(i));
            return true;
        }

        public long ShownStep { get { return currentStep; } }
        public int ShownIndex { get { return currentStep < 0 ? -1 : FrameOf(currentStep); } }
        public Rectangle ShownRect { get { return currentRect; } }

        public void Draw(Graphics g)
        {
            if (current == null) return;
            var mode = g.CompositingMode;
            g.CompositingMode = CompositingMode.SourceCopy;   // frames are opaque (the orb on black)
            g.DrawImageUnscaled(current, currentRect.Location);
            g.CompositingMode = mode;
        }

        /// <summary>Decodes a step's frame at the current size, turned as AngleOf says (used directly by tests).</summary>
        public Bitmap Render(long step)
        {
            var f = data.Frames[FrameOf(step)];
            double angle = AngleOf(step);
            float kk; lock (gate) kk = k;
            int w = Math.Max(1, (int)Math.Round(f.W * kk)), h = Math.Max(1, (int)Math.Round(f.H * kk));
            var bmp = new Bitmap(w, h, PixelFormat.Format32bppPArgb);
            using (var ms = new System.IO.MemoryStream(f.Jpeg))
            using (var src = Image.FromStream(ms))   // drawn straight from the decoded JPEG (no extra copy)
            {
                // The turn is done at the JPEG's own size (a few ms even at 4K), then the result is scaled up once.
                Bitmap nat = null;
                if (angle != 0)
                {
                    nat = new Bitmap(src.Width, src.Height, PixelFormat.Format32bppPArgb);
                    using (var g = Graphics.FromImage(nat))
                    {
                        g.CompositingMode = CompositingMode.SourceCopy;
                        g.DrawImage(src, new Rectangle(0, 0, src.Width, src.Height), 0, 0, src.Width, src.Height, GraphicsUnit.Pixel);
                    }
                    Turn(nat, src, f, (float)angle, src.Width, src.Height);
                }
                try
                {
                    Image pic = nat ?? src;
                    using (var g = Graphics.FromImage(bmp))
                    {
                        g.CompositingMode = CompositingMode.SourceCopy;
                        g.InterpolationMode = InterpolationMode.Bilinear;
                        g.PixelOffsetMode = PixelOffsetMode.Half;
                        g.DrawImage(pic, new Rectangle(0, 0, w, h), 0, 0, pic.Width, pic.Height, GraphicsUnit.Pixel);
                    }
                }
                finally { if (nat != null) nat.Dispose(); }
            }
            return bmp;
        }

        /// <summary>Turns the orb inside its ring by `angle` degrees (clockwise on screen) about its centre: inside
        /// DiscIn it is the turned picture, outside DiscOut the frame as it is (the halo and floor glow stay put),
        /// smoothly blended in between (a smoothstep in the radius).</summary>
        void Turn(Bitmap bmp, Image src, OrbFrame f, float angle, int w, int h)
        {
            float sx = w / (float)f.W, sy = h / (float)f.H;
            float cx = (f.Cx - f.X) * sx, cy = (data.CentreY - f.Y) * sy;
            float rIn = data.DiscIn * f.Scale * sx, rOut = data.DiscOut * f.Scale * sx;
            var box = Rectangle.Intersect(new Rectangle(0, 0, w, h),
                Rectangle.FromLTRB((int)Math.Floor(cx - rOut) - 1, (int)Math.Floor(cy - rOut) - 1, (int)Math.Ceiling(cx + rOut) + 2, (int)Math.Ceiling(cy + rOut) + 2));
            if (box.Width <= 0 || box.Height <= 0) return;
            using (var rot = new Bitmap(w, h, PixelFormat.Format32bppPArgb))
            {
                using (var gr = Graphics.FromImage(rot))
                {
                    gr.CompositingMode = CompositingMode.SourceCopy;
                    gr.InterpolationMode = InterpolationMode.Bilinear;
                    gr.PixelOffsetMode = PixelOffsetMode.Half;
                    gr.TranslateTransform(cx, cy); gr.RotateTransform(angle); gr.TranslateTransform(-cx, -cy);
                    gr.DrawImage(src, new Rectangle(0, 0, w, h), 0, 0, src.Width, src.Height, GraphicsUnit.Pixel);
                }
                // The inside: the turned picture as it is (natively), up to just short of DiscIn.
                float ri = Math.Max(0, rIn - 2);
                if (ri > 0)
                    using (var g = Graphics.FromImage(bmp))
                    using (var disc = new GraphicsPath())
                    {
                        disc.AddEllipse(cx - ri, cy - ri, 2 * ri, 2 * ri);
                        g.SetClip(disc);   // a hard clip is fine: the blended edge below covers it
                        g.CompositingMode = CompositingMode.SourceCopy;
                        g.DrawImageUnscaled(rot, 0, 0);
                    }
                // The edge (rIn - 3 .. rOut): blended pixel by pixel, a row's two short spans at a time.
                // (Whole-picture locks: a sub-rectangle lock comes back skewed under Mono's libgdiplus.)
                var all = new Rectangle(0, 0, w, h);
                var bd = bmp.LockBits(all, ImageLockMode.ReadWrite, PixelFormat.Format32bppPArgb);
                var rd = rot.LockBits(all, ImageLockMode.ReadOnly, PixelFormat.Format32bppPArgb);
                try
                {
                    float r0 = Math.Max(0, rIn - 3), band = Math.Max(0.5f, rOut - rIn);
                    var a = new byte[4 * box.Width]; var b = new byte[4 * box.Width];
                    for (int y = 0; y < box.Height; y++)
                    {
                        float dy = box.Y + y + 0.5f - cy;
                        if (Math.Abs(dy) > rOut) continue;
                        float outer = (float)Math.Sqrt(Math.Max(0, rOut * rOut - dy * dy));
                        float inner = Math.Abs(dy) < r0 ? (float)Math.Sqrt(r0 * r0 - dy * dy) : 0;
                        for (int side = -1; side <= 1; side += 2)
                        {
                            // x from cx + side*inner out to cx + side*outer
                            float xa = side < 0 ? cx - outer : cx + inner, xb = side < 0 ? cx - inner : cx + outer;
                            int x0 = Math.Max(0, (int)Math.Floor(xa - 0.5f) - box.X), x1 = Math.Min(box.Width, (int)Math.Ceiling(xb + 0.5f) - box.X);
                            if (inner == 0 && side > 0) break;   // the row runs through the middle: one span did it
                            if (inner == 0) x1 = Math.Min(box.Width, (int)Math.Ceiling(cx + outer + 0.5f) - box.X);
                            int n = x1 - x0;
                            if (n <= 0) continue;
                            IntPtr pb = bd.Scan0 + (box.Y + y) * bd.Stride + 4 * (box.X + x0), pr = rd.Scan0 + (box.Y + y) * rd.Stride + 4 * (box.X + x0);
                            Marshal.Copy(pb, a, 0, 4 * n); Marshal.Copy(pr, b, 0, 4 * n);
                            for (int i = 0; i < n; i++)
                            {
                                float dx = box.X + x0 + i + 0.5f - cx, r = (float)Math.Sqrt(dx * dx + dy * dy);
                                if (r >= rOut) continue;
                                float t = r <= rIn ? 1f : (rOut - r) / band; t = t * t * (3 - 2 * t);
                                int q = 4 * i;
                                for (int c = 0; c < 4; c++) a[q + c] = (byte)(a[q + c] + (b[q + c] - a[q + c]) * t + 0.5f);
                            }
                            Marshal.Copy(a, 0, pb, 4 * n);
                        }
                    }
                }
                finally { rot.UnlockBits(rd); bmp.UnlockBits(bd); }
            }
        }

        void Work()
        {
            try
            {
                while (!stop)
                {
                    wake.WaitOne(200);
                    if (stop) break;
                    long want; int gen;
                    lock (gate) { want = wanted; gen = generation; }
                    if (want < 0) continue;
                    // The wanted frame, then the ones after it.
                    long idx = want;
                    for (int n = 0; n <= Ahead && !stop; n++, idx = Next(idx))
                    {
                        bool have;
                        lock (gate) have = ready.ContainsKey(idx) || (idx == currentStep && gen == generation && current != null && n == 0);
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
