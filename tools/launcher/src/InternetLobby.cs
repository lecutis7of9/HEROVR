// SM2 VR Launcher - 0.0.74 MP INTERNET: the internet server list (server/lobby, a Cloudflare Worker) next to the
// LAN list. The launcher does all the HTTPS; the game only asks STUN for its public address (MP-PUBLIC.txt) and
// takes punch / relay / token from MP-LOBBY.txt live. No accounts or installs for players.
//   HOST: while the game runs (MP-PUBLIC.txt fresh) POST /v1/host every 5 s (30 s when full) -> listed; joiners
//         that pressed JOIN come back as punch lines -> MP-LOBBY.txt -> the game opens its router to them.
//   JOIN: POST /v1/join (no address yet) -> the host's public address -> MP-LOBBY.txt join=...; once our game
//         knows its public address, POST /v1/join again with it (the host punches back), every 5 s until in.
// The list itself never has an address in it. Made by Agent Mulder.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;

namespace Sm2VrLauncher
{
    class InetGame
    {
        public string Id = "", Name = "", HeroName = "";
        public int Hero = 255, Players = 1, MaxPlayers = 2, Protocol;
        public uint LanId;
        public bool Full { get { return Players >= MaxPlayers; } }
        public bool SameVersion { get { return Protocol == LobbyBeacon.MpProtocol; } }
    }

    /// <summary>One row of the merged list: a game heard on this network (Lan) or listed on the internet (Inet).</summary>
    class ServerEntry
    {
        public LobbyHost Lan;
        public InetGame Inet;
        public bool Internet { get { return Lan == null; } }
        public string Name { get { var n = Lan != null ? Lan.Beacon.Name : Inet.Name; return n.Length == 0 ? "HERO VR" : n; } }
        public string HeroName { get { return Lan != null ? Lan.Beacon.HeroName : Inet.HeroName; } }
        public int Players { get { return Lan != null ? Lan.Beacon.Players : Inet.Players; } }
        public int MaxPlayers { get { return Lan != null ? Lan.Beacon.MaxPlayers : Inet.MaxPlayers; } }
        public bool Full { get { return Lan != null ? Lan.Beacon.Full : Inet.Full; } }
        public bool SameVersion { get { return Lan != null ? Lan.Beacon.SameVersion : Inet.SameVersion; } }
        public bool ThisPc;
        public bool CanJoin { get { return !ThisPc && !Full && SameVersion; } }
        public string Where { get { return Lan != null ? "LAN" : "Internet"; } }
        public string Key { get { return Lan != null ? "lan:" + Lan.Beacon.HostId : "inet:" + Inet.Id; } }
    }

    class HostReply
    {
        public bool Ok;
        public string Error = "", Id = "", Key = "", Relay = "", Token = "";
        public List<string> Joins = new List<string>();
    }

    class JoinReply
    {
        public bool Ok;
        public string Error = "", HostAddr = "", Relay = "", Token = "";
    }

    /// <summary>%APPDATA%\Spidy\MP-PUBLIC.txt: the game's own report (every 3 s while multiplayer is on).</summary>
    class PublicInfo
    {
        public string Public = "", Friend = "";
        public int Port, Players = 1, Hero = 255;
        public bool Relayed, Fresh;
    }

    static class InternetLobby
    {
        /// <summary>CONFIG: the deployed lobby (server/lobby/README.md), e.g. "https://herovr-lobby.NAME.workers.dev".
        /// Empty: no internet list (LAN only). SM2VR_LOBBY_URL or the settings key LobbyUrl override it.</summary>
        public const string DefaultUrl = "";
        public const string Protocol = "HVL1";
        public const int TimeoutMs = 6000;
        public const double FreshSeconds = 10;

