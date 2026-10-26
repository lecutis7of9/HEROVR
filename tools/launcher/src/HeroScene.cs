// HERO VR launcher - the new design's picture, drawn from the concept video in its 1280x720 layout: the HERO VR
// logo, five slanted tiles with a sliding highlight, the footer and the D-pad hint. Everything except the orb is
// drawn here (vector, so it is sharp at any size); the orb frames are drawn underneath by MainForm.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;

namespace Sm2VrLauncher
{
    class HeroTile
    {
        public string Title, Sub, Icon;
        public bool Enabled = true;
        public Color SubColor = Color.Empty;   // Empty: the video's #888
    }

    class HeroScene
    {
        public const float W = 1280f, H = 720f;
        public static readonly Color Neon = Color.FromArgb(104, 236, 66);
        public static readonly Color LogoGreen = Color.FromArgb(128, 228, 78);
        public static readonly Color Grey = Color.FromArgb(136, 136, 136);

        public readonly HeroTile[] Tiles =
        {
            new HeroTile { Title = "PLAY VR", Sub = "ENTER THE GAME", Icon = "helmet" },
            new HeroTile { Title = "FLATSCREEN", Sub = "PLAY ON MONITOR", Icon = "monitor" },
            new HeroTile { Title = "UPDATES", Sub = "CHECK FOR NEW VERSION", Icon = "download" },
            new HeroTile { Title = "OPTIONS", Sub = "POWERS AND GRAPHICS", Icon = "gear" },
            new HeroTile { Title = "REVERT", Sub = "PREVIOUS VERSION", Icon = "revert" },
        };
        public string Version = "0.0.63";
        /// <summary>A short state line above the tiles, only when something needs the player (null: hidden, as in the video).</summary>
        public string Status; public Color StatusColor = Grey;
        /// <summary>Seconds since the window opened (the intro's timeline); IntroEnd and later is the idle picture.</summary>
        public double T = IntroEnd;
        public const double IntroEnd = 5.1;
        /// <summary>The highlight's row (animated) and the row it slides to.</summary>
        public float Sel; public int Target;
        public int Pressed = -1;
        public bool CloseHot, MinHot, StatusHot;

        // Measured from the video (1280x720): tile tops, their top-left x (a gentle curve), size and slant.
        static readonly float[] TileLeft = { 762, 760, 752, 746, 750 };
        public const float TileTop = 128, TilePitch = 98, TileH = 90, TileW = 350, SelW = 360, SelShift = 6, Slant = 45;
        public static readonly RectangleF MinRect = new RectangleF(1196, 10, 32, 30), CloseRect = new RectangleF(1232, 10, 32, 30);
        public static readonly RectangleF StatusRect = new RectangleF(700, 92, 440, 26);

        public bool Animating { get { return T < IntroEnd || Math.Abs(Sel - Target) > 0.002f; } }

        /// <summary>Moves the highlight towards its target (about 140 ms between neighbouring rows).</summary>
        public void Step(double dt)
        {
            float d = Target - Sel;
            if (Math.Abs(d) < 0.002f) { Sel = Target; return; }
            float k = (float)(1 - Math.Exp(-dt / 0.045));
            float move = d * k;
            float min = (float)(dt * 1.2) * Math.Sign(d);
            if (Math.Abs(move) < Math.Abs(min)) move = min;
            if (Math.Abs(move) > Math.Abs(d)) move = d;
            Sel += move;
        }

        static float Clamp01(double v) { return (float)Math.Max(0, Math.Min(1, v)); }
        static float EaseOut(float t) { return 1 - (float)Math.Pow(1 - t, 3); }
        float Phase(double start, double len) { return Clamp01((T - start) / len); }

        // ---- Intro timeline (seconds from the window opening), as in the video ----
        float TileIn(int i) { return i == 0 ? Phase(2.60, 0.55) : Phase(3.42 + 0.25 * (i - 1), 0.35); }
        float TileTextIn(int i) { return i == 0 ? Phase(3.0, 0.3) : TileIn(i); }
        float LogoIn { get { return Phase(3.2, 0.25); } }
        float FooterIn { get { return Phase(4.33, 0.33); } }
        float SelectIn { get { return Phase(4.66, 0.34); } }

        float TileOffset(int i)
        {
            float p = EaseOut(TileIn(i));
            return i == 0 ? (W - TileLeft[0] + 60) * (1 - p) : 40 * (1 - p);
        }

