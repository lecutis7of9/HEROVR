// SM2 VR Launcher - a few more owner-drawn pieces: the version chip, link text, the footer and a dark text box.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    /// <summary>A small rounded pill: the version text, and the glass "Version 0.0.2 available" pill with a red dot.</summary>
    class Chip : Skin
    {
        public Color Fill = Color.FromArgb(150, 14, 16, 24), Edge = Color.FromArgb(60, 255, 255, 255), Fore = Color.FromArgb(236, 238, 242);
        public Font TextFont = Theme.Ui(12);
        /// <summary>No pill at all, just the text (the version next to the small mark).</summary>
        public bool Plain;
        /// <summary>A red dot before the text (an update is waiting).</summary>
        public bool Dot;
        /// <summary>When set, FitWidth keeps the right edge here (the pill sits left of the round icons).</summary>
        public int RightEdge = -1;
        public Chip(string text) { Text = text; }
        int DotSpace { get { return Dot ? Theme.S(16) : 0; } }
        public void FitWidth()
        {
            using (var bmp = new Bitmap(1, 1)) using (var g = Graphics.FromImage(bmp))
                Width = (int)Math.Ceiling(g.MeasureString(Text, TextFont).Width) + DotSpace + (Plain ? Theme.S(4) : Theme.S(26));
            if (RightEdge >= 0) Left = RightEdge - Width;
        }
        protected override void Render(Graphics g)
        {
            var r = new RectangleF(0.5f, 0.5f, Width - 1.5f, Height - 1.5f);
            if (!Plain)
                using (var p = Theme.Round(r, r.Height / 2))
                {
                    using (var b = new SolidBrush(Fill)) g.FillPath(b, p);
                    using (var pen = new Pen(Edge)) g.DrawPath(pen, p);
                }
            float pad = Plain ? Theme.S(2) : Theme.S(13);
            if (Dot)
            {
                float d = Theme.S(8);
                using (var b = new SolidBrush(Theme.RedHot)) g.FillEllipse(b, pad, (Height - d) / 2f, d, d);
                using (var b = new SolidBrush(Color.FromArgb(60, 255, 40, 60))) g.FillEllipse(b, pad - Theme.S(3), (Height - d) / 2f - Theme.S(3), d + Theme.S(6), d + Theme.S(6));
            }
            using (var sf = new StringFormat { Alignment = StringAlignment.Near, LineAlignment = StringAlignment.Center, FormatFlags = StringFormatFlags.NoWrap })
            using (var b = new SolidBrush(Fore)) g.DrawString(Text, TextFont, b, new RectangleF(pad + DotSpace, 0, Width, Height), sf);
        }
    }

    /// <summary>A line of text made of pieces, some of them links (opened in the browser) or actions.</summary>
    class RichLine : Skin
    {
        public class Piece { public string Text; public Font Font; public Color Color; public string Url; public Action Click; public RectangleF Rect; }
        public readonly List<Piece> Pieces = new List<Piece>();
        public StringAlignment Align = StringAlignment.Near;
        Piece hover;
        public ToolTip Tip;

        public RichLine Add(string text, Font font, Color color) { Pieces.Add(new Piece { Text = text, Font = font, Color = color }); return this; }
        public RichLine Link(string text, Font font, string url) { Pieces.Add(new Piece { Text = text, Font = font, Color = Theme.RedHot, Url = url }); return this; }
        public RichLine Act(string text, Font font, Action click) { Pieces.Add(new Piece { Text = text, Font = font, Color = Theme.RedHot, Click = click }); return this; }

        new void Layout(Graphics g)
        {
            float total = 0;
            var sizes = new List<SizeF>();
            using (var sf = StringFormat.GenericTypographic)
            {
                sf.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces;
                foreach (var p in Pieces) { var s = g.MeasureString(p.Text, p.Font, PointF.Empty, sf); sizes.Add(s); total += s.Width; }
            }
            float x = Align == StringAlignment.Far ? Width - total - 2 : Align == StringAlignment.Center ? (Width - total) / 2 : 0;
            for (int i = 0; i < Pieces.Count; i++)
            {
                Pieces[i].Rect = new RectangleF(x, (Height - sizes[i].Height) / 2f, sizes[i].Width, sizes[i].Height);
                x += sizes[i].Width;
            }
        }
        protected override void Render(Graphics g)
        {
            Layout(g);
            using (var sf = StringFormat.GenericTypographic)
            {
                sf.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces;
                foreach (var p in Pieces)
                {
                    bool link = p.Url != null || p.Click != null;
                    Color c = link && p == hover ? Theme.Text : p.Color;
                    using (var b = new SolidBrush(c)) g.DrawString(p.Text, p.Font, b, p.Rect.Location, sf);
                    if (link)
                        using (var pen = new Pen(Color.FromArgb(p == hover ? 220 : 110, c)))
                            g.DrawLine(pen, p.Rect.X, p.Rect.Bottom - 1, p.Rect.X + p.Rect.Width - Theme.S(2), p.Rect.Bottom - 1);
                }
            }
        }
        Piece Hit(Point pt)
        {
            foreach (var p in Pieces) if ((p.Url != null || p.Click != null) && p.Rect.Contains(pt)) return p;
            return null;
        }
        protected override void OnMouseMove(MouseEventArgs e)
        {
            var h = Hit(e.Location);
            if (h != hover)
            {
                hover = h; Cursor = h != null ? Cursors.Hand : Cursors.Default; Invalidate();
                if (Tip != null) Tip.SetToolTip(this, h != null && h.Url != null ? h.Url : "");
            }
            base.OnMouseMove(e);
        }
        protected override void OnMouseLeave(EventArgs e) { hover = null; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnMouseClick(MouseEventArgs e)
        {
            var h = Hit(e.Location);
            if (h != null) { if (h.Url != null) SystemInfo.OpenUrl(h.Url); else h.Click(); }
            base.OnMouseClick(e);
        }
    }

    /// <summary>A dark single-line text box with a thin border.</summary>
    class Field : Panel
    {
        public readonly TextBox Box;
        public Field()
        {
            BackColor = Theme.Field;
            Padding = new Padding(Theme.S(8), Theme.S(6), Theme.S(8), Theme.S(4));
            Box = new TextBox { BorderStyle = BorderStyle.None, BackColor = Theme.Field, ForeColor = Theme.Text, Font = Theme.NUi(13), Dock = DockStyle.Fill };
            Controls.Add(Box);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            using (var pen = new Pen(Color.FromArgb(70, Theme.Silver))) e.Graphics.DrawRectangle(pen, 0, 0, Width - 1, Height - 1);
        }
        public override string Text { get { return Box.Text; } set { Box.Text = value; } }
    }
}