        public static string Url(Settings s)
        {
            string u = Environment.GetEnvironmentVariable("SM2VR_LOBBY_URL");
            if (string.IsNullOrEmpty(u) && s != null) u = s.Get("LobbyUrl", null);
            if (string.IsNullOrEmpty(u)) u = DefaultUrl;
            u = (u ?? "").Trim().TrimEnd(new[] { '/' });
            return u.StartsWith("https://", StringComparison.OrdinalIgnoreCase) || u.StartsWith("http://127.0.0.1", StringComparison.OrdinalIgnoreCase) ||
                   u.StartsWith("http://localhost", StringComparison.OrdinalIgnoreCase) ? u : "";
        }

        public static string Form(params string[] kv)
        {
            var sb = new StringBuilder();
            for (int i = 0; i + 1 < kv.Length; i += 2)
            {
                if (kv[i + 1] == null) continue;
                if (sb.Length > 0) sb.Append('&');
                sb.Append(Uri.EscapeDataString(kv[i])).Append('=').Append(Uri.EscapeDataString(kv[i + 1]));
            }
            return sb.ToString();
        }

        /// <summary>One request; the body (also of an error status), or null when the lobby could not be reached.</summary>
        public static string Http(string method, string url, string form, out string problem)
        {
            problem = "";
            try
            {
                try { ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072; } catch { }   // TLS 1.2 on .NET 4.x
                var req = (HttpWebRequest)WebRequest.Create(url);
                req.Method = method;
                req.Timeout = TimeoutMs;
                req.ReadWriteTimeout = TimeoutMs;
                req.UserAgent = "SM2VRLauncher/" + LauncherInfo.Version;
                req.Proxy = WebRequest.DefaultWebProxy;
                if (method == "POST")
                {
                    byte[] body = Encoding.UTF8.GetBytes(form ?? "");
                    req.ContentType = "application/x-www-form-urlencoded";
                    req.ContentLength = body.Length;
                    using (var s = req.GetRequestStream()) s.Write(body, 0, body.Length);
                }
                HttpWebResponse resp;
                try { resp = (HttpWebResponse)req.GetResponse(); }
                catch (WebException we)
                {
                    resp = we.Response as HttpWebResponse;
                    if (resp == null) { problem = we.Message; return null; }
                }
                using (resp)
                using (var r = new StreamReader(resp.GetResponseStream(), Encoding.UTF8))
                {
                    var buf = new char[65536];
                    int n = r.ReadBlock(buf, 0, buf.Length);
                    return new string(buf, 0, n);
                }
            }
            catch (Exception ex) { problem = ex.Message; return null; }
        }

        static int Int(string s, int lo, int hi, int fallback)
        {
            int v;
            return int.TryParse(s, NumberStyles.Integer, CultureInfo.InvariantCulture, out v) && v >= lo && v <= hi ? v : fallback;
        }

        static string[] Lines(string text)
        {
            return (text ?? "").Replace("\r", "").Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);
        }

        /// <summary>GET /v1/list -> the games, or null when it is not a list (an old or broken lobby).</summary>
        public static List<InetGame> ParseList(string text)
        {
            var lines = Lines(text);
            if (lines.Length == 0 || lines[0].Trim() != Protocol) return null;
            var r = new List<InetGame>();
            foreach (var l in lines.Skip(1))
            {
                var f = l.Split(new[] { '\t' });
                if (f.Length < 7 || f[0].Length == 0) continue;
                var g = new InetGame
                {
                    Id = f[0], Name = Lobby.CleanName(f[1]), Hero = Int(f[2], 0, 255, 255), HeroName = f[3].Trim().ToUpperInvariant(),
                    Players = Int(f[4], 1, 16, 1), MaxPlayers = Int(f[5], 1, 16, 2), Protocol = Int(f[6], 0, 255, 0),
                };
                if (g.HeroName.Length > 16) g.HeroName = g.HeroName.Substring(0, 16);
                uint lan;
                if (f.Length > 7 && uint.TryParse(f[7], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out lan)) g.LanId = lan;
                r.Add(g);
                if (r.Count >= 200) break;
            }
            return r;
        }