        static PointF[] Para(float left, float top, float w, float h, float slant)
        {
            return new[] { new PointF(left, top), new PointF(left + w, top), new PointF(left + w - slant, top + h), new PointF(left - slant, top + h) };
        }

        /// <summary>The tile's shape: s is how selected it is (0..1, so the highlight can slide).</summary>
        public static PointF[] TilePoly(int i, float s)
        {
            return Para(TileLeft[i] + SelShift * s, TileTop + TilePitch * i, TileW + (SelW - TileW) * s, TileH, Slant);
        }

        static float RowLeft(float row)
        {
            int a = (int)Math.Floor(row); if (a < 0) a = 0; if (a > 3) a = 3;
            float f = Math.Max(0, Math.Min(1, row - a));
            return TileLeft[a] + (TileLeft[a + 1] - TileLeft[a]) * f;
        }

        public int HitTile(PointF p)
        {
            for (int i = 0; i < Tiles.Length; i++)
                using (var path = new GraphicsPath())
                {
                    path.AddPolygon(Para(TileLeft[i] - 4, TileTop + TilePitch * i - 4, SelW + 10, TileH + 8, Slant));
                    if (path.IsVisible(p)) return i;
                }
            return -1;
        }

        // ---- Fonts and spaced capitals ----
        readonly Dictionary<string, Font> fonts = new Dictionary<string, Font>();
        public Font Font(bool medium, float px)
        {
            string key = (medium ? "m" : "r") + px;
            Font f;
            if (fonts.TryGetValue(key, out f)) return f;
            FontFamily fam = (medium ? Theme.HeroMedium : Theme.HeroRegular) ?? (medium ? HeroAssets.Medium : HeroAssets.Regular);
            try { f = fam != null ? new Font(fam, px, FontStyle.Regular, GraphicsUnit.Pixel) : null; } catch { f = null; }
            if (f == null)
                foreach (var name in new[] { "Bahnschrift", "Segoe UI", "DejaVu Sans" })
                    try { f = new Font(name, px * 0.96f, medium ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Pixel); break; } catch { }
            fonts[key] = f;
            return f;
        }

        static readonly StringFormat Typo = MakeTypo();
        static StringFormat MakeTypo()
        {
            var sf = (StringFormat)StringFormat.GenericTypographic.Clone();
            sf.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces | StringFormatFlags.NoWrap;
            return sf;
        }
        readonly Dictionary<string, float> advances = new Dictionary<string, float>();

        float Advance(Graphics g, Font f, char c)
        {
            string key = f.GetHashCode() + "|" + f.Size + "|" + c;
            float a;
            if (!advances.TryGetValue(key, out a))
            {
                var st = g.Save();
                g.ResetTransform();
                a = g.MeasureString(c.ToString(), f, PointF.Empty, Typo).Width;
                g.Restore(st);
                advances[key] = a;
            }
            return a;
        }

        public float Measure(Graphics g, string s, Font f, float track)
        {
            float w = 0;
            for (int i = 0; i < s.Length; i++) w += Advance(g, f, s[i]) + (i < s.Length - 1 ? track * f.Size : 0);
            return w;
        }

        static float Ascent(Font f)
        {
            // Some GDI+ builds cannot read metrics of a private (embedded) font family; Barlow's
            // Windows cell ascent is 1112/1000 em, which is what GDI+ reports on Windows.
            try
            {
                var fam = f.FontFamily;
                return f.Size * fam.GetCellAscent(f.Style) / fam.GetEmHeight(f.Style);
            }
            catch (ArgumentException) { return f.Size * (f.Name.IndexOf("Barlow", StringComparison.OrdinalIgnoreCase) >= 0 ? 1.112f : 0.9f); }
        }

        /// <summary>Spaced capitals with their baseline at `baseline` (letter spacing in ems).</summary>
        public void Tracked(Graphics g, string s, Font f, Color c, float x, float baseline, float track)
        {
            if (string.IsNullOrEmpty(s) || c.A == 0) return;
            float y = baseline - Ascent(f);
            using (var b = new SolidBrush(c))
                for (int i = 0; i < s.Length; i++)
                {
                    if (s[i] != ' ') g.DrawString(s[i].ToString(), f, b, x, y, Typo);
                    x += Advance(g, f, s[i]) + track * f.Size;
                }
        }

