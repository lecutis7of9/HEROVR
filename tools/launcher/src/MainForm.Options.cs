// SM2 VR Launcher - the OPTIONS tab: launch-game-vr.ps1 flags as toggles, sliders and choices, remembered.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    partial class MainForm
    {
        readonly Dictionary<string, Control> optRows = new Dictionary<string, Control>(StringComparer.OrdinalIgnoreCase);
        TextLabel commandPreview;
        Field extraArgs;
        bool loadingOptions;

        string OptValue(Opt o)
        {
            if (o == null) return "0";
            return settings.Get("opt." + o.Param, o.Default);
        }

        void SetOpt(Opt o, string v)
        {
            settings.Set("opt." + o.Param, v);
            if (!loadingOptions)
            {
                settings.Save();
                RefreshCommandPreview();
                if (optionsSummary != null) optionsSummary.Text = "Launch options: " + LaunchOptions.Summary(OptValue);
            }
        }

        void BuildOptions()
        {
            var p = pages[1];
            var card = Add(p, new GlassPanel("Launch options"), 0, 0, 1108, 556);
            Add(card, new TextLabel("Remembered between runs. Passed to tools\\launch-game-vr.ps1 when you press PLAY VR. Most can also be changed in VR on the wrist menu.",
                Theme.Ui(12), Theme.Sub) { Wrap = false }, 196, 20, 890, 20);
            var area = Add(card, new ScrollArea(), 20, 50, 1074, 404);
            int y = 0, col = 0;
            string section = null;
            const int colW = 500, gap = 34;
            foreach (var o in LaunchOptions.All)
            {
                if (o.Section != section)
                {
                    if (col == 1) { col = 0; y += 76; }
                    section = o.Section;
                    var head = new TextLabel(GlassPanel.Spaced(section), Theme.UiBold(11), Theme.Red) { Wrap = false };
                    head.Bounds = Theme.R(0, y + (y == 0 ? 0 : 8), 1040, 20);
                    area.Controls.Add(head);
                    y += y == 0 ? 26 : 34;
                }
                Control row;
                var opt = o;
                string v = OptValue(o);
                if (o.Kind == OptKind.Int)
                {
                    int iv; if (!int.TryParse(v, out iv)) int.TryParse(o.Default, out iv);
                    var s = new SliderRow(o.Title, o.Sub, o.Min, o.Max, o.Step, iv, o.Suffix);
                    if (o.Param == "SnapTurn" || o.Param == "SmoothTurn") s.Format = n => n == 0 ? "off" : n + opt.Suffix;
                    if (o.Param == "MonitorRate") s.Format = n => n == 0 ? "every" : n + opt.Suffix;
                    s.Changed += (a, e) => SetOpt(opt, s.Value.ToString());
                    row = s;
                }
                else if (o.Kind == OptKind.Choice)
                {
                    var c = new ChoiceRow(o.Title, o.Sub, o.Choices, null, v);
                    c.Changed += (a, e) => SetOpt(opt, c.Value);
                    row = c;
                }
                else
                {
                    var t = new ToggleRow(o.Title, o.Sub, v == "1");
                    t.Changed += (a, e) =>
                    {
                        SetOpt(opt, t.Checked ? "1" : "0");
                        if (t.Checked && opt.Exclusive != null)
                        {
                            Control other;
                            if (optRows.TryGetValue(opt.Exclusive, out other)) ((ToggleRow)other).Checked = false;
                        }
                    };
                    row = t;
                }
                row.Bounds = Theme.R(col * (colW + gap), y, colW, 70);
                area.Controls.Add(row);
                optRows[o.Param] = row;
                if (col == 0) col = 1; else { col = 0; y += 76; }
            }
            // Room at the end so the last row clears the edge.
            var spacer = new TextLabel("", Theme.Ui(10), Theme.Dim);
            spacer.Bounds = Theme.R(0, y + (col == 1 ? 76 : 0), 10, 10);
            area.Controls.Add(spacer);

            Add(card, new TextLabel("EXTRA ARGUMENTS (ADVANCED)", Theme.UiBold(11), Theme.Silver) { Wrap = false }, 20, 466, 300, 18);
            extraArgs = Add(card, new Field(), 20, 486, 420, 34);
            extraArgs.Text = settings.Get("ExtraArgs", "");
            extraArgs.Box.TextChanged += (s, e) => { settings.Set("ExtraArgs", extraArgs.Text); settings.Save(); RefreshCommandPreview(); };
            tip.SetToolTip(extraArgs.Box, "Anything else for launch-game-vr.ps1, e.g. -EyeHeight 1700");
            commandPreview = Add(card, new TextLabel("", Theme.Mono(11), Theme.Sub), 460, 466, 456, 72);
            var reset = Add(card, new AccentButton("Reset to defaults", ButtonKind.Secondary) { Glyph = "refresh" }, 930, 484, 158, 38);
            reset.Click += (s, e) =>
            {
                if (MessageBox.Show(this, "Put every launch option back to the launcher's defaults (RenderScale 100, overlay webs, engine TAA, smaller desktop window)?",
                    LauncherInfo.Title, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
                loadingOptions = true;
                foreach (var o in LaunchOptions.All)
                {
                    settings.Set("opt." + o.Param, null);
                    Control r;
                    if (!optRows.TryGetValue(o.Param, out r)) continue;
                    if (r is ToggleRow) ((ToggleRow)r).Checked = o.Default == "1";
                    else if (r is SliderRow) ((SliderRow)r).Value = int.Parse(o.Default);
                    else if (r is ChoiceRow) ((ChoiceRow)r).Value = o.Default;
                }
                foreach (var o in LaunchOptions.All) settings.Set("opt." + o.Param, null);
                loadingOptions = false;
                settings.Save();
                RefreshCommandPreview();
                UpdatePlayState();
            };
            RefreshCommandPreview();
        }

        void ApplyScriptParams()
        {
            foreach (var o in LaunchOptions.All)
            {
                Control r;
                if (!optRows.TryGetValue(o.Param, out r)) continue;
                bool ok = LaunchOptions.Supported(o, scriptParams);
                r.Enabled = ok;
                string reason = ok ? null : "Not in the installed mod version (its launch-game-vr.ps1 has no -" + o.Param + ").";
                if (r is ToggleRow) ((ToggleRow)r).DisabledReason = reason;
                else if (r is SliderRow) ((SliderRow)r).DisabledReason = reason;
                else if (r is ChoiceRow) ((ChoiceRow)r).DisabledReason = reason;
            }
            RefreshCommandPreview();
        }

        void RefreshCommandPreview()
        {
            if (commandPreview == null) return;
            var args = LaunchOptions.Arguments(OptValue, scriptParams);
            string more = settings.Get("ExtraArgs", "").Trim();
            commandPreview.Text = "launch-game-vr.ps1 " + string.Join(" ", args) + (more.Length > 0 ? " " + more : "");
        }
    }
}