        static string Err(string[] lines)
        {
            if (lines.Length == 0) return "no answer";
            var f = lines[0].Split(new[] { '\t' });
            return f[0] == "err" && f.Length > 1 ? f[1] : lines[0];
        }

        static bool Relay(string line, ref string relay, ref string token)
        {
            var f = line.Split(new[] { '\t' });
            if (f.Length < 3 || f[0] != "relay") return false;
            relay = f[1].Trim(); token = f[2].Trim();
            return true;
        }

        public static HostReply ParseHost(string text)
        {
            var lines = Lines(text);
            var r = new HostReply();
            var f = lines.Length > 0 ? lines[0].Split(new[] { '\t' }) : new string[0];
            if (f.Length < 3 || f[0] != "ok") { r.Error = Err(lines); return r; }
            r.Ok = true; r.Id = f[1]; r.Key = f[2];
            foreach (var l in lines.Skip(1))
            {
                if (Relay(l, ref r.Relay, ref r.Token)) continue;
                var j = l.Split(new[] { '\t' });
                if (j.Length >= 2 && j[0] == "join" && ValidAddr(j[1]) && !r.Joins.Contains(j[1]) && r.Joins.Count < 8) r.Joins.Add(j[1]);
            }
            return r;
        }

        public static JoinReply ParseJoin(string text)
        {
            var lines = Lines(text);
            var r = new JoinReply();
            var f = lines.Length > 0 ? lines[0].Split(new[] { '\t' }) : new string[0];
            if (f.Length < 2 || f[0] != "ok" || !ValidAddr(f[1])) { r.Error = Err(lines); return r; }
            r.Ok = true; r.HostAddr = f[1];
            foreach (var l in lines.Skip(1)) Relay(l, ref r.Relay, ref r.Token);
            return r;
        }

        /// <summary>a.b.c.d:port, numbers only (what the game's MP-LOBBY.txt reader takes).</summary>
        public static bool ValidAddr(string s)
        {
            if (string.IsNullOrEmpty(s)) return false;
            int c = s.LastIndexOf(':');
            IPAddress a;
            return c > 0 && Int(s.Substring(c + 1), 1, 65535, 0) > 0 && s.Substring(0, c).Split(new[] { '.' }).Length == 4 &&
                IPAddress.TryParse(s.Substring(0, c), out a) && a.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork;
        }

        public static string PublicPath(string folder) { return Path.Combine(folder ?? Lobby.Folder(), "MP-PUBLIC.txt"); }

        public static PublicInfo ReadPublic(string folder, DateTime nowUtc)
        {
            var p = new PublicInfo();
            try
            {
                string path = PublicPath(folder);
                if (!File.Exists(path)) return p;
                p.Fresh = (nowUtc - File.GetLastWriteTimeUtc(path)).TotalSeconds < FreshSeconds;
                foreach (var raw in File.ReadAllLines(path))
                {
                    int eq = raw.IndexOf('=');
                    if (raw.StartsWith("#") || eq <= 0) continue;
                    string k = raw.Substring(0, eq).Trim(), v = raw.Substring(eq + 1).Trim();
                    if (k == "public") p.Public = ValidAddr(v) ? v : "";
                    else if (k == "port") p.Port = Int(v, 0, 65535, 0);
                    else if (k == "players") p.Players = Int(v, 1, 16, 1);
                    else if (k == "friend") p.Friend = v;
                    else if (k == "relayed") p.Relayed = v == "1";
                    else if (k == "hero") p.Hero = Int(v, 0, 255, 255);
                }
            }
            catch { }
            return p;
        }

