// SM2 VR Launcher - version.json, the release manifest published with every GitHub Release (schema 1).
// Read from https://github.com/<repo>/releases/latest/download/version.json (no API limit), else from the API.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Web.Script.Serialization;

namespace Sm2VrLauncher
{
    class ManifestFile
    {
        public string Kind, Name, Url, Sha256;   // kind: update | source | package
        public long Size;
    }

    class Manifest
    {
        public int Schema = 1;
        public string Name, Version, VersionNumber, ReleaseDate, Tag;
        public string MinGameVersion, InstallSubfolder;
        public string ChangelogUrl, SourceUrl, License;
        public List<ManifestFile> Files = new List<ManifestFile>();
        public string BuildScript; public List<string> BuildArgs = new List<string>();
        public string LaunchScript; public List<string> LaunchArgs = new List<string>();
        public string ManifestUrl;
        public int LauncherBuild;   // "launcher_build": the launcher build inside the release zip (0 when not given)
        public string WhatsNew;     // "whats_new": one line shown in the update prompt
        public bool Experimental = true;   // build 12: "experimental" (default true): updating asks to confirm the EXPERIMENTAL notice
        public string StableVersion;       // build 12: "stable_version": the most recent STABLE build named in that notice (default 0.0.73; 0.0.63 before 0.0.74 test 10)
        /// <summary>launcher.ini UpdateManifestUrl=...: read version.json from here instead of GitHub (testing).</summary>
        public static string OverrideUrl;

        /// <summary>https, or http to this PC only (a local test server).</summary>
        public static bool SafeUrl(string u)
        {
            if (string.IsNullOrEmpty(u)) return false;
            if (u.StartsWith("https://", StringComparison.OrdinalIgnoreCase)) return true;
            return u.StartsWith("http://127.0.0.1:", StringComparison.OrdinalIgnoreCase) || u.StartsWith("http://127.0.0.1/", StringComparison.OrdinalIgnoreCase)
                || u.StartsWith("http://localhost:", StringComparison.OrdinalIgnoreCase) || u.StartsWith("http://localhost/", StringComparison.OrdinalIgnoreCase);
        }

        public ManifestFile File(string kind) { return Files.FirstOrDefault(f => string.Equals(f.Kind, kind, StringComparison.OrdinalIgnoreCase)); }
        public ModVersion Ver { get { return ModVersion.Parse(VersionNumber) ?? ModVersion.Parse(Version) ?? ModVersion.Parse(Tag); } }

        static string S(Dictionary<string, object> d, string k) { object v; return d != null && d.TryGetValue(k, out v) && v != null ? Convert.ToString(v) : null; }
        static Dictionary<string, object> D(Dictionary<string, object> d, string k) { object v; return d != null && d.TryGetValue(k, out v) ? v as Dictionary<string, object> : null; }
        static List<string> L(Dictionary<string, object> d, string k)
        {
            object v; var r = new List<string>();
            if (d != null && d.TryGetValue(k, out v) && v is ArrayList) foreach (var x in (ArrayList)v) r.Add(Convert.ToString(x));
            return r;
        }

        public static Manifest Parse(string json)
        {
            var js = new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024 };
            var d = js.Deserialize<Dictionary<string, object>>(json);
            var m = new Manifest();
            int schema; if (int.TryParse(S(d, "schema") ?? "1", out schema)) m.Schema = schema;
            if (m.Schema > 1) throw new InvalidDataException("This version.json is schema " + m.Schema + "; this launcher reads schema 1. Update the launcher.");
            m.Name = S(d, "name"); m.Version = S(d, "version"); m.VersionNumber = S(d, "version_number"); m.Tag = S(d, "tag");
            m.ReleaseDate = S(d, "release_date"); m.MinGameVersion = S(d, "min_game_version"); m.InstallSubfolder = S(d, "install_subfolder");
            int lb; if (int.TryParse(S(d, "launcher_build") ?? "0", out lb)) m.LauncherBuild = lb;
            m.WhatsNew = S(d, "whats_new");
            m.Experimental = !string.Equals(S(d, "experimental"), "false", StringComparison.OrdinalIgnoreCase); m.StableVersion = S(d, "stable_version");   // build 12
            m.ChangelogUrl = S(d, "changelog_url"); m.SourceUrl = S(d, "source_url"); m.License = S(d, "license");
            object files;
            if (d.TryGetValue("files", out files) && files is ArrayList)
                foreach (Dictionary<string, object> f in (ArrayList)files)
                {
                    long size; long.TryParse(S(f, "size") ?? "0", out size);
                    m.Files.Add(new ManifestFile { Kind = S(f, "kind"), Name = S(f, "name"), Url = S(f, "url"), Sha256 = (S(f, "sha256") ?? "").ToLowerInvariant(), Size = size });
                }
            var b = D(d, "build"); m.BuildScript = S(b, "script"); m.BuildArgs = L(b, "args");
            var l = D(d, "launch"); m.LaunchScript = S(l, "script"); m.LaunchArgs = L(l, "args");
            if (string.IsNullOrEmpty(m.Version)) throw new InvalidDataException("version.json has no \"version\".");
            foreach (var f in m.Files)
                if (!SafeUrl(f.Url))
                    throw new InvalidDataException("version.json lists a file without an https download URL (" + f.Name + ").");
            return m;
        }

