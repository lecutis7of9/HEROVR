// SM2 VR Launcher - the mod's version. Published versions are semver ("Version 0.0.1", tag v0.0.1); installs made
// before 0.0.1 carry the legacy internal "Update NN.N" in PORT-VERSION and always count as older than any semver.
//   VERSION        "0.0.1"                         (read first)
//   PORT-VERSION   "Version 0.0.1 (build 38.5)"    (semver parsed out; "(build 38.5)" is only a note)
//   PORT-VERSION   "Update 38.5"                   (legacy, old installs)
using System;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

namespace Sm2VrLauncher
{
    sealed class ModVersion : IComparable<ModVersion>, IEquatable<ModVersion>
    {
        public bool Legacy;
        public int Major, Minor, Patch;
        public string Pre = "";            // semver pre-release ("beta.2"), "" for a release
        public string BuildMeta = "";      // semver "+..." (ignored for ordering)
        public string BuildNote;           // "38.5" from "(build 38.5)"
        public Version LegacyNumber;       // 38.5 for "Update 38.5"
        public string Raw;

        static readonly Regex Sem = new Regex(@"(?<![\d.])v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?![\d.])", RegexOptions.IgnoreCase);
        static readonly Regex BuildRx = new Regex(@"\(\s*build\s+([0-9][0-9A-Za-z.\-]*)\s*\)", RegexOptions.IgnoreCase);
        static readonly Regex LegacyRx = new Regex(@"^\s*(?:update\s+)?v?(\d+(?:\.\d+){0,3})\s*$", RegexOptions.IgnoreCase);

        /// <summary>"0.0.1", "v0.0.1", "Version 0.0.1 (build 38.5)", "0.1.0-beta.2+abc", or legacy "Update 38.5" /
        /// "v38.5" / "38.5". Null when nothing parses.</summary>
        public static ModVersion Parse(string s)
        {
            if (string.IsNullOrWhiteSpace(s)) return null;
            s = s.Trim();
            string note = null;
            var bm = BuildRx.Match(s);
            if (bm.Success) { note = bm.Groups[1].Value; s = s.Remove(bm.Index, bm.Length).Trim(); }
            bool saysUpdate = Regex.IsMatch(s, @"^\s*update\b", RegexOptions.IgnoreCase);
            if (!saysUpdate)
            {
                var m = Sem.Match(s);
                if (m.Success)
                {
                    int a, b, c;
                    if (int.TryParse(m.Groups[1].Value, out a) && int.TryParse(m.Groups[2].Value, out b) && int.TryParse(m.Groups[3].Value, out c))
                        return new ModVersion { Major = a, Minor = b, Patch = c, Pre = m.Groups[4].Value, BuildMeta = m.Groups[5].Value, BuildNote = note, Raw = s };
                }
            }
            var l = LegacyRx.Match(s);
            if (!l.Success) l = Regex.Match(s, @"^\s*update\s+(\d+(?:\.\d+){0,3})", RegexOptions.IgnoreCase);
            if (l.Success)
            {
                string v = l.Groups[1].Value; if (!v.Contains(".")) v += ".0";
                Version r;
                if (Version.TryParse(v, out r)) return new ModVersion { Legacy = true, LegacyNumber = r, BuildNote = note, Raw = s };
            }
            return null;
        }

        /// <summary>The installed version of a mod folder: VERSION first, then PORT-VERSION. Null when neither reads.</summary>
        public static ModVersion Read(string root)
        {
            if (string.IsNullOrEmpty(root)) return null;
            string fromVersion = FirstLine(Path.Combine(root, "VERSION"));
            string fromPort = FirstLine(Path.Combine(root, "PORT-VERSION"));
            ModVersion v = Parse(fromVersion);
            if (v != null && v.Legacy) v = null;            // VERSION is semver-only
            var p = Parse(fromPort);
            if (v == null) return p;
            if (v.BuildNote == null && p != null) v.BuildNote = p.BuildNote;   // keep "(build 38.5)" from PORT-VERSION
            return v;
        }

        /// <summary>The same rule for the first lines of VERSION / PORT-VERSION found inside a zip.</summary>
        public static ModVersion FromFiles(string versionLine, string portVersionLine)
        {
            var v = Parse(versionLine); if (v != null && v.Legacy) v = null;
            var p = Parse(portVersionLine);
            if (v == null) return p;
            if (v.BuildNote == null && p != null) v.BuildNote = p.BuildNote;
            return v;
        }

