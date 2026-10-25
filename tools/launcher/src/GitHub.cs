// SM2 VR Launcher - GitHub Releases: the latest release, the list of releases, downloads and sha256 checks.
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;

namespace Sm2VrLauncher
{
    class ReleaseAsset
    {
        public string Name, Url;
        public long Size;
    }

    class Release
    {
        public string Tag, Name, Body, HtmlUrl;
        public DateTime Published;
        public bool Prerelease;
        public List<ReleaseAsset> Assets = new List<ReleaseAsset>();
        public ModVersion Ver { get { return ModVersion.Parse(Tag) ?? ModVersion.Parse(Name); } }

        /// <summary>The update zip (sm2-vr-*.zip, or sm2-update-*.zip from before 0.0.1), else any non-full .zip.</summary>
        public ReleaseAsset UpdateZip
        {
            get
            {
                return Assets.FirstOrDefault(a => SystemInfo.IsUpdateZipName(a.Name))
                    ?? Assets.FirstOrDefault(a => a.Name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase) && a.Name.IndexOf("full", StringComparison.OrdinalIgnoreCase) < 0
                        && a.Name.IndexOf("win64", StringComparison.OrdinalIgnoreCase) < 0);
            }
        }
        /// <summary>The full package (sm2-full-*.zip / *-full.zip), the right thing for going back to an older version.</summary>
        public ReleaseAsset FullZip
        {
            get { return Assets.FirstOrDefault(a => a.Name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase) && a.Name.IndexOf("full", StringComparison.OrdinalIgnoreCase) >= 0); }
        }
        public string Display { get { return (string.IsNullOrEmpty(Name) ? Tag : Name) + (Published == DateTime.MinValue ? "" : "  \u00B7  " + Published.ToLocalTime().ToString("d MMM yyyy")); } }
    }

    static class GitHub
    {
        const string Api = "https://api.github.com/repos/";

        static GitHub()
        {
            try { ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12; } catch { }
        }

        static HttpWebRequest Request(string url)
        {
            var r = (HttpWebRequest)WebRequest.Create(url);
            r.UserAgent = LauncherInfo.UserAgent;
            r.Accept = "application/vnd.github+json";
            r.Timeout = 20000;
            r.ReadWriteTimeout = 60000;
            r.AllowAutoRedirect = true;
            return r;
        }

        static string GetText(string url)
        {
            try
            {
                using (var resp = (HttpWebResponse)Request(url).GetResponse())
                using (var rd = new StreamReader(resp.GetResponseStream(), Encoding.UTF8))
                    return rd.ReadToEnd();
            }
            catch (WebException ex)
            {
                var resp = ex.Response as HttpWebResponse;
                if (resp != null)
                {
                    int code = (int)resp.StatusCode;
                    if (code == 404) throw new Exception("GitHub says not found: check the repository name in Settings (owner/name), and that it is public and has a published release.");
                    if (code == 403 || code == 429) throw new Exception("GitHub's limit for checks without signing in was reached. Try again in an hour.");
                    throw new Exception("GitHub answered " + code + " " + resp.StatusDescription + ".");
                }
                throw new Exception("Could not reach GitHub (" + ex.Message + "). Check the internet connection.");
            }
        }

        static Release Parse(Dictionary<string, object> d)
        {
            var r = new Release();
            r.Tag = d.ContainsKey("tag_name") ? Convert.ToString(d["tag_name"]) : "";
            r.Name = d.ContainsKey("name") ? Convert.ToString(d["name"]) : "";
            r.Body = d.ContainsKey("body") ? Convert.ToString(d["body"]) : "";
            r.HtmlUrl = d.ContainsKey("html_url") ? Convert.ToString(d["html_url"]) : "";
            r.Prerelease = d.ContainsKey("prerelease") && d["prerelease"] is bool && (bool)d["prerelease"];
            DateTime t;
            if (d.ContainsKey("published_at") && d["published_at"] != null && DateTime.TryParse(Convert.ToString(d["published_at"]), null, System.Globalization.DateTimeStyles.RoundtripKind, out t)) r.Published = t;
            var assets = d.ContainsKey("assets") ? d["assets"] as ArrayList : null;
            if (assets != null)
                foreach (Dictionary<string, object> a in assets)
                    r.Assets.Add(new ReleaseAsset
                    {
                        Name = Convert.ToString(a["name"]),
                        Url = Convert.ToString(a["browser_download_url"]),
                        Size = a.ContainsKey("size") ? Convert.ToInt64(a["size"]) : 0
                    });
            return r;
        }

        public static Release Latest(string repo)
        {
            var js = new JavaScriptSerializer { MaxJsonLength = 16 * 1024 * 1024 };
            return Parse(js.Deserialize<Dictionary<string, object>>(GetText(Api + repo.Trim().Trim(new[] { '/' }) + "/releases/latest")));
        }

        public static List<Release> All(string repo)
        {
            var js = new JavaScriptSerializer { MaxJsonLength = 16 * 1024 * 1024 };
            var list = js.Deserialize<ArrayList>(GetText(Api + repo.Trim().Trim(new[] { '/' }) + "/releases?per_page=30"));
            var result = new List<Release>();
            foreach (Dictionary<string, object> d in list) result.Add(Parse(d));
            return result;
        }

        /// <summary>The expected sha256 of an asset: a "<asset>.sha256" or SHA256SUMS asset, or a 64-hex value in the
        /// release text on the asset's line (or the only one there). Null when the release gives none.</summary>
        public static string ExpectedSha256(Release r, ReleaseAsset asset)
        {
            var hex = new Regex(@"\b[0-9a-fA-F]{64}\b");
            foreach (var a in r.Assets)
            {
                bool own = a.Name.Equals(asset.Name + ".sha256", StringComparison.OrdinalIgnoreCase);
                bool sums = Regex.IsMatch(a.Name, @"^(SHA256SUMS|sha256sums)(\.txt)?$|checksums?\.txt$", RegexOptions.IgnoreCase);
                if (!own && !sums) continue;
                string text = GetText(a.Url);
                foreach (var line in text.Split(new[] { '\n' }))
                {
                    var m = hex.Match(line);
                    if (m.Success && (own || line.IndexOf(asset.Name, StringComparison.OrdinalIgnoreCase) >= 0)) return m.Value.ToLowerInvariant();
                }
            }
            if (!string.IsNullOrEmpty(r.Body))
            {
                foreach (var line in r.Body.Split(new[] { '\n' }))
                    if (line.IndexOf(asset.Name, StringComparison.OrdinalIgnoreCase) >= 0 && hex.IsMatch(line)) return hex.Match(line).Value.ToLowerInvariant();
                var all = hex.Matches(r.Body);
                if (all.Count == 1) return all[0].Value.ToLowerInvariant();
            }
            return null;
        }

        /// <summary>Downloads to `path` (via a .part file), reporting (bytes, total). Cancellable.</summary>
        public static void Download(string url, string path, Action<long, long> progress, CancellationToken cancel)
        {
            string part = path + ".part";
            var req = Request(url);
            req.Accept = "application/octet-stream";
            using (var resp = (HttpWebResponse)req.GetResponse())
            using (var src = resp.GetResponseStream())
            using (var dst = File.Create(part))
            {
                long total = resp.ContentLength, got = 0;
                var buf = new byte[1 << 16];
                int n;
                var last = DateTime.MinValue;
                while ((n = src.Read(buf, 0, buf.Length)) > 0)
                {
                    cancel.ThrowIfCancellationRequested();
                    dst.Write(buf, 0, n);
                    got += n;
                    if ((DateTime.Now - last).TotalMilliseconds > 100) { progress(got, total); last = DateTime.Now; }
                }
                progress(got, total);
                if (total > 0 && got != total) throw new IOException("The download stopped early (" + got + " of " + total + " bytes).");
            }
            if (File.Exists(path)) File.Delete(path);
            File.Move(part, path);
        }

        public static string Sha256(string path)
        {
            using (var s = File.OpenRead(path))
            using (var h = SHA256.Create())
                return BitConverter.ToString(h.ComputeHash(s)).Replace("-", "").ToLowerInvariant();
        }
    }
}