        public static string Fetch(string url)
        {
            var r = (HttpWebRequest)WebRequest.Create(url);
            r.UserAgent = LauncherInfo.UserAgent;
            r.Timeout = 20000;
            r.AllowAutoRedirect = true;
            using (var resp = (HttpWebResponse)r.GetResponse())
            using (var rd = new StreamReader(resp.GetResponseStream(), Encoding.UTF8))
                return rd.ReadToEnd();
        }

        /// <summary>The latest release's manifest: the releases/latest/download URL first (a plain download, not
        /// rate limited), then the API (whose asset list also tells us when a release has no version.json).</summary>
        public static Manifest Latest(string repo, out Release release, Action<string> log)
        {
            release = null;
            if (!string.IsNullOrEmpty(OverrideUrl))
            {
                if (!SafeUrl(OverrideUrl)) throw new InvalidDataException("UpdateManifestUrl must be https (or http://127.0.0.1 for a local test): " + OverrideUrl);
                log("Reading version.json from UpdateManifestUrl " + OverrideUrl);
                var o = Parse(Fetch(OverrideUrl)); o.ManifestUrl = OverrideUrl; return o;
            }
            string direct = "https://github.com/" + repo.Trim().Trim(new[] { '/' }) + "/releases/latest/download/version.json";
            try
            {
                var m = Parse(Fetch(direct));
                m.ManifestUrl = direct;
                return m;
            }
            catch (Exception ex) { log("version.json from releases/latest/download: " + ex.Message + " Trying the GitHub API."); }
            release = GitHub.Latest(repo);
            return ForRelease(release, log);
        }

        /// <summary>A release's own version.json if it has one, else a manifest made from the release's assets.</summary>
        public static Manifest ForRelease(Release r, Action<string> log)
        {
            var asset = r.Assets.FirstOrDefault(a => a.Name.Equals("version.json", StringComparison.OrdinalIgnoreCase));
            if (asset != null)
            {
                try { var m = Parse(Fetch(asset.Url)); m.ManifestUrl = asset.Url; return m; }
                catch (Exception ex) { log("The release's version.json could not be read (" + ex.Message + "); using its assets instead."); }
            }
            var g = new Manifest { Name = "SM2 VR", Version = string.IsNullOrEmpty(r.Name) ? r.Tag : r.Name, Tag = r.Tag, VersionNumber = r.Ver == null ? null : r.Ver.Number };
            if (r.Ver != null && !r.Ver.Legacy) g.Version = r.Ver.Display;
            if (r.UpdateZip != null) g.Files.Add(new ManifestFile { Kind = "update", Name = r.UpdateZip.Name, Url = r.UpdateZip.Url, Size = r.UpdateZip.Size, Sha256 = GitHub.ExpectedSha256(r, r.UpdateZip) });
            if (r.FullZip != null) g.Files.Add(new ManifestFile { Kind = "source", Name = r.FullZip.Name, Url = r.FullZip.Url, Size = r.FullZip.Size, Sha256 = GitHub.ExpectedSha256(r, r.FullZip) });
            g.ChangelogUrl = r.HtmlUrl;
            return g;
        }
    }
}