        public static string FirstLine(string file)
        {
            try
            {
                if (!File.Exists(file)) return null;
                foreach (var l in File.ReadAllLines(file)) { string t = l.Trim().TrimStart('\uFEFF'); if (t.Length > 0) return t; }
            }
            catch { }
            return null;
        }

        /// <summary>"0.0.1" / "0.1.0-beta.2" (semver) or "38.5" (legacy).</summary>
        public string Number
        {
            get { return Legacy ? LegacyNumber.ToString() : Major + "." + Minor + "." + Patch + (Pre.Length > 0 ? "-" + Pre : ""); }
        }
        /// <summary>"Version 0.0.1" or, for an old install, "Update 38.5".</summary>
        public string Display { get { return Legacy ? "Update " + Number : "Version " + Number; } }
        /// <summary>"Version 0.0.1 (build 38.5)".</summary>
        public string DisplayLong { get { return Display + (BuildNote != null && !Legacy ? " (build " + BuildNote + ")" : ""); } }
        public string Tag { get { return "v" + Number; } }
        public override string ToString() { return Display; }

        // ---- Ordering: every legacy version < 0.0.1; semver 2.0.0 precedence otherwise ---------------------

        public int CompareTo(ModVersion o)
        {
            if (ReferenceEquals(o, null)) return 1;
            if (Legacy != o.Legacy) return Legacy ? -1 : 1;
            if (Legacy) return LegacyNumber.CompareTo(o.LegacyNumber);
            int c = Major.CompareTo(o.Major); if (c != 0) return c;
            c = Minor.CompareTo(o.Minor); if (c != 0) return c;
            c = Patch.CompareTo(o.Patch); if (c != 0) return c;
            return ComparePre(Pre, o.Pre);
        }

        static int ComparePre(string a, string b)
        {
            if (a == b) return 0;
            if (a.Length == 0) return 1;        // a release is above its pre-releases
            if (b.Length == 0) return -1;
            var x = a.Split('.'); var y = b.Split('.');
            for (int i = 0; i < Math.Min(x.Length, y.Length); i++)
            {
                bool xn = x[i].All(char.IsDigit), yn = y[i].All(char.IsDigit);
                int c;
                if (xn && yn) { c = x[i].Length != y[i].Length ? x[i].TrimStart('0').Length.CompareTo(y[i].TrimStart('0').Length) : 0; if (c == 0) c = string.CompareOrdinal(x[i].TrimStart('0'), y[i].TrimStart('0')); }
                else if (xn) c = -1;            // numeric identifiers sort below alphanumeric ones
                else if (yn) c = 1;
                else c = string.CompareOrdinal(x[i], y[i]);
                if (c != 0) return c;
            }
            return x.Length.CompareTo(y.Length);
        }

        public bool Equals(ModVersion o) { return !ReferenceEquals(o, null) && CompareTo(o) == 0; }
        public override bool Equals(object o) { return Equals(o as ModVersion); }
        public override int GetHashCode() { return Legacy ? LegacyNumber.GetHashCode() : (Major * 1000003 ^ Minor * 1009 ^ Patch ^ Pre.GetHashCode()); }
        public static bool operator >(ModVersion a, ModVersion b) { return !ReferenceEquals(a, null) && a.CompareTo(b) > 0; }
        public static bool operator <(ModVersion a, ModVersion b) { return ReferenceEquals(a, null) ? !ReferenceEquals(b, null) : a.CompareTo(b) < 0; }

        /// <summary>The same version named two ways ("Version 0.0.1" and "0.0.1 (build 38.5)").</summary>
        public static bool Same(string a, string b)
        {
            var x = Parse(a); var y = Parse(b);
            if (x == null || y == null) return string.Equals(a, b, StringComparison.OrdinalIgnoreCase);
            return x.Equals(y);
        }
    }

    static class LauncherInfo
    {
        public const string Version = "0.0.5";
        /// <summary>The name shown to players: window title, taskbar, dialogs, wordmark. Internal names (exe, settings folder,
        /// mutex, release manifest, update zips) stay "SM2 VR" so existing installs and settings keep working.</summary>
        public const string Title = "HERO VR";
        public const string UserAgent = "SM2VR-Launcher/" + Version;
    }
}
