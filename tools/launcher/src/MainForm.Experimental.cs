// HERO VR launcher build 12 (0.0.71.1): the EXPERIMENTAL / STABLE confirmation before every update (text: ExperimentalNotice.cs).
using System;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    partial class MainForm
    {
        /// <summary>True: the player confirmed the experimental notice (or the release is marked stable). False: the
        /// update stops here, before anything is downloaded or changed.</summary>
        bool ConfirmExperimental(Manifest m)
        {
            if (m == null || !m.Experimental) return true;
            string have = SystemInfo.PortVersion(root) ?? "no mod";
            string v = ExperimentalNotice.NewVersionOf(m), s = ExperimentalNotice.StableVersionOf(m);
            // automated tests only: SM2VR_LAUNCHER_TEST_EXPERIMENTAL_ANSWER=yes|no; an automated "yes" to the update
            // prompt (SM2VR_LAUNCHER_TEST_UPDATE_ANSWER=yes) also confirms it, so the e2e runs never block.
            string test = Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_EXPERIMENTAL_ANSWER");
            if (string.IsNullOrEmpty(test) && Environment.GetEnvironmentVariable("SM2VR_LAUNCHER_TEST_UPDATE_ANSWER") == "yes") test = "yes";
            buildLog.Line("EXPERIMENTAL build notice: " + v + " may crash or have issues; " + s + " is the most recent STABLE build (Revert > USE " + s + ").", Theme.Warn);
            bool ok = test == "yes" ? true : test == "no" ? false :
                MessageBox.Show(this, ExperimentalNotice.Text(m, have), LauncherInfo.Title + " - " + v + " is EXPERIMENTAL",
                    MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2) == DialogResult.Yes;
            if (ok) buildLog.Line("Experimental notice confirmed: updating to " + v + ".", Theme.Dim);
            else buildLog.Line("Update cancelled: the experimental notice for " + v + " was not confirmed. You stay on " + have + ".", Theme.Dim);
            return ok;
        }
    }
}
