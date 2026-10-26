// HERO VR launcher - which design opens: the new one (the concept video) or the classic one. Both share every
// function; only the look differs. The choice lives in settings.ini (LauncherDesign=hero|classic). A start of the
// new design that never finished (a crash or a hang, seen through a marker file), a crash in it, or a bad asset
// opens the classic design and says why, once. Shift while opening, or --classic, always opens the classic design.
using System;
using System.IO;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    enum Design { Classic, Hero }

    static class LauncherMode
    {
        public const string KeyDesign = "LauncherDesign";          // hero | classic (anything else: classic)
        public const string KeyOfferShown = "HeroOfferShown";      // the classic launcher's one-time "new design" question
        public const string KeyFallbackReason = "HeroFallbackReason";
        public const string KeySound = "LauncherSound";            // the new design's intro and ambient sound (default on)
        public const string Hero = "hero", Classic = "classic";

        /// <summary>The design this process runs (set once in Main).</summary>
        public static Design Current = Design.Classic;
        /// <summary>Why the classic design opened instead of the new one, shown once ("" when nothing to say).</summary>
        public static string Notice = "";
        /// <summary>True when the player forced the classic design for this start (Shift or --classic).</summary>
        public static bool Forced;

        public static string MarkerPath { get { return Path.Combine(Settings.Folder, "hero-starting.marker"); } }

        public static bool IsHero(Settings s)
        {
            string v = (s.Get(KeyDesign, Classic) ?? "").Trim();
            return v.Equals(Hero, StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>Decides the design for this start. Pure apart from the marker file and the settings it changes.</summary>
        public static Design Decide(Settings s, string[] args, bool shiftHeld)
        {
            Notice = "";
            Forced = false;
            string pending = s.Get(KeyFallbackReason, "");
            if (!string.IsNullOrEmpty(pending)) Notice = pending;
            if (Array.IndexOf(args, "--classic") >= 0 || shiftHeld)
            {
                Forced = true;
                return Design.Classic;
            }
            string raw = s.Get(KeyDesign, null);
            if (raw == null) return Design.Classic;
            if (!IsHero(s))
            {
                if (!raw.Trim().Equals(Classic, StringComparison.OrdinalIgnoreCase))
                {
                    // A damaged value: the classic design, and the file is repaired.
                    s.Set(KeyDesign, Classic);
                    TrySave(s);
                }
                return Design.Classic;
            }
            if (File.Exists(MarkerPath))
            {
                // The last start of the new design never finished: it crashed or hung before its first frame.
                FallBack(s, "The new launcher design did not finish starting last time (it closed or stopped responding), so the classic launcher opened instead.");
                Notice = s.Get(KeyFallbackReason, "");
                return Design.Classic;
            }
            return Design.Hero;
        }

        /// <summary>Switches the next starts to the classic design and remembers why (shown once).</summary>
        public static void FallBack(Settings s, string reason)
        {
            try { File.Delete(MarkerPath); } catch { }
            s.Set(KeyDesign, Classic);
            s.Set(KeyFallbackReason, reason);
            TrySave(s);
            Log("fallback to classic: " + reason);
        }

        /// <summary>The reason was shown: never again.</summary>
        public static void NoticeShown(Settings s)
        {
            Notice = "";
            if (string.IsNullOrEmpty(s.Get(KeyFallbackReason, ""))) return;
            s.Set(KeyFallbackReason, null);
            s.Set(KeyOfferShown, "1");   // they have already tried it: the one-time question is not asked as well
            TrySave(s);
        }

        public static void WriteMarker()
        {
            try { Directory.CreateDirectory(Settings.Folder); File.WriteAllText(MarkerPath, DateTime.Now.ToString("o") + " launcher " + LauncherInfo.Version + " build " + LauncherInfo.Build); }
            catch { }
        }

        public static void ClearMarker()
        {
            try { if (File.Exists(MarkerPath)) File.Delete(MarkerPath); } catch { }
        }

        static void TrySave(Settings s) { try { s.Save(); } catch { } }

        public static void Log(string line)
        {
            try
            {
                string d = Path.Combine(Settings.Folder, "logs");
                Directory.CreateDirectory(d);
                File.AppendAllText(Path.Combine(d, "launcher-design.log"), DateTime.Now.ToString("o") + "  " + line + Environment.NewLine);
            }
            catch { }
        }

        /// <summary>True while Shift is held (checked once, as the launcher opens).</summary>
        public static bool ShiftHeld()
        {
            try { return (Control.ModifierKeys & Keys.Shift) == Keys.Shift; } catch { return false; }
        }

        /// <summary>Starts a new copy of the launcher (it waits for this one to close) and closes this one.</summary>
        public static bool Restart(Form owner, string extraArg)
        {
            try
            {
                string exe = System.Reflection.Assembly.GetExecutingAssembly().Location;
                var psi = Type.GetType("Mono.Runtime") != null
                    ? new System.Diagnostics.ProcessStartInfo("mono", "\"" + exe + "\" --after-update" + (extraArg == null ? "" : " " + extraArg))
                    : new System.Diagnostics.ProcessStartInfo(exe, "--after-update" + (extraArg == null ? "" : " " + extraArg));
                psi.UseShellExecute = false;
                psi.WorkingDirectory = Path.GetDirectoryName(exe);
                System.Diagnostics.Process.Start(psi);
                owner.Close();
                return true;
            }
            catch (Exception ex)
            {
                Program.LogError(ex);
                MessageBox.Show(owner, "Could not restart the launcher: " + ex.Message + "\n\nClose it and open it again to use the other design.", LauncherInfo.Title);
                return false;
            }
        }
    }
}
