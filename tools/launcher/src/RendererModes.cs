// HERO VR Revamped PRIVATE preview: renderer choice visible on first launcher frame.
// This file is intentionally separate from the public 0.0.78 launcher.
// The selector changes only the launcher's persistent preference; DLL swapping
// happens at PLAY VR before the game starts, never inside a live game.
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Security.Cryptography;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    static class RendererModes
    {
        public const string Key = "RendererMode";
        public const string Original = "original";
        public const string Revamped = "revamped";
        public static string Normal(string text)
        {
            return string.Equals(text, Revamped, StringComparison.OrdinalIgnoreCase) ? Revamped : Original;
        }
        public static string Store(string root, string mode)
        {
            return Path.Combine(root, "reports", "renderer-modes", Normal(mode), "spidy_stereo_probe.dll");
        }
        static string Hash(string path)
        {
            using (var stream = File.OpenRead(path))
            using (var sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }
        public static bool Ready(string root)
        {
            return File.Exists(Store(root, Original)) && File.Exists(Store(root, Revamped));
        }
        public static bool Apply(string root, string mode, out string info)
        {
            mode = Normal(mode);
            info = "";
            try
            {
                if (!SystemInfo.IsPortRoot(root)) throw new InvalidOperationException("HERO VR mod folder was not found.");
                if (SystemInfo.GameRunning())
                    throw new InvalidOperationException("Close Spider-Man 2 before changing renderer mode. VR modules cannot be swapped inside a running game.");
                string source = Store(root, mode);
                if (!File.Exists(source))
                    throw new FileNotFoundException("The private test pack needs both renderer DLLs installed first. Re-run the private-preview installer.", source);
                string target = SystemInfo.BuiltModule(root);
                if (!File.Exists(target)) throw new FileNotFoundException("The HERO VR mod is not installed.", target);
                string digest = Hash(source);
                // The launcher stages a module by SHA. HEROVR's 70% enable
                // marker must be NEXT TO the staged DLL, not the root DLL.
                string staged = Path.Combine(root, "reports", "stereo-modules", digest);
                Directory.CreateDirectory(staged);
                string stagedDll = Path.Combine(staged, "spidy_stereo_probe.dll");
                if (!File.Exists(stagedDll) || Hash(stagedDll) != digest)
                    File.Copy(source, stagedDll, true);
                string marker = Path.Combine(staged, "herovr-own-taa-70.enable");
                if (mode == Revamped)
                {
                    if (!File.Exists(marker)) File.WriteAllText(marker, "Private Revamped 70% native-eye temporal upscale");
                }
                else if (File.Exists(marker)) File.Delete(marker);
                // Fully stage and verify before replacing the active on-disk
                // module; never delete the user's only copy on an error.
                if (Hash(target) != digest)
                {
                    string temp = target + ".render-mode.tmp";
                    try
                    {
                        File.Copy(source, temp, true);
                        if (Hash(temp) != digest) throw new IOException("The selected renderer DLL failed verification.");
                        File.Copy(temp, target, true);
                    }
                    finally { try { if (File.Exists(temp)) File.Delete(temp); } catch { } }
                }
                // The new 70% path is selected only with VR AA = ours and this
                // staged marker. Retain every other user's visual preference.
                if (mode == Revamped)
                {
                    if (!LiveFile.Set(root, "vr_aa", "ours"))
                        throw new IOException("Could not set the Revamped reconstruction in live.txt.");
                }
                info = (mode == Revamped ? "REVAMPED: 70% temporal reconstruction" : "ORIGINAL: HERO VR 0.0.78 renderer") +
                       " (DLL SHA256 " + digest.Substring(0, 12) + "...)";
                return true;
            }
            catch (Exception ex) { info = ex.Message; return false; }
        }
    }

    /// <summary>A compact, keyboard-accessible two-choice bar in the launcher header:
    /// ORIGINAL / REVAMPED are visible immediately, before PLAY VR.</summary>
    sealed class RendererModePicker : Skin
    {
        string selected = RendererModes.Original;
        Rectangle originalButton, revampedButton;
        bool hoverOriginal, hoverRevamped;

        public event EventHandler Changed;
        public string Selection
        {
            get { return selected; }
            set
            {
                string next = RendererModes.Normal(value);
                if (next == selected) return;
                selected = next;
                Invalidate();
                if (Changed != null) Changed(this, EventArgs.Empty);
            }
        }
        public RendererModePicker()
        {
            TabStop = true;
            Cursor = Cursors.Hand;
            MinimumSize = new Size(270, 34);
            SetStyle(ControlStyles.Selectable, true);
        }
        void LayoutButtons()
        {
            int h = Math.Max(24, Height - 14), y = (Height - h) / 2;
            int first = Math.Max(94, (int)(Width * .28));
            int gap = 5, bw = Math.Max(70, (Width - first - 16 - gap) / 2);
            originalButton = new Rectangle(first, y, bw, h);
            revampedButton = new Rectangle(first + bw + gap, y, bw, h);
        }
        protected override void Render(Graphics g)
        {
            LayoutButtons();
            var face = new RectangleF(.5f, .5f, Width - 1.5f, Height - 1.5f);
            using (var p = Theme.Round(face, Math.Min(14f, Height / 2f)))
            {
                using (var b = new SolidBrush(Color.FromArgb(215, 14, 17, 24))) g.FillPath(b, p);
                using (var pen = new Pen(Color.FromArgb(130, 108, 118, 138), 1f)) g.DrawPath(pen, p);
            }
            using (var label = new SolidBrush(Theme.Sub))
            using (var format = new StringFormat { Alignment = StringAlignment.Near, LineAlignment = StringAlignment.Center })
                g.DrawString("PRIVATE TEST\nRENDER", Theme.UiBold(9f), label, new RectangleF(11, 0, originalButton.Left - 14, Height), format);
            DrawChoice(g, originalButton, "ORIGINAL", selected == RendererModes.Original, hoverOriginal);
            DrawChoice(g, revampedButton, "REVAMPED", selected == RendererModes.Revamped, hoverRevamped);
        }
        void DrawChoice(Graphics g, Rectangle rect, string text, bool current, bool hover)
        {
            var shape = new RectangleF(rect.X, rect.Y, rect.Width, rect.Height);
            Color accent = Theme.HeroStyle ? Theme.Neon : Theme.Red;
            using (var p = Theme.Round(shape, 10))
            {
                using (var bg = new SolidBrush(current
                    ? (Theme.HeroStyle ? Color.FromArgb(65, 107, 236, 65) : Color.FromArgb(140, 178, 37, 53))
                    : hover ? Color.FromArgb(70, 90, 104, 120) : Color.FromArgb(55, 30, 37, 49))) g.FillPath(bg, p);
                using (var pen = new Pen(current ? accent : Color.FromArgb(75, 120, 126, 135), current ? 1.8f : 1f))
                    g.DrawPath(pen, p);
            }
            using (var format = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center })
            using (var brush = new SolidBrush(current ? Theme.Text : Theme.Sub))
                g.DrawString(text, Theme.UiBold(10.4f), brush, shape, format);
        }
        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e);
            LayoutButtons();
            bool a = originalButton.Contains(e.Location), b = revampedButton.Contains(e.Location);
            if (hoverOriginal != a || hoverRevamped != b) { hoverOriginal = a; hoverRevamped = b; Invalidate(); }
        }
        protected override void OnMouseLeave(EventArgs e)
        {
            base.OnMouseLeave(e);
            hoverOriginal = hoverRevamped = false;
            Invalidate();
        }
        protected override void OnMouseUp(MouseEventArgs e)
        {
            base.OnMouseUp(e);
            if (e.Button != MouseButtons.Left || !Enabled) return;
            LayoutButtons();
            if (originalButton.Contains(e.Location)) Selection = RendererModes.Original;
            else if (revampedButton.Contains(e.Location)) Selection = RendererModes.Revamped;
        }
        protected override bool IsInputKey(Keys keyData)
        {
            if (keyData == Keys.Left || keyData == Keys.Right) return true;
            return base.IsInputKey(keyData);
        }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (Enabled && (e.KeyCode == Keys.Left || e.KeyCode == Keys.Right || e.KeyCode == Keys.Space))
            {
                Selection = e.KeyCode == Keys.Left ? RendererModes.Original : RendererModes.Revamped;
                e.Handled = true;
                return;
            }
            base.OnKeyDown(e);
        }
    }
}
