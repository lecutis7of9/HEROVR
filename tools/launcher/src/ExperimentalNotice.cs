// HERO VR launcher build 12 (0.0.71.1): every update to a newer version first asks the player to confirm that the
// newest live build is EXPERIMENTAL (it may crash or have issues), that 0.0.63 (0.0.73 in 0.0.74 test 10 only) is the most recent
// STABLE build and that Revert > USE 0.0.63 goes back to it. version.json can name another stable build ("stable_version") or mark a
// release stable ("experimental": false, then no notice). The update itself (MainForm.Updates.cs) is build 9's; it
// only calls ConfirmExperimental once, right before anything downloads (both the startup prompt and Update now).
using System;

namespace Sm2VrLauncher
{
    /// <summary>The text of the build-12 EXPERIMENTAL / STABLE notice (no UI here, so tests can compile it on its own).</summary>
    static class ExperimentalNotice
    {
        /// <summary>The most recent STABLE build when version.json does not name one ("stable_version").</summary>
        internal const string StableVersionDefault = "0.0.63";   // the 0.0.74 release: 0.0.63, the most recent STABLE build (test 10 used 0.0.73)

        internal static string StableVersionOf(Manifest m)
        {
            var s = m == null ? null : ModVersion.Parse(m.StableVersion);
            return s != null && !s.Legacy ? s.Number : StableVersionDefault;
        }

        internal static string NewVersionOf(Manifest m)
        {
            if (m == null) return "the new version";
            var v = m.Ver;
            return v != null && !v.Legacy ? v.Number : (m.Version ?? "the new version");
        }

        internal static string Text(Manifest m, string have)
        {
            string v = NewVersionOf(m), s = StableVersionOf(m);
            return v + " is an EXPERIMENTAL build. It may crash or have issues.\n\n" +
                s + " is the most recent STABLE build. If " + v + " gives you trouble, open Revert and press USE " + s +
                " to go back to it (your settings are kept).\n\n" +
                "Do you understand that " + v + " is experimental, and want to update to it?\n\n" +
                "Yes: I understand, update to " + v + ".    No: stay on " + (have ?? "my version") + ".";
        }

    }
}