        /// <summary>LAN games first (a game heard on this network is joined directly), then internet games not heard
        /// here; mine (this PC's beacon ids) are marked so JOIN is off for them. Joinable before full / other version.</summary>
        public static List<ServerEntry> Merge(IList<LobbyHost> lan, IList<InetGame> inet, ICollection<uint> mine)
        {
            var r = new List<ServerEntry>();
            var lanIds = new HashSet<uint>();
            foreach (var h in lan ?? new LobbyHost[0])
            {
                lanIds.Add(h.Beacon.HostId);
                r.Add(new ServerEntry { Lan = h, ThisPc = h.ThisPc });
            }
            foreach (var g in inet ?? new InetGame[0])
            {
                if (g.LanId != 0 && lanIds.Contains(g.LanId)) continue;
                r.Add(new ServerEntry { Inet = g, ThisPc = g.LanId != 0 && mine != null && mine.Contains(g.LanId) });
            }
            return r.Select((e, i) => new { e, i }).OrderBy(x => x.e.ThisPc ? 0 : x.e.CanJoin ? 1 : 2).ThenBy(x => x.i).Select(x => x.e).ToList();
        }

        public static string RowText(ServerEntry e)
        {
            string hero = string.IsNullOrEmpty(e.HeroName) ? "-" : e.HeroName;
            return e.Name + "   " + hero + "   " + e.Players + "/" + e.MaxPlayers + (e.Full ? " FULL" : "") +
                (e.SameVersion ? "" : "   (another mod version)") + "   " + e.Where + (e.ThisPc ? "   (you)" : "");
        }

        /// <summary>MP-LOBBY.txt for a host: the LAN lines plus what the internet lobby said (punch / relay / token).</summary>
        public static string HostText(string name, IList<string> punch, string relay, string token)
        {
            var sb = new StringBuilder(Lobby.HostText(name));
            foreach (var p in punch ?? new string[0]) sb.Append("punch=").Append(p).Append("\r\n");
            AppendRelay(sb, relay, token);
            return sb.ToString();
        }

        /// <summary>MP-LOBBY.txt for an internet join (inet= tells the launcher which listing to keep posting to).</summary>
        public static string JoinText(string name, InetGame g, JoinReply j)
        {
            var sb = new StringBuilder("# HERO VR multiplayer (written by the launcher's server list; LEAVE deletes it)\r\nmode=join\r\njoin=" + j.HostAddr +
                "\r\nname=" + Lobby.CleanName(name) + "\r\nhost=" + Lobby.CleanName(g.Name) + "\r\ninet=" + g.Id + "\r\n");
            AppendRelay(sb, j.Relay, j.Token);
            return sb.ToString();
        }

