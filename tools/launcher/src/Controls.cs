// SM2 VR Launcher - theme and owner-drawn controls (C# 5 / .NET Framework 4.8, WinForms).
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    static class Theme
    {
        public static readonly Color Bg = Color.FromArgb(10, 12, 19);
        public static readonly Color PanelTint = Color.FromArgb(200, 10, 12, 20);
        public static readonly Color PanelBorder = Color.FromArgb(60, 205, 210, 220);
        public static readonly Color Red = Color.FromArgb(226, 32, 48);
        public static readonly Color RedHot = Color.FromArgb(246, 58, 72);
        public static readonly Color RedDark = Color.FromArgb(120, 12, 22);
        public static readonly Color Silver = Color.FromArgb(206, 211, 219);
        public static readonly Color Text = Color.FromArgb(242, 244, 248);
        public static readonly Color Sub = Color.FromArgb(150, 158, 174);
        public static readonly Color Dim = Color.FromArgb(95, 102, 118);
        public static readonly Color Good = Color.FromArgb(72, 214, 132);
        public static readonly Color Warn = Color.FromArgb(246, 182, 70);
        public static readonly Color Bad = Color.FromArgb(255, 86, 96);
        public static readonly Color Info = Color.FromArgb(120, 170, 255);
        public static readonly Color Field = Color.FromArgb(16, 19, 29);
        public static readonly Color ScrollBg = Color.FromArgb(14, 16, 25);
        public static float Scale = 1f;

        public static int S(int v) { return (int)Math.Round(v * Scale); }
        public static Rectangle R(int x, int y, int w, int h) { return new Rectangle(S(x), S(y), S(w), S(h)); }

        static readonly Dictionary<string, Font> fonts = new Dictionary<string, Font>();
        static readonly Dictionary<string, string> families = new Dictionary<string, string>();

        static string Family(string kind)
        {
            string found;
            if (families.TryGetValue(kind, out found)) return found;
            string[] wanted;
            if (kind == "display") wanted = new[] { "Bahnschrift SemiBold", "Bahnschrift", "Segoe UI Black", "Segoe UI", "Noto Sans", "DejaVu Sans" };
            else if (kind == "mono") wanted = new[] { "Cascadia Mono", "Consolas", "Noto Sans Mono", "DejaVu Sans Mono", "Courier New" };
            else if (kind == "thin") wanted = new[] { "Bahnschrift Light", "Segoe UI Light", "Segoe UI Semilight", "Montserrat", "Segoe UI", "Noto Sans", "DejaVu Sans" };
            else if (kind == "heavy") wanted = new[] { "Segoe UI Black", "Arial Black", "Poppins", "Bahnschrift SemiBold", "Segoe UI", "Noto Sans", "DejaVu Sans" };
            else wanted = new[] { "Segoe UI", "Inter", "Noto Sans", "DejaVu Sans", "Arial" };
            var installed = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            using (var col = new InstalledFontCollection())
                foreach (var f in col.Families) installed.Add(f.Name);
            found = SystemFonts.DefaultFont.FontFamily.Name;
            foreach (var w in wanted) if (installed.Contains(w)) { found = w; break; }
            families[kind] = found;
            return found;
        }

        /// <summary>A cached font; size is in 96-dpi pixels and scaled.</summary>
        public static Font F(string kind, float px, FontStyle style)
        {
            string key = kind + "|" + px + "|" + (int)style;
            Font f;
            if (!fonts.TryGetValue(key, out f))
            {
                f = new Font(Family(kind), px * Scale, style, GraphicsUnit.Pixel);
                fonts[key] = f;
            }
            return f;
        }
        public static Font Ui(float px) { return F("ui", px, FontStyle.Regular); }
        public static Font UiBold(float px) { return F("ui", px, FontStyle.Bold); }
        public static Font Display(float px) { return F("display", px, FontStyle.Bold); }
        public static Font Mono(float px) { return F("mono", px, FontStyle.Regular); }
        /// <summary>The thin face of the wordmark ("SM2"): Bahnschrift Light / Segoe UI Light on Windows.</summary>
        public static Font Thin(float px) { return F("thin", px, FontStyle.Regular); }
        /// <summary>The heavy face of the wordmark ("VR") and PLAY VR: Segoe UI Black / Arial Black are already black, others get bold.</summary>
        public static Font Heavy(float px)
        {
            string fam = Family("heavy");
            return F("heavy", px, fam.EndsWith("Black") ? FontStyle.Regular : FontStyle.Bold);
        }

        public static void Hq(Graphics g)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
            g.PixelOffsetMode = PixelOffsetMode.HighQuality;
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        }

        public static GraphicsPath Round(RectangleF r, float radius)
        {
            var p = new GraphicsPath();
            float d = Math.Max(1f, radius * 2);
            if (radius <= 0.5f) { p.AddRectangle(r); return p; }
            p.AddArc(r.X, r.Y, d, d, 180, 90);
            p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
            p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
            p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
            p.CloseFigure();
            return p;
        }

        public static Color StateColor(CheckState s)
        {
            switch (s)
            {
                case CheckState.Ok: return Good;
                case CheckState.Warn: return Warn;
                case CheckState.Bad: return Bad;
                case CheckState.Busy: return Silver;
                default: return Info;
            }
        }

        /// <summary>Text height for a width (word wrap).</summary>
        public static int Measure(string text, Font font, int width)
        {
            if (string.IsNullOrEmpty(text)) return 0;
            using (var bmp = new Bitmap(1, 1))
            using (var g = Graphics.FromImage(bmp))
            {
                g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
                return (int)Math.Ceiling(g.MeasureString(text, font, width).Height);
            }
        }
    }

    enum CheckState { Ok, Warn, Bad, Info, Busy }

    /// <summary>A container that can paint the area behind a child (fake transparency for owner-drawn controls).</summary>
    interface IBackdrop
    {
        void DrawBackdrop(Graphics g, Rectangle srcInMe, Point dest);
    }

    static class Backdrop
    {
        public static void Draw(Control c, Graphics g, Rectangle r)
        {
            Control p = c.Parent;
            if (p == null) { g.Clear(Theme.Bg); return; }
            var src = new Rectangle(r.X + c.Left, r.Y + c.Top, r.Width, r.Height);
            var b = p as IBackdrop;
            if (b != null) b.DrawBackdrop(g, src, r.Location);
            else using (var br = new SolidBrush(p.BackColor)) g.FillRectangle(br, r);
        }

        public static void InvalidateAll(Control root)
        {
            var gp = root as GlassPanel;
            if (gp != null) gp.Rebuild();
            root.Invalidate();
            foreach (Control c in root.Controls) InvalidateAll(c);
        }
    }

    /// <summary>Base of every owner-drawn control: double buffered, paints its parent's backdrop first.</summary>
    class Skin : Control
    {
        public Skin()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            ForeColor = Theme.Text;
        }
        protected override void OnPaintBackground(PaintEventArgs e) { Backdrop.Draw(this, e.Graphics, ClientRectangle); }
        protected override void OnPaint(PaintEventArgs e) { Theme.Hq(e.Graphics); Render(e.Graphics); }
        protected virtual void Render(Graphics g) { }
        protected override void OnTextChanged(EventArgs e) { base.OnTextChanged(e); Invalidate(); }
        protected override void OnEnabledChanged(EventArgs e) { base.OnEnabledChanged(e); Invalidate(); }
    }

    /// <summary>A transparent container (a tab page) that forwards backdrop requests to its parent.</summary>
    class Page : Panel, IBackdrop
    {
        public Page()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint, true);
        }
        protected override void OnPaintBackground(PaintEventArgs e) { Backdrop.Draw(this, e.Graphics, ClientRectangle); }
        public void DrawBackdrop(Graphics g, Rectangle src, Point dest)
        {
            var p = Parent as IBackdrop;
            if (p == null) { using (var b = new SolidBrush(Theme.Bg)) g.FillRectangle(b, new Rectangle(dest, src.Size)); return; }
            src.Offset(Left, Top);
            p.DrawBackdrop(g, src, dest);
        }
    }

    /// <summary>A frosted dark card with an optional small caps heading ("THIS PC").</summary>
    class GlassPanel : Panel, IBackdrop
    {
        Bitmap cache;
        string heading;
        public GlassPanel(string heading)
        {
            this.heading = heading;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
        }
        public string Heading { get { return heading; } set { heading = value; Rebuild(); Invalidate(); } }
        public void Rebuild() { if (cache != null) { cache.Dispose(); cache = null; } }
        protected override void OnSizeChanged(EventArgs e) { base.OnSizeChanged(e); Rebuild(); }
        protected override void OnLocationChanged(EventArgs e) { base.OnLocationChanged(e); Rebuild(); }

        /// <summary>Corner radius; sheets use a large one.</summary>
        public int Radius = 22;
        /// <summary>Shows the small grab handle at the top centre (click it to lower the sheet).</summary>
        public bool ShowHandle = true;
        public event EventHandler HandleClicked;

        void Ensure()
        {
            if (cache != null || Width <= 0 || Height <= 0) return;
            cache = new Bitmap(Width, Height);
            using (var behind = new Bitmap(Width, Height))
            {
                using (var bg = Graphics.FromImage(behind)) Backdrop.Draw(this, bg, new Rectangle(0, 0, Width, Height));
                using (var g = Graphics.FromImage(cache))
                {
                    Theme.Hq(g);
                    g.DrawImageUnscaled(behind, 0, 0);
                    var r = new RectangleF(0.5f, 0.5f, Width - 1.5f, Height - 1.5f);
                    Frost.Pane(g, behind, r, Theme.S(Radius), Color.FromArgb(208, 20, 21, 29), Color.FromArgb(222, 11, 12, 18));
                    if (ShowHandle)
                        using (var p = Theme.Round(new RectangleF(Width / 2f - Theme.S(20), Theme.S(7), Theme.S(40), Theme.S(4)), Theme.S(2)))
                        using (var b = new SolidBrush(Color.FromArgb(90, 255, 255, 255))) g.FillPath(b, p);
                    if (!string.IsNullOrEmpty(heading))
                        using (var b = new SolidBrush(Color.FromArgb(246, 247, 250)))
                            g.DrawString(heading, Theme.F("ui", 17, FontStyle.Bold), b, Theme.S(22), Theme.S(14));
                }
            }
        }

        protected override void OnMouseClick(MouseEventArgs e)
        {
            if (ShowHandle && HandleClicked != null && e.Y < Theme.S(18) && Math.Abs(e.X - Width / 2) < Theme.S(40)) HandleClicked(this, EventArgs.Empty);
            base.OnMouseClick(e);
        }
        protected override void OnMouseMove(MouseEventArgs e)
        {
            Cursor = ShowHandle && HandleClicked != null && e.Y < Theme.S(18) && Math.Abs(e.X - Width / 2) < Theme.S(40) ? Cursors.Hand : Cursors.Default;
            base.OnMouseMove(e);
        }

        public static string Spaced(string s)
        {
            var sb = new System.Text.StringBuilder();
            foreach (char c in s.ToUpperInvariant()) { if (sb.Length > 0) sb.Append('\u200A'); sb.Append(c); }
            return sb.ToString();
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            Ensure();
            if (cache != null) e.Graphics.DrawImageUnscaled(cache, 0, 0);
        }
        public void DrawBackdrop(Graphics g, Rectangle src, Point dest)
        {
            Ensure();
            if (cache == null) return;
            g.DrawImage(cache, new Rectangle(dest, src.Size), src, GraphicsUnit.Pixel);
        }
    }

    /// <summary>A scrolling area inside a glass panel: the glass stays put while rows scroll over it.</summary>
    class ScrollArea : Panel, IBackdrop
    {
        public ScrollArea()
        {
            AutoScroll = true;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint, true);
        }
        protected override void OnPaintBackground(PaintEventArgs e) { Backdrop.Draw(this, e.Graphics, ClientRectangle); }
        public void DrawBackdrop(Graphics g, Rectangle src, Point dest)
        {
            var p = Parent as IBackdrop;
            if (p == null) { using (var b = new SolidBrush(Theme.ScrollBg)) g.FillRectangle(b, new Rectangle(dest, src.Size)); return; }
            src.Offset(Left, Top);
            p.DrawBackdrop(g, src, dest);
        }
        protected override void OnScroll(ScrollEventArgs se) { base.OnScroll(se); Invalidate(true); }
        protected override void OnMouseWheel(MouseEventArgs e) { base.OnMouseWheel(e); Invalidate(true); }
    }

    class TextLabel : Skin
    {
        public Font TextFont;
        public StringAlignment Align = StringAlignment.Near;
        public bool Wrap = true;
        public bool Shadow;
        public TextLabel(string text, Font font, Color color)
        {
            Text = text; TextFont = font; ForeColor = color;
        }
        public int FitHeight() { Height = Math.Max(Theme.S(4), Theme.Measure(Text, TextFont, Width) + Theme.S(2)); return Height; }
        protected override void Render(Graphics g)
        {
            using (var sf = new StringFormat())
            {
                sf.Alignment = Align;
                sf.Trimming = StringTrimming.EllipsisCharacter;
                if (!Wrap) sf.FormatFlags = StringFormatFlags.NoWrap;
                var r = new RectangleF(0, 0, Width, Height);
                if (Shadow)
                {
                    using (var sb = new SolidBrush(Color.FromArgb(200, Theme.Red)))
                        g.DrawString(Text, TextFont, sb, new RectangleF(Theme.S(3), Theme.S(3), Width, Height), sf);
                }
                using (var b = new SolidBrush(Enabled ? ForeColor : Theme.Dim)) g.DrawString(Text, TextFont, b, r, sf);
            }
        }
    }

    enum ButtonKind { Primary, Secondary, Ghost, Danger }

    class AccentButton : Skin
    {
        public ButtonKind Kind;
        public string Glyph;     // drawn left of the text: "play", "stop", "folder", "refresh", "download", "revert", "build"
        public Font TextFont;
        bool hover, down;
        public AccentButton(string text, ButtonKind kind)
        {
            Text = text; Kind = kind; Cursor = Cursors.Hand;
            TextFont = kind == ButtonKind.Primary ? Theme.Display(30) : Theme.UiBold(12);
            TabStop = true;
        }
        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; down = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseDown(MouseEventArgs e) { down = true; Invalidate(); base.OnMouseDown(e); }
        protected override void OnMouseUp(MouseEventArgs e) { down = false; Invalidate(); base.OnMouseUp(e); }
        protected override void OnClick(EventArgs e) { if (Enabled) base.OnClick(e); }

        /// <summary>The cinematic hero pill: PLAY VR with a soft red glow and a white play disc.</summary>
        public bool Hero;

        protected override void Render(Graphics g)
        {
            if (Hero) { RenderHero(g); return; }
            int depth = Kind == ButtonKind.Primary ? Theme.S(6) : 0;
            int press = (down && Enabled) ? depth / 2 : 0;
            var face = new RectangleF(0.5f, press + 0.5f, Width - 1.5f, Height - depth - 1.5f);
            float radius = Kind == ButtonKind.Primary ? Theme.S(8) : Math.Min(face.Height / 2f, Theme.S(18));
            if (Kind == ButtonKind.Primary)
            {
                var under = new RectangleF(0.5f, depth + 0.5f, Width - 1.5f, Height - depth - 1.5f);
                using (var p = Theme.Round(under, radius)) using (var b = new SolidBrush(Enabled ? Theme.RedDark : Color.FromArgb(40, 42, 52))) g.FillPath(b, p);
                using (var p = Theme.Round(face, radius))
                {
                    Color top = Enabled ? (hover ? Theme.RedHot : Theme.Red) : Color.FromArgb(62, 64, 76);
                    Color bottom = Enabled ? Color.FromArgb(196, 22, 38) : Color.FromArgb(50, 52, 62);
                    using (var b = new LinearGradientBrush(face, top, bottom, 90f)) g.FillPath(b, p);
                    using (var pen = new Pen(Color.FromArgb(Enabled ? 90 : 30, 255, 255, 255), 1f)) g.DrawPath(pen, p);
                }
            }
            else if (Kind == ButtonKind.Danger || Kind == ButtonKind.Secondary)
            {
                using (var p = Theme.Round(face, radius))
                {
                    Color fill = Kind == ButtonKind.Danger ? Color.FromArgb(hover ? 110 : 70, Theme.Red) : Color.FromArgb(hover ? 54 : 30, 230, 234, 242);
                    if (!Enabled) fill = Color.FromArgb(16, 200, 200, 210);
                    using (var b = new SolidBrush(fill)) g.FillPath(b, p);
                    Color edge = Kind == ButtonKind.Danger ? Color.FromArgb(hover ? 255 : 200, Theme.Red) : Color.FromArgb(hover ? 120 : 52, 255, 255, 255);
                    if (!Enabled) edge = Color.FromArgb(34, 255, 255, 255);
                    using (var pen = new Pen(edge, 1f)) g.DrawPath(pen, p);
                }
            }
            Color fg = Enabled ? (Kind == ButtonKind.Ghost ? (hover ? Theme.RedHot : Theme.Red) : Theme.Text) : Theme.Dim;
            string label = Kind == ButtonKind.Primary ? Text.ToUpperInvariant() : Text;
            SizeF ts = g.MeasureString(label, TextFont);
            float glyphW = string.IsNullOrEmpty(Glyph) ? 0 : (Kind == ButtonKind.Primary ? Theme.S(30) : Theme.S(18));
            float total = ts.Width + glyphW;
            float x = Kind == ButtonKind.Ghost ? 0 : (Width - total) / 2f;
            float cy = face.Y + face.Height / 2f;
            if (glyphW > 0) DrawGlyph(g, Glyph, new RectangleF(x, cy - glyphW / 2f + Theme.S(1), glyphW * 0.72f, glyphW * 0.72f), fg);
            using (var b = new SolidBrush(fg)) g.DrawString(label, TextFont, b, x + glyphW, cy - ts.Height / 2f);
        }

        void RenderHero(Graphics g)
        {
            int glow = Theme.S(10);
            int press = down && Enabled ? Theme.S(2) : 0;
            var face = new RectangleF(glow, glow + press, Width - glow * 2, Height - glow * 2);
            float rad = face.Height / 2f;
            if (Enabled)
                for (int i = glow; i > 0; i -= Math.Max(1, Theme.S(2)))
                    using (var p = Theme.Round(new RectangleF(face.X - i, face.Y - i + Theme.S(3), face.Width + i * 2, face.Height + i * 2), rad + i))
                    using (var b = new SolidBrush(Color.FromArgb((hover ? 26 : 18) * (glow - i + 1) / glow, 255, 40, 60))) g.FillPath(b, p);
            using (var p = Theme.Round(face, rad))
            {
                Color top = Enabled ? (hover ? Color.FromArgb(255, 92, 104) : Color.FromArgb(250, 66, 80)) : Color.FromArgb(70, 72, 84);
                Color bottom = Enabled ? Color.FromArgb(200, 18, 38) : Color.FromArgb(52, 54, 64);
                using (var b = new LinearGradientBrush(new RectangleF(face.X, face.Y - 1, face.Width, face.Height + 2), top, bottom, 90f)) g.FillPath(b, p);
                // Glassy highlight on the upper half.
                var hi = new RectangleF(face.X + Theme.S(6), face.Y + Theme.S(3), face.Width - Theme.S(12), face.Height * 0.45f);
                using (var hp = Theme.Round(hi, hi.Height / 2f))
                using (var b = new LinearGradientBrush(new RectangleF(hi.X, hi.Y - 1, hi.Width, hi.Height + 2), Color.FromArgb(Enabled ? 70 : 20, 255, 255, 255), Color.FromArgb(0, 255, 255, 255), 90f))
                    g.FillPath(b, hp);
                using (var pen = new Pen(Color.FromArgb(Enabled ? 110 : 40, 255, 255, 255), 1f)) g.DrawPath(pen, p);
            }
            float d = face.Height - Theme.S(22);
            var disc = new RectangleF(face.X + Theme.S(11), face.Y + Theme.S(11), d, d);
            using (var b = new SolidBrush(Enabled ? Color.White : Color.FromArgb(150, 152, 160))) g.FillEllipse(b, disc);
            using (var b = new SolidBrush(Enabled ? Color.FromArgb(220, 26, 44) : Color.FromArgb(70, 72, 84)))
                g.FillPolygon(b, new[] { new PointF(disc.X + d * .38f, disc.Y + d * .28f), new PointF(disc.X + d * .76f, disc.Y + d * .5f), new PointF(disc.X + d * .38f, disc.Y + d * .72f) });
            string label = Text.ToUpperInvariant();
            var ts = g.MeasureString(label, TextFont);
            float left = disc.Right + Theme.S(8);
            float tx = left + (face.Right - Theme.S(14) - left - ts.Width) / 2f;
            using (var b = new SolidBrush(Enabled ? Color.White : Color.FromArgb(170, 172, 180)))
                g.DrawString(label, TextFont, b, tx, face.Y + (face.Height - ts.Height) / 2f + Theme.S(1));
        }

        public static void DrawGlyph(Graphics g, string glyph, RectangleF r, Color c)
        {
            using (var pen = new Pen(c, Math.Max(1.5f, r.Width / 9f)))
            using (var b = new SolidBrush(c))
            {
                pen.StartCap = pen.EndCap = LineCap.Round; pen.LineJoin = LineJoin.Round;
                float x = r.X, y = r.Y, w = r.Width, h = r.Height;
                switch (glyph)
                {
                    case "play":
                        g.DrawPolygon(pen, new[] { new PointF(x + w * .15f, y), new PointF(x + w, y + h / 2), new PointF(x + w * .15f, y + h) }); break;
                    case "stop": g.DrawRectangle(pen, x + w * .1f, y + h * .1f, w * .8f, h * .8f); break;
                    case "folder":
                        g.DrawLines(pen, new[] { new PointF(x, y + h * .15f), new PointF(x + w * .4f, y + h * .15f), new PointF(x + w * .5f, y + h * .3f),
                            new PointF(x + w, y + h * .3f), new PointF(x + w, y + h * .9f), new PointF(x, y + h * .9f), new PointF(x, y + h * .15f) }); break;
                    case "refresh":
                        g.DrawArc(pen, x, y, w, h, 30, 290);
                        g.FillPolygon(b, new[] { new PointF(x + w, y + h * .05f), new PointF(x + w, y + h * .45f), new PointF(x + w * .62f, y + h * .3f) }); break;
                    case "revert":
                        g.DrawArc(pen, x, y, w, h, 220, 290);
                        g.FillPolygon(b, new[] { new PointF(x - w * .05f, y + h * .05f), new PointF(x + w * .35f, y + h * .1f), new PointF(x, y + h * .45f) }); break;
                    case "download":
                        g.DrawLine(pen, x + w / 2, y, x + w / 2, y + h * .65f);
                        g.DrawLines(pen, new[] { new PointF(x + w * .2f, y + h * .4f), new PointF(x + w / 2, y + h * .7f), new PointF(x + w * .8f, y + h * .4f) });
                        g.DrawLine(pen, x, y + h, x + w, y + h); break;
                    case "build":
                        g.DrawEllipse(pen, x + w * .25f, y + h * .25f, w * .5f, h * .5f);
                        for (int i = 0; i < 8; i++)
                        {
                            double a = i * Math.PI / 4;
                            g.DrawLine(pen, x + w / 2 + (float)Math.Cos(a) * w * .36f, y + h / 2 + (float)Math.Sin(a) * h * .36f,
                                       x + w / 2 + (float)Math.Cos(a) * w * .5f, y + h / 2 + (float)Math.Sin(a) * h * .5f);
                        }
                        break;
                    case "zip":
                        g.DrawRectangle(pen, x + w * .15f, y, w * .7f, h);
                        for (int i = 0; i < 3; i++) g.DrawLine(pen, x + w * .45f, y + h * (.2f + .2f * i), x + w * .55f, y + h * (.2f + .2f * i)); break;
                    case "monitor":
                        g.DrawRectangle(pen, x, y + h * .05f, w, h * .65f);
                        g.DrawLine(pen, x + w * .3f, y + h, x + w * .7f, y + h); break;
                }
            }
        }
    }

    /// <summary>The spider web logo in a red disc.</summary>
    class Logo : Skin
    {
        protected override void Render(Graphics g)
        {
            var r = new RectangleF(1, 1, Width - 3, Height - 3);
            using (var b = new LinearGradientBrush(r, Theme.RedHot, Color.FromArgb(170, 16, 30), 90f)) g.FillEllipse(b, r);
            using (var pen = new Pen(Color.FromArgb(120, 255, 255, 255), 1f)) g.DrawEllipse(pen, r);
            DrawWeb(g, new PointF(Width / 2f, Height / 2f), Width * 0.34f, Color.White, Math.Max(1.2f, Width / 34f));
        }
        public static void DrawWeb(Graphics g, PointF c, float radius, Color color, float width)
        {
            using (var pen = new Pen(color, width))
            {
                int spokes = 8;
                for (int i = 0; i < spokes; i++)
                {
                    double a = i * Math.PI * 2 / spokes + Math.PI / 8;
                    g.DrawLine(pen, c.X, c.Y, c.X + (float)Math.Cos(a) * radius, c.Y + (float)Math.Sin(a) * radius);
                }
                for (int ring = 1; ring <= 3; ring++)
                {
                    float rr = radius * ring / 3f;
                    var pts = new List<PointF>();
                    for (int i = 0; i <= spokes; i++)
                    {
                        double a = i * Math.PI * 2 / spokes + Math.PI / 8;
                        pts.Add(new PointF(c.X + (float)Math.Cos(a) * rr, c.Y + (float)Math.Sin(a) * rr));
                    }
                    for (int i = 0; i < spokes; i++)
                    {
                        var a = pts[i]; var b2 = pts[i + 1];
                        var mid = new PointF((a.X + b2.X) / 2f, (a.Y + b2.Y) / 2f);
                        var sag = new PointF(c.X + (mid.X - c.X) * 0.86f, c.Y + (mid.Y - c.Y) * 0.86f);
                        g.DrawBezier(pen, a, sag, sag, b2);
                    }
                }
            }
        }
    }

    /// <summary>The tabs under the title.</summary>
    class TabStrip : Skin
    {
        public readonly List<string> Tabs = new List<string>();
        public int Selected;
        public readonly Dictionary<int, string> Badges = new Dictionary<int, string>();
        public event EventHandler SelectedChanged;
        int hover = -1;
        List<RectangleF> rects = new List<RectangleF>();
        public TabStrip() { Cursor = Cursors.Hand; }
        new void Layout(Graphics g)
        {
            rects.Clear();
            float x = 0;
            foreach (var t in Tabs)
            {
                var sz = g.MeasureString(GlassPanel.Spaced(t), Theme.UiBold(14));
                rects.Add(new RectangleF(x, 0, sz.Width + Theme.S(36), Height));
                x += sz.Width + Theme.S(36);
            }
        }
        protected override void Render(Graphics g)
        {
            Layout(g);
            using (var pen = new Pen(Color.FromArgb(40, 255, 255, 255))) g.DrawLine(pen, 0, Height - 1, Width, Height - 1);
            for (int i = 0; i < Tabs.Count; i++)
            {
                var r = rects[i];
                bool sel = i == Selected;
                Color c = sel ? Theme.Text : (i == hover ? Theme.Silver : Theme.Sub);
                string label = GlassPanel.Spaced(Tabs[i]);
                var sz = g.MeasureString(label, Theme.UiBold(14));
                float tx = r.X + Theme.S(4);
                using (var b = new SolidBrush(c)) g.DrawString(label, Theme.UiBold(14), b, tx, (Height - sz.Height) / 2f - Theme.S(2));
                if (sel) using (var b = new SolidBrush(Theme.Red)) g.FillRectangle(b, tx, Height - Theme.S(4), sz.Width - Theme.S(4), Theme.S(3));
                string badge;
                if (Badges.TryGetValue(i, out badge) && !string.IsNullOrEmpty(badge))
                {
                    var bs = g.MeasureString(badge, Theme.UiBold(9));
                    var br = new RectangleF(tx + sz.Width - Theme.S(2), Theme.S(4), bs.Width + Theme.S(6), bs.Height);
                    using (var p = Theme.Round(br, br.Height / 2)) using (var b = new SolidBrush(Theme.Red)) g.FillPath(b, p);
                    using (var b = new SolidBrush(Color.White)) g.DrawString(badge, Theme.UiBold(9), b, br.X + Theme.S(3), br.Y);
                }
            }
        }
        int Hit(Point p) { for (int i = 0; i < rects.Count; i++) if (rects[i].Contains(p)) return i; return -1; }
        protected override void OnMouseMove(MouseEventArgs e) { int h = Hit(e.Location); if (h != hover) { hover = h; Invalidate(); } base.OnMouseMove(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = -1; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseClick(MouseEventArgs e)
        {
            int h = Hit(e.Location);
            if (h >= 0 && h != Selected) { Selected = h; Invalidate(); if (SelectedChanged != null) SelectedChanged(this, EventArgs.Empty); }
            base.OnMouseClick(e);
        }
        public void Select(int i) { if (i == Selected) return; Selected = i; Invalidate(); if (SelectedChanged != null) SelectedChanged(this, EventArgs.Empty); }
    }

    /// <summary>A toggle with a title and a one-line subtext.</summary>
    class ToggleRow : Skin
    {
        public string Title, Sub;
        bool on;
        public string DisabledReason;
        public event EventHandler Changed;
        public ToggleRow(string title, string sub, bool value) { Title = title; Sub = sub; on = value; Cursor = Cursors.Hand; }
        public bool Checked { get { return on; } set { if (on == value) return; on = value; Invalidate(); if (Changed != null) Changed(this, EventArgs.Empty); } }
        protected override void OnClick(EventArgs e) { if (Enabled) Checked = !Checked; base.OnClick(e); }
        protected override void Render(Graphics g)
        {
            int sw = Theme.S(44), sh = Theme.S(24);
            var track = new RectangleF(Width - sw - Theme.S(4), Theme.S(8), sw, sh);
            bool active = on && Enabled;
            using (var p = Theme.Round(track, sh / 2f))
            {
                using (var b = new SolidBrush(active ? Theme.Red : Color.FromArgb(Enabled ? 60 : 25, 200, 205, 215))) g.FillPath(b, p);
                using (var pen = new Pen(Color.FromArgb(70, 255, 255, 255))) g.DrawPath(pen, p);
            }
            float k = sh - Theme.S(6);
            float kx = on ? track.Right - k - Theme.S(3) : track.X + Theme.S(3);
            using (var b = new SolidBrush(Enabled ? Color.White : Theme.Dim)) g.FillEllipse(b, kx, track.Y + Theme.S(3), k, k);
            int textW = Width - sw - Theme.S(16);
            using (var b = new SolidBrush(Enabled ? Theme.Text : Theme.Dim))
                g.DrawString(Title, Theme.UiBold(13), b, new RectangleF(0, Theme.S(4), textW, Theme.S(20)));
            string sub = Enabled || string.IsNullOrEmpty(DisabledReason) ? Sub : DisabledReason;
            using (var b = new SolidBrush(Enabled ? Theme.Sub : Theme.Dim))
                g.DrawString(sub, Theme.Ui(11.5f), b, new RectangleF(0, Theme.S(24), textW, Height - Theme.S(24)));
        }
    }

    /// <summary>A slider with title, subtext and value readout.</summary>
    class SliderRow : Skin
    {
        public string Title, Sub, Suffix;
        public int Min, Max, Step;
        int value;
        public string DisabledReason;
        public Func<int, string> Format;
        public event EventHandler Changed;
        bool dragging;
        public SliderRow(string title, string sub, int min, int max, int step, int val, string suffix)
        {
            Title = title; Sub = sub; Min = min; Max = max; Step = Math.Max(1, step); Suffix = suffix; value = Clamp(val);
            Cursor = Cursors.Hand; TabStop = true;
        }
        int Clamp(int v) { v = Math.Max(Min, Math.Min(Max, v)); return Min + (int)Math.Round((v - Min) / (double)Step) * Step; }
        public int Value { get { return value; } set { int v = Clamp(value); if (v == this.value) return; this.value = v; Invalidate(); if (Changed != null) Changed(this, EventArgs.Empty); } }
        RectangleF Track { get { return new RectangleF(Theme.S(2), Theme.S(50), Width - Theme.S(80), Theme.S(4)); } }
        protected override void Render(Graphics g)
        {
            using (var b = new SolidBrush(Enabled ? Theme.Text : Theme.Dim)) g.DrawString(Title, Theme.UiBold(13), b, 0, Theme.S(4));
            string sub = Enabled || string.IsNullOrEmpty(DisabledReason) ? Sub : DisabledReason;
            using (var b = new SolidBrush(Enabled ? Theme.Sub : Theme.Dim))
                g.DrawString(sub, Theme.Ui(11.5f), b, new RectangleF(0, Theme.S(24), Width, Theme.S(18)));
            var t = Track;
            using (var p = Theme.Round(t, t.Height / 2)) using (var b = new SolidBrush(Color.FromArgb(60, 200, 205, 215))) g.FillPath(b, p);
            float f = Max == Min ? 0 : (value - Min) / (float)(Max - Min);
            var fill = new RectangleF(t.X, t.Y, Math.Max(t.Height, t.Width * f), t.Height);
            using (var p = Theme.Round(fill, t.Height / 2)) using (var b = new SolidBrush(Enabled ? Theme.Red : Theme.Dim)) g.FillPath(b, p);
            float kr = Theme.S(8);
            float kx = t.X + t.Width * f;
            using (var b = new SolidBrush(Enabled ? Color.White : Theme.Dim)) g.FillEllipse(b, kx - kr, t.Y + t.Height / 2 - kr, kr * 2, kr * 2);
            using (var pen = new Pen(Enabled ? Theme.Red : Theme.Dim, Theme.S(2))) g.DrawEllipse(pen, kx - kr, t.Y + t.Height / 2 - kr, kr * 2, kr * 2);
            string txt = Format != null ? Format(value) : value + Suffix;
            var vr = new RectangleF(Width - Theme.S(68), Theme.S(40), Theme.S(66), Theme.S(24));
            using (var p = Theme.Round(vr, Theme.S(5))) using (var b = new SolidBrush(Theme.Field)) g.FillPath(b, p);
            using (var sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center })
            using (var b = new SolidBrush(Enabled ? Theme.Silver : Theme.Dim)) g.DrawString(txt, Theme.UiBold(12), b, vr, sf);
        }
        void SetFromX(int x)
        {
            var t = Track;
            float f = Math.Max(0, Math.Min(1, (x - t.X) / t.Width));
            Value = Min + (int)Math.Round(f * (Max - Min));
        }
        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (Enabled && e.Y > Theme.S(36) && e.X < Width - Theme.S(72)) { dragging = true; Capture = true; SetFromX(e.X); }
            Focus(); base.OnMouseDown(e);
        }
        protected override void OnMouseMove(MouseEventArgs e) { if (dragging) SetFromX(e.X); base.OnMouseMove(e); }
        protected override void OnMouseUp(MouseEventArgs e) { dragging = false; Capture = false; base.OnMouseUp(e); }
        protected override bool IsInputKey(Keys k) { return k == Keys.Left || k == Keys.Right || base.IsInputKey(k); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.KeyCode == Keys.Left) Value = value - Step;
            if (e.KeyCode == Keys.Right) Value = value + Step;
            base.OnKeyDown(e);
        }
    }

    /// <summary>A segmented choice (Small | Medium | Large) with title and subtext.</summary>
    class ChoiceRow : Skin
    {
        public string Title, Sub;
        public string[] Choices;
        public string[] Labels;
        int index;
        public string DisabledReason;
        public event EventHandler Changed;
        public ChoiceRow(string title, string sub, string[] choices, string[] labels, string value)
        {
            Title = title; Sub = sub; Choices = choices; Labels = labels ?? choices; Cursor = Cursors.Hand;
            index = Math.Max(0, Array.IndexOf(choices, value));
        }
        public string Value
        {
            get { return Choices[index]; }
            set { int i = Array.IndexOf(Choices, value); if (i < 0 || i == index) return; index = i; Invalidate(); if (Changed != null) Changed(this, EventArgs.Empty); }
        }
        RectangleF Seg(int i)
        {
            float w = (Width - Theme.S(2)) / (float)Choices.Length;
            return new RectangleF(Theme.S(1) + w * i, Theme.S(42), w, Theme.S(24));
        }
        protected override void Render(Graphics g)
        {
            using (var b = new SolidBrush(Enabled ? Theme.Text : Theme.Dim)) g.DrawString(Title, Theme.UiBold(13), b, 0, Theme.S(4));
            string sub = Enabled || string.IsNullOrEmpty(DisabledReason) ? Sub : DisabledReason;
            using (var b = new SolidBrush(Enabled ? Theme.Sub : Theme.Dim))
                g.DrawString(sub, Theme.Ui(11.5f), b, new RectangleF(0, Theme.S(24), Width, Theme.S(18)));
            var all = new RectangleF(Theme.S(1), Theme.S(42), Width - Theme.S(2), Theme.S(24));
            using (var p = Theme.Round(all, Theme.S(5))) using (var b = new SolidBrush(Theme.Field)) g.FillPath(b, p);
            using (var sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center })
                for (int i = 0; i < Choices.Length; i++)
                {
                    var r = Seg(i);
                    if (i == index)
                        using (var p = Theme.Round(new RectangleF(r.X + 2, r.Y + 2, r.Width - 4, r.Height - 4), Theme.S(4)))
                        using (var b = new SolidBrush(Enabled ? Theme.Red : Theme.Dim)) g.FillPath(b, p);
                    using (var b = new SolidBrush(i == index ? Color.White : (Enabled ? Theme.Sub : Theme.Dim)))
                        g.DrawString(Labels[i], Theme.UiBold(11), b, r, sf);
                }
        }
        protected override void OnMouseClick(MouseEventArgs e)
        {
            if (Enabled) for (int i = 0; i < Choices.Length; i++) if (Seg(i).Contains(e.Location)) Value = Choices[i];
            base.OnMouseClick(e);
        }
    }

    /// <summary>A status check: icon, title, detail, optional action button on the right.</summary>
    class StatusRow : Skin
    {
        public string Title, Detail;
        public CheckState State = CheckState.Info;
        public AccentButton Action;
        public bool Separator = true;
        public StatusRow(string title) { Title = title; Detail = ""; }
        public void Set(CheckState s, string detail) { State = s; Detail = detail ?? ""; Invalidate(); }
        public void AddAction(AccentButton b)
        {
            Action = b; Controls.Add(b);
            b.SetBounds(Width - Theme.S(122), Theme.S(10), Theme.S(118), Theme.S(30));
            b.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        }
        protected override void Render(Graphics g)
        {
            float d = Theme.S(22);
            var ir = new RectangleF(Theme.S(2), Theme.S(8), d, d);
            Color c = Theme.StateColor(State);
            using (var b = new SolidBrush(Color.FromArgb(46, c))) g.FillEllipse(b, ir);
            using (var pen = new Pen(c, Math.Max(1.6f, Theme.S(2))))
            {
                pen.StartCap = pen.EndCap = LineCap.Round;
                float x = ir.X, y = ir.Y;
                switch (State)
                {
                    case CheckState.Ok:
                        g.DrawLines(pen, new[] { new PointF(x + d * .28f, y + d * .52f), new PointF(x + d * .45f, y + d * .68f), new PointF(x + d * .74f, y + d * .34f) }); break;
                    case CheckState.Bad:
                        g.DrawLine(pen, x + d * .32f, y + d * .32f, x + d * .68f, y + d * .68f); g.DrawLine(pen, x + d * .68f, y + d * .32f, x + d * .32f, y + d * .68f); break;
                    case CheckState.Warn:
                        g.DrawLine(pen, x + d / 2, y + d * .26f, x + d / 2, y + d * .56f); g.DrawLine(pen, x + d / 2, y + d * .73f, x + d / 2, y + d * .74f); break;
                    case CheckState.Busy:
                        g.DrawArc(pen, x + d * .25f, y + d * .25f, d * .5f, d * .5f, (Environment.TickCount / 3) % 360, 260); break;
                    default:
                        g.DrawLine(pen, x + d / 2, y + d * .44f, x + d / 2, y + d * .74f); g.DrawLine(pen, x + d / 2, y + d * .27f, x + d / 2, y + d * .28f); break;
                }
            }
            int right = Action != null && Action.Visible ? Theme.S(132) : Theme.S(4);
            float tx = Theme.S(36);
            using (var b = new SolidBrush(Theme.Text)) g.DrawString(Title.ToUpperInvariant(), Theme.UiBold(12.5f), b, tx, Theme.S(6));
            using (var b = new SolidBrush(Theme.Sub))
                g.DrawString(Detail, Theme.Ui(11.5f), b, new RectangleF(tx, Theme.S(25), Width - tx - right, Height - Theme.S(28)));
            if (Separator) using (var pen = new Pen(Color.FromArgb(28, 255, 255, 255))) g.DrawLine(pen, tx, Height - 1, Width, Height - 1);
        }
    }

    class ProgressStrip : Skin
    {
        double value; bool indeterminate;
        Timer anim;
        public ProgressStrip()
        {
            anim = new Timer { Interval = 40 };
            anim.Tick += delegate { if (indeterminate) Invalidate(); };
        }
        public void Set(double v) { value = Math.Max(0, Math.Min(1, v)); indeterminate = false; anim.Stop(); Invalidate(); }
        public void Busy() { indeterminate = true; anim.Start(); Invalidate(); }
        protected override void Render(Graphics g)
        {
            var r = new RectangleF(0, 0, Width - 1, Height - 1);
            using (var p = Theme.Round(r, Height / 2f)) using (var b = new SolidBrush(Color.FromArgb(50, 200, 205, 215))) g.FillPath(b, p);
            RectangleF f;
            if (indeterminate)
            {
                float w = Width * 0.25f;
                float x = (Environment.TickCount / 3 % (Width + (int)w)) - w;
                f = new RectangleF(Math.Max(0, x), 0, Math.Min(w, Width - Math.Max(0, x)), Height - 1);
            }
            else f = new RectangleF(0, 0, (float)((Width - 1) * value), Height - 1);
            if (f.Width > 1)
                using (var p = Theme.Round(f, Height / 2f))
                using (var b = new LinearGradientBrush(new RectangleF(0, 0, Width, Height), Theme.Red, Theme.Silver, 0f)) g.FillPath(b, p);
        }
        protected override void Dispose(bool disposing) { if (disposing) anim.Dispose(); base.Dispose(disposing); }
    }

    /// <summary>The session/build log: a dark read-only rich text box. Thread safe.</summary>
    class LogBox : Panel
    {
        readonly RichTextBox box;
        public string FilePath;
        const int MaxChars = 400000;
        public LogBox()
        {
            BackColor = Color.FromArgb(9, 10, 16);
            Padding = new Padding(Theme.S(10), Theme.S(8), Theme.S(4), Theme.S(8));
            box = new RichTextBox
            {
                Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None, BackColor = BackColor,
                ForeColor = Theme.Silver, Font = Theme.Mono(12), DetectUrls = false, WordWrap = true, HideSelection = false
            };
            Controls.Add(box);
        }
        public string AllText { get { return box.Text; } }
        public void Clear() { if (InvokeRequired) { BeginInvoke((Action)Clear); return; } box.Clear(); }
        public void Line(string text) { Line(text, ColorFor(text)); }
        public void Line(string text, Color color)
        {
            if (text == null) return;
            if (IsDisposed) return;
            if (InvokeRequired) { try { BeginInvoke((Action<string, Color>)Line, text, color); } catch (InvalidOperationException) { } return; }
            if (box.TextLength > MaxChars) { box.Select(0, box.TextLength / 3); box.SelectedText = ""; }
            box.SelectionStart = box.TextLength;
            box.SelectionLength = 0;
            box.SelectionColor = color;
            box.AppendText(text + Environment.NewLine);
            box.SelectionStart = box.TextLength;
            box.ScrollToCaret();
            if (FilePath != null) { try { System.IO.File.AppendAllText(FilePath, text + Environment.NewLine); } catch { } }
        }
        public static Color ColorFor(string t)
        {
            string l = t.ToLowerInvariant();
            if (l.StartsWith("error") || l.Contains("failed") || l.Contains("vr stopped") || l.Contains("exception") || l.StartsWith("fail ")) return Theme.Bad;
            if (l.StartsWith("warning") || l.Contains("warning:")) return Theme.Warn;
            if (l.StartsWith("sm2 vr port:") || l.StartsWith("built ") || l.StartsWith("ok") || l.StartsWith("success") || l.StartsWith("done")) return Theme.Good;
            if (l.StartsWith(">")) return Theme.Info;
            return Theme.Silver;
        }
    }
}
