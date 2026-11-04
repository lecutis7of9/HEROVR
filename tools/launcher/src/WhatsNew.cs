// SM2 VR Launcher - HERO VR 0.0.78 WHAT'S NEW (the owner, t1879u): what changed in each update, every version listed.
// Sources, merged (the first one that names a version wins): version.json's "whats_new_history" (and "whats_new" for
// its own version), the RELEASE-NOTES-v<version>.md files in the install, then each GitHub release's notes.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    static class WhatsNew
    {
        public const int MostChanges = 14, MostChars = 240;

        /// <summary>A release note's changes: each top-level "- " bullet with its indented continuation lines,
        /// markdown dropped (make_version_json.py's notes_changes, the same rule).</summary>
        public static List<string> Changes(string text)
        {
            var items = new List<List<string>>(); List<string> cur = null;
            foreach (var raw in (text ?? "").Replace("\r", "").Split(new[] { '\n' }))
            {
                if (raw.StartsWith("- ")) { cur = new List<string> { raw.Substring(2).Trim() }; items.Add(cur); }
                else if (cur != null && raw.StartsWith("  ") && raw.Trim().Length > 0) cur.Add(raw.Trim());
                else cur = null;
            }
            var outp = new List<string>();
            foreach (var parts in items.Take(MostChanges))
            {
                string t = Regex.Replace(string.Join(" ", parts), "[*`]", "").Trim();
                if (t.Length > MostChars) { t = t.Substring(0, MostChars); int sp = t.LastIndexOf(' '); if (sp > 0) t = t.Substring(0, sp); t += "..."; }
                if (t.Length > 0) outp.Add(t);
            }
            return outp;
        }

        static Version Key(string v)
        {
            var m = Regex.Match(v ?? "", @"\d+(\.\d+)+");
            Version r; return m.Success && Version.TryParse(m.Value.Split(new[] { '.' }).Length > 4 ? string.Join(".", m.Value.Split(new[] { '.' }).Take(4)) : m.Value, out r) ? r : new Version(0, 0);
        }

        /// <summary>Every version's changes, newest first.</summary>
        public static List<KeyValuePair<string, List<string>>> Collect(Manifest latest, string root, List<Release> releases)
        {
            var map = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
            Action<string, List<string>> put = (v, c) =>
            {
                var m = Regex.Match(v ?? "", @"\d+(\.\d+)+");
                if (!m.Success || c == null || c.Count == 0 || map.ContainsKey(m.Value)) return;
                map[m.Value] = c;
            };
            if (latest != null)
            {
                foreach (var h in latest.WhatsNewHistory) put(h.Key, h.Value);
                if (!string.IsNullOrEmpty(latest.WhatsNew)) put(latest.VersionNumber ?? latest.Version, new List<string> { latest.WhatsNew });
            }
            try
            {
                if (!string.IsNullOrEmpty(root) && Directory.Exists(root))
                    foreach (var f in Directory.GetFiles(root, "RELEASE-NOTES-v*.md"))
                    {
                        string v = Path.GetFileNameWithoutExtension(f).Substring("RELEASE-NOTES-v".Length);
                        put(v, Changes(File.ReadAllText(f, Encoding.UTF8)));
                    }
            }
            catch { }
            if (releases != null)
                foreach (var r in releases.Where(r => r != null && !r.Prerelease))
                    put(r.Tag, Changes(r.Body));
            return map.OrderByDescending(kv => Key(kv.Key)).ToList();
        }

        public static string Text(List<KeyValuePair<string, List<string>>> all)
        {
            var sb = new StringBuilder();
            foreach (var kv in all)
            {
                sb.Append("HERO VR ").Append(kv.Key).Append("\n");
                foreach (var c in kv.Value) sb.Append("  \u2022 ").Append(c).Append("\n");
                sb.Append("\n");
            }
            return sb.Length == 0 ? "No release notes found yet. Press Check for update to read them from GitHub." : sb.ToString();
        }

        public static void Show(IWin32Window owner, List<KeyValuePair<string, List<string>>> all)
        {
            using (var f = new Form())
            {
                f.Text = "What's New - " + LauncherInfo.Title;
                f.StartPosition = FormStartPosition.CenterParent;
                f.Size = new Size(860, 640);
                f.BackColor = Theme.Bg; f.ForeColor = Theme.Text;
                f.MinimizeBox = false; f.MaximizeBox = false; f.ShowInTaskbar = false;
                var box = new RichTextBox
                {
                    Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None, BackColor = Theme.Bg,
                    ForeColor = Theme.Text, Font = Theme.Ui(12.5f), DetectUrls = false, Text = Text(all)
                };
                foreach (var kv in all)
                {
                    int at = box.Text.IndexOf("HERO VR " + kv.Key + "\n", StringComparison.Ordinal);
                    if (at < 0) continue;
                    box.Select(at, ("HERO VR " + kv.Key).Length); box.SelectionFont = Theme.UiBold(14); box.SelectionColor = Theme.Red;
                }
                box.Select(0, 0);
                var close = new Button { Text = "Close", Dock = DockStyle.Bottom, Height = 36, FlatStyle = FlatStyle.Flat, BackColor = Theme.Bg, ForeColor = Theme.Text };
                close.Click += (s, e) => f.Close();
                f.Controls.Add(box); f.Controls.Add(close);
                f.ShowDialog(owner);
            }
        }
    }
}