        static void AppendRelay(StringBuilder sb, string relay, string token)
        {
            if (string.IsNullOrEmpty(relay) || string.IsNullOrEmpty(token) || token.Length != 16) return;
            sb.Append("relay=").Append(relay).Append("\r\ntoken=").Append(token).Append("\r\n");
        }
    }

    /// <summary>The internet side, on its own thread (the window never waits on the network): the list every 15 s
    /// (or on Refresh), the host heartbeat and the joiner's address. The window reads Snapshot / takes Messages.</summary>
    class InternetSession : IDisposable
    {
        public readonly string Url, Folder;
        public double ListEvery = 15, HostEvery = 5, HostFullEvery = 30, JoinEvery = 5, StepMs = 500;
        readonly object gate = new object();
        readonly Queue<string> messages = new Queue<string>();
        readonly AutoResetEvent wake = new AutoResetEvent(false);
        Thread thread;
        volatile bool stop, refresh = true;
        List<InetGame> games = new List<InetGame>();
        string listProblem = "";
        DateTime listedAt = DateTime.MinValue;
        // host
        string hostId = "", hostKey = "", lastHostText = "";
        DateTime nextHost = DateTime.MinValue;
        string localHeroName = "";
        uint localLanId;
        // join
        InetGame joinPick;
        string joinName = "";
        DateTime nextJoin = DateTime.MinValue;

        public InternetSession(string url, string folder) { Url = url; Folder = folder; }
        public bool On { get { return Url.Length > 0; } }

        public void Start()
        {
            if (!On || thread != null) return;
            thread = new Thread(Run) { IsBackground = true, Name = "HERO VR internet lobby" };
            thread.Start();
        }

        public void Refresh() { refresh = true; wake.Set(); }

        public List<InetGame> Games(out string problem, out DateTime at)
        {
            lock (gate) { problem = listProblem; at = listedAt; return new List<InetGame>(games); }
        }

        public List<string> TakeMessages()
        {
            lock (gate) { var r = messages.ToList(); messages.Clear(); return r; }
        }

        void Say(string m) { lock (gate) { if (messages.Count < 50) messages.Enqueue(m); } }

        /// <summary>What this PC's own game beacon says (the window hears it on the LAN listener).</summary>
        public void SetLocal(string heroName, uint lanId) { lock (gate) { localHeroName = heroName ?? ""; localLanId = lanId; } }

        public string HostId { get { lock (gate) return hostId; } }

        /// <summary>JOIN pressed on an internet row: ask the lobby for the host (background), then MP-LOBBY.txt.</summary>
        public void Join(InetGame g, string name)
        {
            lock (gate) { joinPick = g; joinName = name; nextJoin = DateTime.MinValue; }
            wake.Set();
        }

        void Run()
        {
            while (!stop)
            {
                try { Step(DateTime.UtcNow); }
                catch (Exception ex) { Say("Internet list: " + ex.Message); }
                wake.WaitOne((int)StepMs);
            }
        }

        /// <summary>One pass (public for tests).</summary>
        public void Step(DateTime now)
        {
            string problem;
            if (refresh || (now - listedAt).TotalSeconds >= ListEvery)
            {
                refresh = false;
                string body = InternetLobby.Http("GET", Url + "/v1/list", null, out problem);
                var list = body == null ? null : InternetLobby.ParseList(body);
                lock (gate)
                {
                    listedAt = now;
                    if (list != null) { games = list; listProblem = ""; }
                    else listProblem = body == null ? "cannot reach the server list (" + problem + ")" : "the server list answered something else";
                }
            }
            var file = Lobby.Read(Folder);
            string mode, inet;
            file.TryGetValue("mode", out mode); file.TryGetValue("inet", out inet);
            mode = (mode ?? "").ToLowerInvariant();
            var pub = InternetLobby.ReadPublic(Folder, now);
            InetGame pick;
            lock (gate) { pick = joinPick; joinPick = null; }
            if (pick != null) StartJoin(pick, now);
            HostStep(mode, file, pub, now);
            if (mode == "join" && !string.IsNullOrEmpty(inet) && pub.Fresh && pub.Public.Length > 0 && pub.Friend.Length == 0 && now >= nextJoin)
            {
                nextJoin = now.AddSeconds(JoinEvery);
                string name;
                file.TryGetValue("name", out name);
                string body = InternetLobby.Http("POST", Url + "/v1/join", InternetLobby.Form("id", inet, "name", name ?? "", "addr", pub.Public,
                    "protocol", LobbyBeacon.MpProtocol.ToString(CultureInfo.InvariantCulture)), out problem);
                var r = body == null ? null : InternetLobby.ParseJoin(body);
                if (r != null && !r.Ok && (r.Error == "gone" || r.Error == "full")) Say("Internet: that game is " + r.Error + " now.");
            }
        }

        void StartJoin(InetGame g, DateTime now)
        {
            string problem, name;
            lock (gate) name = joinName;
            string body = InternetLobby.Http("POST", Url + "/v1/join", InternetLobby.Form("id", g.Id, "name", Lobby.CleanName(name),
                "protocol", LobbyBeacon.MpProtocol.ToString(CultureInfo.InvariantCulture)), out problem);
            if (body == null) { Say("Internet: cannot reach the server list (" + problem + ") - not joined."); return; }
            var r = InternetLobby.ParseJoin(body);
            if (!r.Ok)
            {
                Say(r.Error == "full" ? "Internet: " + g.Name + " is full." : r.Error == "gone" ? "Internet: " + g.Name + " is not hosting any more." :
                    r.Error == "version" ? "Internet: " + g.Name + " runs another mod version." : "Internet: not joined (" + r.Error + ").");
                return;
            }
            try
            {
                Lobby.Write(Folder, InternetLobby.JoinText(name, g, r));
                nextJoin = DateTime.MinValue;
                Say("OK:Internet: joining " + g.Name + ". Start the game (PLAY VR); it connects by itself" +
                    (r.Relay.Length > 0 ? " (through the relay if the routers do not let it in directly)." : "."));
            }
            catch (Exception ex) { Say("Internet: cannot write MP-LOBBY.txt: " + ex.Message); }
        }

        void HostStep(string mode, Dictionary<string, string> file, PublicInfo pub, DateTime now)
        {
            string problem;
            if (mode != "host" || !pub.Fresh)
            {
                string id, key;
                lock (gate) { id = hostId; key = hostKey; hostId = hostKey = ""; }
                lastHostText = "";
                if (id.Length > 0)
                    InternetLobby.Http("POST", Url + "/v1/leave", InternetLobby.Form("id", id, "key", key), out problem);
                return;
            }
            if (pub.Public.Length == 0 || now < nextHost) return;
            nextHost = now.AddSeconds(pub.Players >= 2 ? HostFullEvery : HostEvery);
            string name, heroName, myId, myKey;
            uint lanId;
            file.TryGetValue("name", out name);
            lock (gate) { heroName = localHeroName; lanId = localLanId; myId = hostId; myKey = hostKey; }
            string body = InternetLobby.Http("POST", Url + "/v1/host", InternetLobby.Form(
                "name", name ?? "", "hero", pub.Hero.ToString(CultureInfo.InvariantCulture), "heroName", heroName,
                "players", pub.Players.ToString(CultureInfo.InvariantCulture), "max", "2",
                "protocol", LobbyBeacon.MpProtocol.ToString(CultureInfo.InvariantCulture), "addr", pub.Public,
                "lan", lanId == 0 ? null : lanId.ToString("x8", CultureInfo.InvariantCulture),
                "id", myId.Length > 0 ? myId : null, "key", myKey.Length > 0 ? myKey : null), out problem);
            if (body == null) return;   // offline for a moment: the LAN keeps working, try again next time
            var r = InternetLobby.ParseHost(body);
            if (!r.Ok)
            {
                if (r.Error == "key") lock (gate) { hostId = hostKey = ""; }
                Say("Internet: not listed (" + r.Error + ").");
                return;
            }
            bool first;
            lock (gate) { first = hostId.Length == 0; hostId = r.Id; hostKey = r.Key; }
            if (first) Say("OK:Internet: your game is on the server list as " + Lobby.CleanName(name) + ".");
            string text = InternetLobby.HostText(name, r.Joins, r.Relay, r.Token);
            if (text != lastHostText)
            {
                lastHostText = text;
                try { Lobby.Write(Folder, text); } catch (Exception ex) { Say("Internet: cannot write MP-LOBBY.txt: " + ex.Message); }
            }
        }

        public void Dispose()
        {
            stop = true;
            wake.Set();
            if (thread != null) thread.Join(TimeSpanMs(InternetLobby.TimeoutMs + 1000));
            // A host that closes the launcher: take it off the list now (else it drops out after 45 s).
            string id, key, problem;
            lock (gate) { id = hostId; key = hostKey; hostId = ""; }
            if (id.Length > 0) InternetLobby.Http("POST", Url + "/v1/leave", InternetLobby.Form("id", id, "key", key), out problem);
        }

        static TimeSpan TimeSpanMs(int ms) { return TimeSpan.FromMilliseconds(ms); }
    }
}
