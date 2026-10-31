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
        public AutosetupDialog(IList<SetupItem> items)
        {
            Text = LauncherInfo.Title + " - Multiplayer setup";
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = MinimizeBox = false;
            StartPosition = FormStartPosition.CenterParent;
            BackColor = Theme.Bg; ForeColor = Theme.Text;
            ClientSize = new Size(Theme.S(560), Theme.S(300));
            var label = new Label { Text = Autosetup.Text(items), Font = Theme.Ui(12), ForeColor = Theme.Text, BackColor = Color.Transparent, Bounds = Theme.R(20, 18, 520, 220) };
            var yes = new Button { Text = "Yes, Autosetup", DialogResult = DialogResult.Yes, Font = Theme.UiBold(12), Bounds = Theme.R(340, 248, 200, 38), FlatStyle = FlatStyle.Flat, BackColor = Theme.Red, ForeColor = Color.White };
            var no = new Button { Text = "No", DialogResult = DialogResult.No, Font = Theme.Ui(12), Bounds = Theme.R(220, 248, 110, 38), FlatStyle = FlatStyle.Flat, ForeColor = Theme.Text };
            Controls.Add(label); Controls.Add(no); Controls.Add(yes);
            AcceptButton = yes; CancelButton = no;
        }
    }
}
