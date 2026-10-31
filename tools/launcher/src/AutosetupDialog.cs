// SM2 VR Launcher - 0.0.74 MP INTERNET: the "No" / "Yes, Autosetup" pop-up window (the logic is Autosetup.cs).
// Made by Agent Mulder.
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    /// <summary>The pop-up: the text and two buttons, "No" and "Yes, Autosetup".</summary>
    class AutosetupDialog : Form
    {
        public AutosetupDialog(IList<SetupItem> items) : this("Multiplayer setup", Autosetup.Text(items)) { }

        /// <summary>0.0.75 SM1: the same pop-up for any missing piece (what is missing and why); the window grows to fit the text.</summary>
        public AutosetupDialog(string title, string text)
        {
            Text = LauncherInfo.Title + " - " + title;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = MinimizeBox = false;
            StartPosition = FormStartPosition.CenterParent;
            BackColor = Theme.Bg; ForeColor = Theme.Text;
            int textH = 220;
            try { textH = System.Math.Max(220, System.Math.Min(520, (int)System.Math.Ceiling(Theme.Measure(text ?? "", Theme.Ui(12), Theme.S(520)) / Theme.Scale) + 12)); } catch { }
            ClientSize = new Size(Theme.S(560), Theme.S(textH + 80));
            var label = new Label { Text = text ?? "", Font = Theme.Ui(12), ForeColor = Theme.Text, BackColor = Color.Transparent, Bounds = Theme.R(20, 18, 520, textH) };
            var yes = new Button { Text = "Yes, Autosetup", DialogResult = DialogResult.Yes, Font = Theme.UiBold(12), Bounds = Theme.R(340, textH + 28, 200, 38), FlatStyle = FlatStyle.Flat, BackColor = Theme.Red, ForeColor = Color.White };
            var no = new Button { Text = "No", DialogResult = DialogResult.No, Font = Theme.Ui(12), Bounds = Theme.R(220, textH + 28, 110, 38), FlatStyle = FlatStyle.Flat, ForeColor = Theme.Text };
            Controls.Add(label); Controls.Add(no); Controls.Add(yes);
            AcceptButton = yes; CancelButton = no;
        }
    }
}
