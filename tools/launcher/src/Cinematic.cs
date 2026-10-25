// SM2 VR Launcher - the cinematic look: frosted glass, the SM2 VR wordmark with its byline, the bottom action bar
// with slide-up sheets, and the round icon buttons. All owner-drawn GDI+ (no extra dependencies).
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    static class Frost
    {
        /// <summary>A cheap blur (GDI+ has none): shrink the picture a lot, then stretch it back smoothly.</summary>
        public static Bitmap Blur(Bitmap src, int factor)
        {
            int w = src.Width, h = src.Height;
            int sw = Math.Max(1, w / factor), sh = Math.Max(1, h / factor);
            var result = new Bitmap(w, h);
            using (var small = new Bitmap(sw, sh))
            using (var attr = new ImageAttributes())
            {
                attr.SetWrapMode(WrapMode.TileFlipXY);
                using (var g = Graphics.FromImage(small))
                {
                    g.InterpolationMode = InterpolationMode.HighQualityBilinear;
                    g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                    g.DrawImage(src, new Rectangle(0, 0, sw, sh), 0, 0, w, h, GraphicsUnit.Pixel, attr);
                }
                using (var g = Graphics.FromImage(result))
                {
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                    g.DrawImage(small, new Rectangle(0, 0, w, h), 0, 0, sw, sh, GraphicsUnit.Pixel, attr);
                }
            }
            return result;
        }

        /// <summary>Paints a frosted rounded pane: the blurred picture behind it, a dark tint, a hairline edge.</summary>
        public static void Pane(Graphics g, Bitmap behind, RectangleF r, float radius, Color tintTop, Color tintBottom)
        {
            using (var path = Theme.Round(r, radius))
            {
                if (behind != null)
                    using (var blurred = Blur(behind, 9))
                    {
                        var clip = g.Clip;
                        g.SetClip(path);
                        g.DrawImageUnscaled(blurred, 0, 0);
                        g.Clip = clip;
                    }
                using (var b = new LinearGradientBrush(new RectangleF(r.X, r.Y - 1, r.Width, r.Height + 2), tintTop, tintBottom, 90f)) g.FillPath(b, path);
                using (var pen = new Pen(Color.FromArgb(46, 255, 255, 255), 1f)) g.DrawPath(pen, path);
            }
            // A soft highlight along the top edge, like light catching glass.
            using (var b = new LinearGradientBrush(new RectangleF(r.X, r.Y, r.Width, 2), Color.FromArgb(0, 255, 255, 255), Color.FromArgb(70, 255, 255, 255), 0f))
            {
                var blend = new Blend { Positions = new[] { 0f, 0.5f, 1f }, Factors = new[] { 0f, 1f, 0f } };
                b.Blend = blend;
                g.FillRectangle(b, r.X + radius, r.Y + 0.5f, r.Width - radius * 2, 1);
            }
        }
    }

    /// <summary>The big "HERO VR" wordmark: thin silver HERO, heavy red VR, the "BY AGENT MULDER" byline and the subtitle.
    /// The thin word is letter-spaced (Tracking, in ems) and the whole mark shrinks to fit the control if it would be wider.</summary>
    class Wordmark : Skin
    {
        public string ThinWord = "HERO";
        public string HeavyWord = "VR";
        public float Tracking = 0.07f;
        public string Byline = "MADE BY AGENT MULDER";
        public string Subtitle = "MARVEL'S SPIDER-MAN 2  \u00B7  IN VR";
        public string Credit = "";   // on screen: "Made by Agent Mulder" only (credits: NOTICE, CREDITS.md, README)
        public float WordSize = 100;

        static GraphicsPath Word(string s, FontFamily fam, FontStyle style, float px)
        {
            var p = new GraphicsPath();
            using (var sf = StringFormat.GenericTypographic) p.AddString(s, fam, (int)style, px, PointF.Empty, sf);
            return p;
        }

        /// <summary>Like Word, but letter-spaced: each letter is placed after the previous one's ink plus a fixed gap
        /// (GDI+ has no tracking). A small kern pulls diagonal pairs like "AV" together so the spacing looks even.</summary>
        static GraphicsPath Tracked(string s, FontFamily fam, FontStyle style, float px, float trackEm)
        {
            var p = new GraphicsPath();
            float x = 0, gap = px * trackEm;
            char prev = '\0';
            using (var sf = StringFormat.GenericTypographic)
                foreach (char c in s)
                {
                    using (var lp = new GraphicsPath())
                    {
                        lp.AddString(c.ToString(), fam, (int)style, px, PointF.Empty, sf);
                        var b = lp.GetBounds();
                        if (prev != '\0' && "AVWY".IndexOf(prev) >= 0 && "AVWY".IndexOf(c) >= 0) x -= px * 0.10f;
                        using (var m = new Matrix()) { m.Translate(x - b.Left, 0); lp.Transform(m); }
                        p.AddPath(lp, false);
                        x += b.Width + gap;
                        prev = c;
                    }
                }
            return p;
        }

        protected override void Render(Graphics g)
        {
            float px = WordSize * Theme.Scale;
            Font thin = Theme.Thin(WordSize), heavy = Theme.Heavy(WordSize);
            using (var sm2 = Tracked(ThinWord, thin.FontFamily, thin.Style, px, Tracking))
            using (var vr = Word(HeavyWord, heavy.FontFamily, heavy.Style, px))
            {
                var a = sm2.GetBounds(); var b = vr.GetBounds();
                float gap = px * 0.16f;
                float total = a.Width + gap + b.Width;
                // Shrink the whole mark if it would not fit the control (other fonts, other scales).
                float room = Width - Theme.S(12);
                if (total > room)
                {
                    float k = room / total;
                    using (var m = new Matrix()) { m.Scale(k, k); sm2.Transform(m); vr.Transform(m); }
                    a = sm2.GetBounds(); b = vr.GetBounds(); gap *= k; total = a.Width + gap + b.Width;
                }
                float x0 = Width - total - Theme.S(6);
                float top = Math.Min(a.Top, b.Top);
                float capH = Math.Max(a.Bottom, b.Bottom) - top;
                using (var m = new Matrix()) { m.Translate(x0 - a.Left, Theme.S(4) - top); sm2.Transform(m); }
                using (var m = new Matrix()) { m.Translate(x0 + a.Width + gap - b.Left, Theme.S(4) - top); vr.Transform(m); }
                var ra = sm2.GetBounds(); var rb = vr.GetBounds();
                // Shadow first so the letters lift off the picture.
                using (var m = new Matrix()) using (var s1 = (GraphicsPath)sm2.Clone()) using (var s2 = (GraphicsPath)vr.Clone())
                {
                    m.Translate(0, Theme.S(5)); s1.Transform(m); s2.Transform(m);
                    using (var sb = new SolidBrush(Color.FromArgb(110, 0, 0, 0))) { g.FillPath(sb, s1); g.FillPath(sb, s2); }
                }
                using (var br = new LinearGradientBrush(new RectangleF(ra.X, ra.Y - 1, ra.Width, ra.Height + 2), Color.FromArgb(250, 251, 253), Color.FromArgb(150, 158, 172), 90f))
                {
                    br.InterpolationColors = new ColorBlend
                    {
                        Positions = new[] { 0f, 0.48f, 0.52f, 1f },
                        Colors = new[] { Color.FromArgb(252, 252, 255), Color.FromArgb(206, 211, 219), Color.FromArgb(176, 182, 194), Color.FromArgb(228, 231, 237) }
                    };
                    g.FillPath(br, sm2);
                }
                using (var br = new LinearGradientBrush(new RectangleF(rb.X, rb.Y - 1, rb.Width, rb.Height + 2), Color.FromArgb(255, 86, 98), Color.FromArgb(178, 14, 32), 90f))
                    g.FillPath(br, vr);
                using (var pen = new Pen(Color.FromArgb(70, 255, 255, 255), 1f)) g.DrawPath(pen, vr);

                // Byline, right-aligned under VR like a signature: "BY AGENT MULDER".
                float y = Theme.S(4) + capH + Theme.S(12);
                var bf = Theme.F("ui", 12, FontStyle.Bold);
                string by = Spread(Byline, 3);
                var bs = g.MeasureString(by, bf);
                float bx = rb.Right - bs.Width + Theme.S(4);
                using (var red = new SolidBrush(Theme.Red)) g.FillRectangle(red, bx - Theme.S(26), y + bs.Height / 2f - Theme.S(1), Theme.S(18), Math.Max(1, Theme.S(2)));
                using (var wb = new SolidBrush(Color.FromArgb(236, 238, 242))) g.DrawString(by, bf, wb, bx, y);

                // A thin red line that fades out, the subtitle and the Spidy credit.
                y += bs.Height + Theme.S(12);
                using (var lb = new LinearGradientBrush(new RectangleF(x0, y, total * 0.7f, 2), Color.FromArgb(220, Theme.Red), Color.FromArgb(0, Theme.Red), 0f))
                    g.FillRectangle(lb, x0 + Theme.S(2), y, total * 0.7f, Math.Max(1, Theme.S(1)));
                y += Theme.S(10);
                var sf2 = Theme.Ui(13);
                using (var sb = new SolidBrush(Theme.Silver)) g.DrawString(Spread(Subtitle, 2), sf2, sb, x0, y);
                y += g.MeasureString("Ag", sf2).Height + Theme.S(4);
                using (var sb = new SolidBrush(Theme.Sub)) g.DrawString(Credit, Theme.Ui(12.5f), sb, x0 + Theme.S(1), y);
            }
        }

        /// <summary>Letter-spacing for spaced caps: thin spaces between the letters.</summary>
        public static string Spread(string s, int n)
        {
            var sb = new System.Text.StringBuilder();
            string pad = new string('\u2009', Math.Max(1, n / 2));
            foreach (char c in s) { if (sb.Length > 0) sb.Append(pad); sb.Append(c); }
            return sb.ToString();
        }
    }

    /// <summary>A round glass icon button (TikTok, Discord, mod folder).</summary>
    class IconButton : Skin
    {
        public string Glyph;
        bool hover;
        public IconButton(string glyph) { Glyph = glyph; Cursor = Cursors.Hand; }
        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void Render(Graphics g)
        {
            var r = new RectangleF(0.5f, 0.5f, Width - 1.5f, Height - 1.5f);
            using (var b = new SolidBrush(hover ? Color.FromArgb(200, 40, 14, 20) : Color.FromArgb(150, 14, 16, 24))) g.FillEllipse(b, r);
            using (var pen = new Pen(hover ? Theme.Red : Color.FromArgb(70, 255, 255, 255), 1f)) g.DrawEllipse(pen, r);
            float s = Width * 0.42f;
            var ir = new RectangleF((Width - s) / 2f, (Height - s) / 2f, s, s);
            Draw(g, Glyph, ir, Color.FromArgb(240, 242, 246));
        }

        public static void Draw(Graphics g, string glyph, RectangleF r, Color c)
        {
            float x = r.X, y = r.Y, w = r.Width, h = r.Height;
            if (glyph == "tiktok")
            {
                // A musical note with a curled flag, the TikTok shape, in white with a hint of the brand's offset colours.
                Action<float, float, Color> note = (dx, dy, col) =>
                {
                    using (var pen = new Pen(col, w * 0.16f) { StartCap = LineCap.Round, EndCap = LineCap.Round })
                    {
                        g.DrawArc(pen, x + dx + w * 0.06f, y + dy + h * 0.50f, w * 0.40f, h * 0.40f, 300, 300);
                        g.DrawLine(pen, x + dx + w * 0.46f, y + dy + h * 0.70f, x + dx + w * 0.46f, y + dy + h * 0.04f);
                        g.DrawArc(pen, x + dx + w * 0.46f, y + dy - h * 0.20f, w * 0.44f, h * 0.44f, 180, -90);
                    }
                };
                note(-w * 0.05f, -h * 0.03f, Color.FromArgb(200, 37, 244, 238));
                note(w * 0.05f, h * 0.03f, Color.FromArgb(200, 254, 44, 85));
                note(0, 0, c);
            }
            else if (glyph == "discord")
            {
                // The Discord "Clyde" controller face.
                using (var p = new GraphicsPath())
                {
                    p.AddBezier(x + w * 0.20f, y + h * 0.18f, x + w * 0.40f, y + h * 0.08f, x + w * 0.60f, y + h * 0.08f, x + w * 0.80f, y + h * 0.18f);
                    p.AddBezier(x + w * 0.80f, y + h * 0.18f, x + w * 0.98f, y + h * 0.45f, x + w * 1.02f, y + h * 0.70f, x + w * 0.98f, y + h * 0.82f);
                    p.AddBezier(x + w * 0.98f, y + h * 0.82f, x + w * 0.88f, y + h * 0.92f, x + w * 0.76f, y + h * 0.96f, x + w * 0.70f, y + h * 0.96f);
                    p.AddLine(x + w * 0.70f, y + h * 0.96f, x + w * 0.64f, y + h * 0.84f);
                    p.AddBezier(x + w * 0.64f, y + h * 0.84f, x + w * 0.55f, y + h * 0.87f, x + w * 0.45f, y + h * 0.87f, x + w * 0.36f, y + h * 0.84f);
                    p.AddLine(x + w * 0.36f, y + h * 0.84f, x + w * 0.30f, y + h * 0.96f);
                    p.AddBezier(x + w * 0.30f, y + h * 0.96f, x + w * 0.24f, y + h * 0.96f, x + w * 0.12f, y + h * 0.92f, x + w * 0.02f, y + h * 0.82f);
                    p.AddBezier(x + w * 0.02f, y + h * 0.82f, x - w * 0.02f, y + h * 0.70f, x + w * 0.02f, y + h * 0.45f, x + w * 0.20f, y + h * 0.18f);
                    p.CloseFigure();
                    using (var b = new SolidBrush(c)) g.FillPath(b, p);
                }
                using (var b = new SolidBrush(Color.FromArgb(20, 22, 32)))
                {
                    g.FillEllipse(b, x + w * 0.24f, y + h * 0.44f, w * 0.17f, h * 0.20f);
                    g.FillEllipse(b, x + w * 0.59f, y + h * 0.44f, w * 0.17f, h * 0.20f);
                }
            }
            else AccentButton.DrawGlyph(g, glyph, r, c);
        }
    }

    /// <summary>The frosted bar along the bottom: section buttons left and right of the PLAY VR pill. A section opens its sheet;
    /// pressing it again (or Esc) lowers the sheet. Keeps the old tab strip's API (Select, Selected, Badges, SelectedChanged).</summary>
    class ActionBar : Skin
    {
        public class Item { public string Label, Glyph; public int Page = -1; public Action Click; public bool Right; public RectangleF Rect; }
        public readonly List<Item> Items = new List<Item>();
        public int Selected = -1;
        public readonly Dictionary<int, string> Badges = new Dictionary<int, string>();
        public event EventHandler SelectedChanged;
        public int ItemWidth = 84;
        int hover = -1;

        public Item Add(string label, string glyph, int page, Action click, bool right)
        {
            var it = new Item { Label = label, Glyph = glyph, Page = page, Click = click, Right = right };
            Items.Add(it);
            return it;
        }

        void LayoutItems()
        {
            float iw = Theme.S(ItemWidth), pad = Theme.S(18);
            float lx = pad, rx = Width - pad;
            var rights = new List<Item>();
            foreach (var it in Items)
            {
                if (it.Right) { rights.Add(it); continue; }
                it.Rect = new RectangleF(lx, 0, iw, Height); lx += iw;
            }
            for (int i = rights.Count - 1; i >= 0; i--) { rx -= iw; rights[i].Rect = new RectangleF(rx, 0, iw, Height); }
        }

        protected override void Render(Graphics g)
        {
            LayoutItems();
            for (int i = 0; i < Items.Count; i++)
            {
                var it = Items[i];
                bool sel = it.Page >= 0 && it.Page == Selected;
                Color c = sel ? Color.White : (i == hover ? Theme.Text : Color.FromArgb(178, 184, 196));
                float gs = Theme.S(18);
                var gr = new RectangleF(it.Rect.X + (it.Rect.Width - gs) / 2f, it.Rect.Y + Theme.S(15), gs, gs);
                DrawItemGlyph(g, it.Glyph, gr, c);
                using (var sf = new StringFormat { Alignment = StringAlignment.Center })
                using (var b = new SolidBrush(c))
                    g.DrawString(it.Label, sel ? Theme.UiBold(11.5f) : Theme.Ui(11.5f), b, new RectangleF(it.Rect.X, it.Rect.Y + Theme.S(41), it.Rect.Width, Theme.S(20)), sf);
                if (sel)
                    using (var p = Theme.Round(new RectangleF(it.Rect.X + it.Rect.Width / 2f - Theme.S(12), Theme.S(1), Theme.S(24), Theme.S(3)), Theme.S(1)))
                    using (var b = new SolidBrush(Theme.Red)) g.FillPath(b, p);
                string badge;
                if (it.Page >= 0 && Badges.TryGetValue(it.Page, out badge) && !string.IsNullOrEmpty(badge))
                {
                    float d = Theme.S(8);
                    using (var b = new SolidBrush(Theme.RedHot)) g.FillEllipse(b, gr.Right - d / 3f, gr.Y - d / 2f, d, d);
                    using (var pen = new Pen(Color.FromArgb(20, 22, 30), Math.Max(1, Theme.S(2)))) g.DrawEllipse(pen, gr.Right - d / 3f, gr.Y - d / 2f, d, d);
                }
            }
        }

        static void DrawItemGlyph(Graphics g, string glyph, RectangleF r, Color c)
        {
            float x = r.X, y = r.Y, w = r.Width, h = r.Height;
            using (var pen = new Pen(c, Math.Max(1.4f, w / 11f)) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round })
            {
                switch (glyph)
                {
                    case "status":
                        g.DrawEllipse(pen, x + 1, y + 1, w - 2, h - 2);
                        g.DrawLines(pen, new[] { new PointF(x + w * .3f, y + h * .52f), new PointF(x + w * .45f, y + h * .67f), new PointF(x + w * .72f, y + h * .36f) }); break;
                    case "options":
                        for (int i = 0; i < 3; i++)
                        {
                            float ly = y + h * (.2f + .3f * i);
                            g.DrawLine(pen, x + 1, ly, x + w - 1, ly);
                            float kx = x + w * (i == 1 ? .3f : .68f);
                            using (var b = new SolidBrush(Color.FromArgb(20, 22, 30))) g.FillEllipse(b, kx - w * .13f, ly - h * .13f, w * .26f, h * .26f);
                            g.DrawEllipse(pen, kx - w * .11f, ly - h * .11f, w * .22f, h * .22f);
                        }
                        break;
                    case "reports":
                        g.DrawLines(pen, new[] { new PointF(x + w * .62f, y + 1), new PointF(x + w * .18f, y + 1), new PointF(x + w * .18f, y + h - 1),
                            new PointF(x + w * .82f, y + h - 1), new PointF(x + w * .82f, y + h * .22f), new PointF(x + w * .62f, y + 1), new PointF(x + w * .62f, y + h * .22f), new PointF(x + w * .82f, y + h * .22f) });
                        g.DrawLine(pen, x + w * .34f, y + h * .5f, x + w * .66f, y + h * .5f);
                        g.DrawLine(pen, x + w * .34f, y + h * .7f, x + w * .66f, y + h * .7f); break;
                    case "credits":
                        using (var p = new GraphicsPath())
                        {
                            p.AddBezier(x + w / 2, y + h * .95f, x - w * .1f, y + h * .5f, x + w * .1f, y - h * .05f, x + w / 2, y + h * .25f);
                            p.AddBezier(x + w / 2, y + h * .25f, x + w * .9f, y - h * .05f, x + w * 1.1f, y + h * .5f, x + w / 2, y + h * .95f);
                            g.DrawPath(pen, p);
                        }
                        break;
                    default: AccentButton.DrawGlyph(g, glyph, r, c); break;
                }
            }
        }

        int Hit(Point p) { for (int i = 0; i < Items.Count; i++) if (Items[i].Rect.Contains(p)) return i; return -1; }
        protected override void OnMouseMove(MouseEventArgs e) { int h = Hit(e.Location); if (h != hover) { hover = h; Cursor = h >= 0 ? Cursors.Hand : Cursors.Default; Invalidate(); } base.OnMouseMove(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = -1; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseClick(MouseEventArgs e)
        {
            int h = Hit(e.Location);
            if (h >= 0)
            {
                var it = Items[h];
                if (it.Click != null) it.Click();
                else if (it.Page >= 0) Select(it.Page == Selected ? -1 : it.Page);
            }
            base.OnMouseClick(e);
        }
        /// <summary>Opens a section's sheet (-1 lowers whichever is open).</summary>
        public void Select(int i) { if (i == Selected) return; Selected = i; Invalidate(); if (SelectedChanged != null) SelectedChanged(this, EventArgs.Empty); }
    }
}