        static Color A(Color c, float a) { return Color.FromArgb(Math.Max(0, Math.Min(255, (int)(c.A * a))), c.R, c.G, c.B); }

        // ---- Drawing ----
        /// <summary>Draws the whole overlay (not the orb) in 1280x720 design units; the caller scales and places it.</summary>
        public void Draw(Graphics g)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.TextRenderingHint = TextRenderingHint.AntiAlias;
            g.CompositingQuality = CompositingQuality.HighQuality;
            DrawDecorations(g);
            for (int i = Tiles.Length - 1; i >= 0; i--) DrawTile(g, i);
            DrawHighlight(g);
            for (int i = 0; i < Tiles.Length; i++) DrawTileContent(g, i);
            DrawLogo(g, LogoIn);
            DrawFooter(g);
            DrawStatus(g);
            DrawWindowButtons(g);
        }

        float Selectedness(int i) { return Math.Max(0, 1 - Math.Abs(Sel - i)); }

        void DrawDecorations(Graphics g)
        {
            // No ghost tiles or side strips: only the highlighted item has a frame (the player's video; build 10 taste note).
            float fa = FooterIn;
            if (fa > 0)
                using (var p = new Pen(A(Color.FromArgb(20, 104, 236, 66), fa), 1f))
                {
                    g.DrawLine(p, 1150, 0, 1080, 70); g.DrawLine(p, 1210, 0, 1130, 92); g.DrawLine(p, 1280, 18, 1196, 112);
                    g.DrawLine(p, 1180, 720, 1250, 640); g.DrawLine(p, 1236, 720, 1280, 668);
                }
        }

        void DrawTile(Graphics g, int i)
        {
            float a = TileIn(i);
            if (a <= 0) return;
            float s = Selectedness(i), off = TileOffset(i);
            var poly = TilePoly(i, s);
            for (int k = 0; k < 4; k++) poly[k].X += off;
            // Unselected items have no box at all (no glass, no outline): DrawHighlight draws the one frame.
            if (Pressed == i)
                using (var b = new SolidBrush(A(Color.FromArgb(40, 104, 236, 66), a))) g.FillPolygon(b, poly);
        }

        void DrawHighlight(Graphics g)
        {
            // The highlight is its own frame that slides between rows (in the video it rests on PLAY VR).
            int near = (int)Math.Round(Sel);
            float a = TileIn(Math.Max(0, Math.Min(4, near)));
            if (a <= 0) return;
            float top = TileTop + TilePitch * Sel;
            float off = TileOffset(Math.Max(0, Math.Min(4, near)));
            var poly = Para(RowLeft(Sel) + SelShift + off, top, SelW, TileH, Slant);
            using (var b = new LinearGradientBrush(new PointF(poly[3].X, top + TileH), new PointF(poly[1].X, top), A(Color.FromArgb(96, 70, 190, 40), a), A(Color.FromArgb(38, 60, 170, 40), a)))
                g.FillPolygon(b, poly);
            using (var b = new LinearGradientBrush(new PointF(0, top), new PointF(0, top + TileH * 0.55f), A(Color.FromArgb(46, 170, 255, 120), a), Color.FromArgb(0, 104, 236, 66)))
                g.FillPolygon(b, poly);
            // Glow: wide faint strokes under a bright 2 px edge.
            foreach (var gl in new[] { new KeyValuePair<float, int>(12f, 14), new KeyValuePair<float, int>(7f, 28), new KeyValuePair<float, int>(4f, 60) })
                using (var p = new Pen(A(Color.FromArgb(gl.Value, 104, 236, 66), a), gl.Key) { LineJoin = LineJoin.Miter })
                    g.DrawPolygon(p, poly);
            using (var p = new Pen(A(Color.FromArgb(255, 132, 248, 92), a), 2f) { LineJoin = LineJoin.Miter }) g.DrawPolygon(p, poly);
        }

        void DrawTileContent(Graphics g, int i)
        {
            float a = TileTextIn(i);
            if (a <= 0) return;
            var t = Tiles[i];
            float s = Selectedness(i), off = TileOffset(i), top = TileTop + TilePitch * i;
            float left = TileLeft[i] + SelShift * s + off;
            float en = t.Enabled ? 1f : 0.42f;
            Color white = A(Color.FromArgb(244, 246, 244), a * en);
            DrawIcon(g, t.Icon, left + 23, top + 46, white);
            float tx = left + 74;
            Tracked(g, t.Title, Font(true, 27), white, tx, top + 30 + 19, 0.04f);
            Color sub = t.SubColor.IsEmpty ? Grey : t.SubColor;
            Tracked(g, t.Sub, Font(false, 15.5f), A(sub, a * (t.Enabled ? 1f : 0.7f)), tx + 1, top + 71, 0.15f);
        }

        // ---- Icons: white line art, about 36 units, stroke 2.2 ----
        public static void DrawIcon(Graphics g, string icon, float cx, float cy, Color c)
        {
            using (var p = new Pen(c, 2.2f) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round })
            {
                switch (icon)
                {
                    case "helmet":
                        using (var path = new GraphicsPath())
                        {
                            path.AddArc(cx - 17, cy - 19, 34, 32, 180, 180);
                            path.AddLine(cx + 17, cy - 3, cx + 17, cy + 8);
                            path.AddLine(cx + 17, cy + 8, cx + 10, cy + 18);
                            path.AddLine(cx + 10, cy + 18, cx + 5, cy + 18);
                            path.StartFigure();
                            path.AddLine(cx - 5, cy + 18, cx - 10, cy + 18);
                            path.AddLine(cx - 10, cy + 18, cx - 17, cy + 8);
                            path.AddLine(cx - 17, cy + 8, cx - 17, cy - 3);
                            g.DrawPath(p, path);
                        }
                        // the visor and the cheek guards
                        using (var v = new GraphicsPath())
                        {
                            v.AddLine(cx - 13, cy - 7, cx + 13, cy - 7);
                            v.AddLine(cx + 13, cy - 7, cx + 11, cy + 1);
                            v.AddLine(cx + 11, cy + 1, cx + 4, cy + 3);
                            v.AddLine(cx + 4, cy + 3, cx, cy);
                            v.AddLine(cx, cy, cx - 4, cy + 3);
                            v.AddLine(cx - 4, cy + 3, cx - 11, cy + 1);
                            v.CloseFigure();
                            g.DrawPath(p, v);
                        }
                        g.DrawLine(p, cx - 5, cy + 8, cx - 5, cy + 18);
                        g.DrawLine(p, cx + 5, cy + 8, cx + 5, cy + 18);
                        break;
                    case "monitor":
                        g.DrawRectangle(p, cx - 19, cy - 14, 38, 25);
                        g.DrawLine(p, cx, cy + 11, cx, cy + 16);
                        g.DrawLine(p, cx - 8, cy + 17, cx + 8, cy + 17);
                        break;
                    case "download":
                        g.DrawLine(p, cx, cy - 17, cx, cy + 4);
                        g.DrawLines(p, new[] { new PointF(cx - 8, cy - 4), new PointF(cx, cy + 4), new PointF(cx + 8, cy - 4) });
                        g.DrawLines(p, new[] { new PointF(cx - 17, cy + 3), new PointF(cx - 17, cy + 15), new PointF(cx + 17, cy + 15), new PointF(cx + 17, cy + 3) });
                        break;
                    case "gear":
                        using (var path = new GraphicsPath())
                        {
                            var pts = new List<PointF>();
                            int teeth = 8;
                            for (int k = 0; k < teeth; k++)
                            {
                                double a0 = (k / (double)teeth) * Math.PI * 2 - Math.PI / 2;
                                double w = Math.PI / teeth;
                                foreach (var pr in new[] { new KeyValuePair<double, float>(-w * 0.55, 13.5f), new KeyValuePair<double, float>(-w * 0.32, 18.5f), new KeyValuePair<double, float>(w * 0.32, 18.5f), new KeyValuePair<double, float>(w * 0.55, 13.5f) })
                                    pts.Add(new PointF(cx + (float)Math.Cos(a0 + pr.Key) * pr.Value, cy + (float)Math.Sin(a0 + pr.Key) * pr.Value));
                            }
                            path.AddPolygon(pts.ToArray());
                            g.DrawPath(p, path);
                        }
                        g.DrawEllipse(p, cx - 6, cy - 6, 12, 12);
                        break;
                    case "revert":
                        g.DrawArc(p, cx - 16, cy - 16, 32, 32, -60, 300);
                        float ex = cx + 16 * (float)Math.Cos(-60 * Math.PI / 180), ey = cy + 16 * (float)Math.Sin(-60 * Math.PI / 180);
                        g.DrawLines(p, new[] { new PointF(ex - 1, ey - 9), new PointF(ex + 1, ey + 1), new PointF(ex + 10, ey - 2) });
                        break;
                    case "stop":
                        g.DrawRectangle(p, cx - 13, cy - 13, 26, 26);
                        break;
                }
            }
        }

        // ---- The HERO VR logo, as vector shapes measured from the video ----
        public void DrawLogo(Graphics g, float a)
        {
            if (a <= 0) return;
            using (var path = new GraphicsPath())
            {
                path.FillMode = FillMode.Winding;
                // H
                path.AddRectangle(new RectangleF(64, 37.5f, 11.5f, 51.5f));
                path.AddRectangle(new RectangleF(104, 37.5f, 11.5f, 51.5f));
                path.AddRectangle(new RectangleF(70, 58, 40, 9));
                // E
                path.AddRectangle(new RectangleF(128, 37.5f, 12, 51.5f));
                path.AddRectangle(new RectangleF(134, 37.5f, 40, 10));
                path.AddRectangle(new RectangleF(134, 59.5f, 38, 8));
                path.AddRectangle(new RectangleF(134, 79.5f, 40, 9.5f));
                // R: stem, the bowl (top bar, rounded right side, a bottom bar apart from the stem) and the leg
                path.AddRectangle(new RectangleF(186, 37.5f, 11, 51.5f));
                using (var bowl = new GraphicsPath())
                {
                    bowl.AddLine(192, 37.5f, 222, 37.5f);
                    bowl.AddArc(212, 37.5f, 25, 23, 270, 90);
                    bowl.AddArc(212, 45, 25, 23, 0, 90);
                    bowl.AddLine(224.5f, 68, 202, 68);
                    bowl.AddLine(202, 68, 202, 61);
                    bowl.AddLine(202, 61, 222, 61);
                    bowl.AddArc(219, 52, 9, 9, 90, -90);
                    bowl.AddArc(219, 46, 9, 9, 0, -90);
                    bowl.AddLine(223.5f, 46, 192, 46);
                    bowl.CloseFigure();
                    path.AddPath(bowl, false);
                }
                path.AddPolygon(new[] { new PointF(209, 68), new PointF(221.5f, 68), new PointF(237, 89), new PointF(224.5f, 89) });
                // Q-like O: a ring open at the lower right, with a slash
                using (var ring = new GraphicsPath())
                {
                    float cx = 276, cy = 63.5f, ro = 27.5f, ri = 16.5f;
                    ring.AddArc(cx - ro, cy - ro, ro * 2, ro * 2, 104, 281);
                    ring.AddArc(cx - ri, cy - ri, ri * 2, ri * 2, 25, -281);
                    ring.CloseFigure();
                    path.AddPath(ring, false);
                }
                path.AddPolygon(new[] { new PointF(288.5f, 89.5f), new PointF(301.5f, 78.5f), new PointF(305.5f, 83), new PointF(292.5f, 94) });
                // Glow, then the fill.
                for (int k = 3; k >= 1; k--)
                    using (var p = new Pen(A(Color.FromArgb(18 * (4 - k), 110, 230, 70), a), k * 3.2f) { LineJoin = LineJoin.Round })
                        g.DrawPath(p, path);
                using (var b = new LinearGradientBrush(new PointF(0, 37), new PointF(0, 90), A(Color.FromArgb(150, 240, 96), a), A(Color.FromArgb(104, 214, 62), a)))
                    g.FillPath(b, path);
            }
            // VR in thin strokes, with the lines beside it.
            using (var p = new Pen(A(LogoGreen, a), 4.6f) { LineJoin = LineJoin.Miter, StartCap = LineCap.Flat, EndCap = LineCap.Flat })
            {
                g.DrawLines(p, new[] { new PointF(152, 104.5f), new PointF(164.5f, 124.5f), new PointF(177, 104.5f) });
                using (var r = new GraphicsPath())
                {
                    r.AddLine(191.5f, 126, 191.5f, 104.5f);
                    r.AddLine(191.5f, 104.5f, 205, 104.5f);
                    r.AddArc(199, 104.5f, 12, 11, 270, 180);
                    r.AddLine(205, 115.5f, 191.5f, 115.5f);
                    g.DrawPath(p, r);
                }
                g.DrawLine(p, 202, 115.5f, 212.5f, 126);
            }
            using (var p = new Pen(A(Color.FromArgb(200, 120, 220, 76), a), 1.8f))
            {
                g.DrawLine(p, 64, 112, 131, 112); g.DrawLine(p, 131, 112, 135, 108);
                g.DrawLine(p, 105, 120, 137, 120); g.DrawLine(p, 137, 120, 141, 116);
                g.DrawLine(p, 241, 112, 302, 112); g.DrawLine(p, 237, 108, 241, 112);
                g.DrawLine(p, 238, 120, 265, 120); g.DrawLine(p, 234, 116, 238, 120);
            }
        }

        void DrawFooter(Graphics g)
        {
            float a = FooterIn;
            if (a <= 0) return;
            Color grey = A(Color.FromArgb(178, 182, 178), a);
            var f = Font(false, 14.3f);
            string ver = "VERSION " + Version;
            Tracked(g, ver, f, grey, 65, 645.5f, 0.08f);
            float vw = Measure(g, ver, f, 0.08f);
            using (var p = new Pen(A(Color.FromArgb(120, 150, 150, 150), a), 1f)) g.DrawLine(p, 65 + vw + 14, 640, 65 + vw + 116, 640);
            Tracked(g, "MADE BY AGENT MULDER", f, grey, 66, 674.5f, 0.105f);
            var f2 = Font(false, 17f);
            string pw = "POWERED BY ";
            Tracked(g, pw, f2, A(Color.FromArgb(200, 204, 200), a), 445, 674.5f, 0.12f);
            Tracked(g, "HERO VR", Font(true, 17f), A(Neon, a), 445 + Measure(g, pw, f2, 0.12f) + 0.12f * 17, 674.5f, 0.12f);
            float sa = SelectIn;
            if (sa > 0)
            {
                // D-pad outline and SELECT
                using (var path = new GraphicsPath())
                {
                    float x0 = 1079, y0 = 650, s = 35, arm = 11.5f, c0 = (s - arm) / 2f;
                    path.AddPolygon(new[] {
                        new PointF(x0 + c0, y0), new PointF(x0 + c0 + arm, y0), new PointF(x0 + c0 + arm, y0 + c0), new PointF(x0 + s, y0 + c0),
                        new PointF(x0 + s, y0 + c0 + arm), new PointF(x0 + c0 + arm, y0 + c0 + arm), new PointF(x0 + c0 + arm, y0 + s), new PointF(x0 + c0, y0 + s),
                        new PointF(x0 + c0, y0 + c0 + arm), new PointF(x0, y0 + c0 + arm), new PointF(x0, y0 + c0), new PointF(x0 + c0, y0 + c0) });
                    using (var p = new Pen(A(Color.FromArgb(150, 154, 150), sa), 1.6f) { LineJoin = LineJoin.Round }) g.DrawPath(p, path);
                }
                Tracked(g, "SELECT", Font(false, 18.5f), A(Color.FromArgb(160, 164, 160), sa), 1123, 673, 0.12f);
            }
        }

        void DrawStatus(Graphics g)
        {
            if (string.IsNullOrEmpty(Status)) return;
            float a = Math.Min(TileIn(0), FooterIn > 0 ? 1f : TileTextIn(0));
            if (a <= 0) return;
            var f = Font(false, 13f);
            float w = Measure(g, Status, f, 0.1f);
            float x = Math.Max(StatusRect.X, 1120 - w);
            using (var b = new SolidBrush(A(StatusColor, a))) g.FillEllipse(b, x - 14, 101, 6, 6);
            Tracked(g, Status, f, A(StatusHot ? Color.White : StatusColor, a), x, 108.5f, 0.1f);
        }

        void DrawWindowButtons(Graphics g)
        {
            float a = Math.Max(FooterIn, T >= IntroEnd ? 1f : 0f);
            if (a <= 0) return;
            using (var p = new Pen(A(MinHot ? Color.White : Grey, a), 1.6f))
                g.DrawLine(p, MinRect.X + 10, MinRect.Y + 16, MinRect.Right - 10, MinRect.Y + 16);
            using (var p = new Pen(A(CloseHot ? Color.White : Grey, a), 1.6f))
            {
                float cx = CloseRect.X + CloseRect.Width / 2, cy = CloseRect.Y + 16, r = 6;
                g.DrawLine(p, cx - r, cy - r, cx + r, cy + r);
                g.DrawLine(p, cx - r, cy + r, cx + r, cy - r);
            }
        }
    }
}
