// SM2 VR Launcher - 0.0.74 MP LOBBY: the Multiplayer sheet (HOST, the games on this network, JOIN). No IP typing:
// a hosting game announces itself on the LAN (Lobby.cs); JOIN writes %APPDATA%\Spidy\MP-LOBBY.txt for the game.
// 0.0.74 MP INTERNET: it is the server list - the window opens on it, LAN games and the internet list
// (InternetLobby.cs) merged, listening from the start; the "No / Yes, Autosetup" pop-up (Autosetup.cs) on start.
// Made by Agent Mulder.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace Sm2VrLauncher
{
    partial class MainForm
    {
        const int SheetMultiplayer = 5;
        const int MpRows = 5;
        LobbyListener mpListener;
        readonly System.Windows.Forms.Timer mpTick = new System.Windows.Forms.Timer { Interval = 1000 };
        Field mpName;
        TextLabel mpState, mpListHead, mpThisPc;
        readonly TextLabel[] mpRowText = new TextLabel[MpRows];
        readonly AccentButton[] mpRowJoin = new AccentButton[MpRows];
        readonly ServerEntry[] mpRowHost = new ServerEntry[MpRows];
        AccentButton mpHost, mpLeave, btnMultiplayer, mpRefreshBtn;
        InternetSession mpInet;
        bool mpStarted;
        string mpFailSaid = "";   // 0.0.74 MP FREE LOBBY: the try= whose "could not connect" line is already in the log

        static Rectangle MultiplayerBounds() { return Theme.R(170, 196, 840, 436); }

        void BuildMultiplayer()
        {
            var card = Add(pages[SheetMultiplayer], new GlassPanel("Multiplayer - servers"), 0, 0, 840, 436);
            Add(card, new TextLabel("Your name", Theme.Ui(12), Theme.Sub) { Wrap = false }, 20, 56, 90, 22);
            mpName = Add(card, new Field(), 110, 50, 200, 34);
            mpName.Text = settings.Get("MpName", Lobby.CleanName(Environment.UserName));
            mpName.Box.MaxLength = 16;
            mpName.Box.TextChanged += (s, e) => { settings.Set("MpName", mpName.Text); settings.Save(); };
            mpHost = Add(card, new AccentButton("Host", ButtonKind.Primary) { Glyph = "play", TextFont = Theme.UiBold(13) }, 326, 48, 130, 38);
            mpLeave = Add(card, new AccentButton("Leave", ButtonKind.Secondary) { Glyph = "stop" }, 466, 48, 120, 38);
            mpState = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub), 600, 46, 222, 44);
            mpRefreshBtn = Add(card, new AccentButton("Refresh", ButtonKind.Ghost) { Glyph = "refresh", TextFont = Theme.UiBold(11.5f) }, 690, 100, 130, 28);
            mpRefreshBtn.Click += (s, e) => { if (mpInet != null) mpInet.Refresh(); MpRefresh(); };
            mpHost.Click += (s, e) => MpHost();
            mpLeave.Click += (s, e) => MpLeave();
            tip.SetToolTip(mpHost, "Your game shows up in everyone's server list while it runs (same network and internet). You do not need to type any address.");
            tip.SetToolTip(mpLeave, "Stops hosting or joining (deletes MP-LOBBY.txt). MP-PEER.txt, the typed-address fallback, is left alone.");

            mpListHead = Add(card, new TextLabel("Servers", Theme.UiBold(12.5f), Theme.Text) { Wrap = false }, 20, 104, 660, 22);
            for (int i = 0; i < MpRows; i++)
            {
                int y = 132 + i * 42;
                mpRowText[i] = Add(card, new TextLabel("", Theme.Ui(12.5f), Theme.Text) { Wrap = false }, 20, y + 9, 660, 22);
                mpRowJoin[i] = Add(card, new AccentButton("Join", ButtonKind.Secondary) { Glyph = "play", Visible = false }, 690, y, 130, 36);
                int row = i;
                mpRowJoin[i].Click += (s, e) => MpJoinEntry(mpRowHost[row]);
            }
            mpThisPc = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub) { Wrap = false }, 20, 350, 560, 20);
            var fw = Add(card, new AccentButton("Allow through firewall", ButtonKind.Ghost) { TextFont = Theme.UiBold(11.5f) }, 620, 344, 200, 30);
            fw.Click += (s, e) => MpAutosetup(true);
            tip.SetToolTip(fw, "Multiplayer setup (asks Windows once): one firewall rule, UDP " + Autosetup.PortRange + " in, nothing else.");
            Add(card, new TextLabel(Lobby.FirewallNote + " Internet games join by themselves; the advanced fallback is MP-PEER.txt (docs\\MP-SOLO-TEST.md).", Theme.Ui(11), Theme.Dim), 20, 378, 800, 44);
            mpTick.Tick += (s, e) => MpRefresh();
        }

        // 0.0.74 MP INTERNET: the list runs from the start (MultiplayerStart from OnShown), not only while the sheet is open.
        void MultiplayerShown(bool visible)
        {
            MultiplayerStart();
            if (visible) MpRefresh();
        }

        void MultiplayerStart()
        {
            if (mpStarted) return;
            mpStarted = true;
            if (mpListener == null && preview)
            {
                // SM2VR_LAUNCHER_PREVIEW: two made-up hosts (documentation addresses) for the screenshot.
                mpListener = new LobbyListener(LobbyBeacon.LobbyPort) { ForgetSeconds = 1e9 };
                mpListener.Heard(System.Net.IPAddress.Parse("192.0.2.20"), new LobbyBeacon { Protocol = 1, Port = 39470, Players = 1, MaxPlayers = 2, Hero = 0, HostId = 1, Name = "MARY", HeroName = "SPIDER-MAN" }, DateTime.UtcNow, null);
                mpListener.Heard(System.Net.IPAddress.Parse("192.0.2.31"), new LobbyBeacon { Protocol = 1, Port = 39470, Players = 2, MaxPlayers = 2, Hero = 1, HostId = 2, Name = "BOB", HeroName = "IRON MAN" }, DateTime.UtcNow, null);
            }
            if (mpListener == null && !preview)
            {
                mpListener = new LobbyListener(LobbyBeacon.LobbyPort);
                if (!mpListener.Start()) sessionLog.Line("Multiplayer: cannot listen for games on UDP " + LobbyBeacon.LobbyPort + " (" + mpListener.Error + ")", Theme.Warn);
            }
            if (!preview)
            {
                mpInet = new InternetSession(InternetLobby.Url(settings), null);
                mpInet.Start();
                if (!mpInet.On) sessionLog.Line("Multiplayer: no internet server list is set up in this build - games on this network only.", Theme.Dim);
            }
            MpRefresh();
            mpTick.Start();
        }

        /// <summary>The pop-up: at start when something is missing and the player did not say No before; from the
        /// sheet's button always (force).</summary>
        void MpAutosetup(bool force)
        {
            if (preview) return;
            var need = Autosetup.Needed(Autosetup.All());
            if (need.Count == 0) { if (force) sessionLog.Line("Multiplayer: this PC is set up (firewall rule '" + Autosetup.FirewallRule + "' is there).", Theme.Good); return; }
            if (!force && settings.GetBool(Autosetup.KeyDeclined, false)) return;
            DialogResult r;
            using (var d = new AutosetupDialog(need)) r = d.ShowDialog(this);
            if (r != DialogResult.Yes)
            {
                settings.SetBool(Autosetup.KeyDeclined, true); settings.Save();
                sessionLog.Line("Multiplayer setup skipped. Multiplayer > Allow through firewall offers it again.", Theme.Sub);
                return;
            }
            settings.SetBool(Autosetup.KeyDeclined, false); settings.Save();
            foreach (var l in Autosetup.Run(need)) sessionLog.Line("Multiplayer setup: " + l, l.StartsWith("Done") ? Theme.Good : Theme.Warn);
        }

        void MpRefresh()
        {
            if (mpState == null) return;
            var mine = Lobby.LocalAddresses();
            mpThisPc.Text = mine.Count == 0 ? "This PC: no network address found" : "This PC: " + string.Join(", ", mine.Select(a => a.ToString()).ToArray());
            var file = Lobby.Read(null);
            string mode;
            file.TryGetValue("mode", out mode);
            mode = (mode ?? "").ToLowerInvariant();
            if (mode == "host") { mpState.Text = "Hosting: start the game (PLAY VR) and your friend sees you in his list."; mpState.ForeColor = Theme.Good; }
            else if (mode == "join")
            {
                string h, at;
                file.TryGetValue("host", out h); file.TryGetValue("join", out at);
                mpState.Text = "Joining " + (string.IsNullOrEmpty(h) ? at : h) + ": start the game (PLAY VR)."; mpState.ForeColor = Theme.Good;
                // 0.0.74 MP FREE LOBBY: the game's call got no answer in 30 s and stopped - one clear line, no hanging.
                var pub = InternetLobby.ReadPublic(null, DateTime.UtcNow);
                if (pub.Fresh && pub.Failed && pub.Friend.Length == 0)
                {
                    string t, relay;
                    file.TryGetValue("try", out t); file.TryGetValue("relay", out relay);
                    mpState.Text = InternetLobby.JoinFailText(h, !string.IsNullOrEmpty(relay));
                    mpState.ForeColor = Theme.Bad;
                    if ((t ?? "") != mpFailSaid) { mpFailSaid = t ?? ""; sessionLog.Line("Multiplayer: " + mpState.Text, Theme.Bad); }
                }
                else if (pub.Fresh && pub.Friend.Length > 0)
                    mpState.Text = "Playing with " + pub.Friend + (pub.Relayed ? " (through the relay)." : ".");
            }
            else { mpState.Text = "Not in a game. HOST, or JOIN one below."; mpState.ForeColor = Theme.Sub; }
            mpLeave.Enabled = mode == "host" || mode == "join";

            var hosts = mpListener == null ? new List<LobbyHost>() : mpListener.Snapshot(DateTime.UtcNow);
            var mineIds = hosts.Where(h => h.ThisPc).Select(h => h.Beacon.HostId).ToList();
            string inetProblem = "";
            var inet = new List<InetGame>();
            if (mpInet != null && mpInet.On)
            {
                DateTime at;
                inet = mpInet.Games(out inetProblem, out at);
                var me = hosts.FirstOrDefault(h => h.ThisPc);
                mpInet.SetLocal(me == null ? "" : me.Beacon.HeroName, me == null ? 0u : me.Beacon.HostId);
                foreach (var m in mpInet.TakeMessages())
                    sessionLog.Line(m.StartsWith("OK:") ? m.Substring(3) : m, m.StartsWith("OK:") ? Theme.Good : Theme.Warn);
            }
            var rows = InternetLobby.Merge(hosts, inet, mineIds);
            int lanN = rows.Count(r => !r.Internet), netN = rows.Count - lanN;
            mpListHead.Text = MpHeadText(lanN, netN, mpInet != null && mpInet.On, inetProblem);
            for (int i = 0; i < MpRows; i++)
            {
                var h = i < rows.Count ? rows[i] : null;
                mpRowHost[i] = h;
                mpRowText[i].Text = h == null ? "" : InternetLobby.RowText(h);
                mpRowJoin[i].Visible = h != null;
                if (h != null)
                {
                    mpRowJoin[i].Enabled = h.CanJoin;
                    mpRowJoin[i].Text = h.ThisPc ? "You" : h.Full ? "Full" : !h.SameVersion ? "Update" : "Join";
                }
            }
        }

        internal static string MpHeadText(int lan, int internet, bool inetOn, string problem)
        {
            string n = lan + internet == 0 ? "none yet (a friend presses HOST and starts his game)" : (lan + internet) + (lan + internet == 1 ? " game" : " games");
            string where = !inetOn ? " - this network only" : problem.Length > 0 ? " - this network only: " + problem : "";
            return "Servers: " + n + where;
        }

        internal static string MpRowText(LobbyHost h)
        {
            var b = h.Beacon;
            return (b.Name.Length == 0 ? "HERO VR" : b.Name) + "   " + (b.HeroName.Length == 0 ? "-" : b.HeroName) + "   " + b.Players + "/" + b.MaxPlayers +
                (b.Full ? " FULL" : "") + (b.SameVersion ? "" : "   (another mod version)") + "   " + h.Address + (h.ThisPc ? "   (this PC)" : "");
        }

        void MpHost()
        {
            try
            {
                Lobby.Write(null, Lobby.HostText(mpName.Text));
                sessionLog.Line("Multiplayer: hosting as " + Lobby.CleanName(mpName.Text) + " - your friend picks you in his Multiplayer list. " + Lobby.FirewallNote, Theme.Good);
            }
            catch (Exception ex) { sessionLog.Line("Multiplayer: cannot write MP-LOBBY.txt: " + ex.Message, Theme.Bad); }
            MpRefresh();
        }

        void MpJoinEntry(ServerEntry e)
        {
            if (e == null || !e.CanJoin) return;
            if (!e.Internet) { MpJoin(e.Lan); return; }
            if (mpInet == null) return;
            sessionLog.Line("Multiplayer: asking the server list for " + e.Name + "...", Theme.Sub);
            mpInet.Join(e.Inet, mpName.Text);
        }

        void MpJoin(LobbyHost h)
        {
            if (h == null) return;
            try
            {
                Lobby.Write(null, Lobby.JoinText(mpName.Text, h));
                sessionLog.Line("Multiplayer: joining " + h.Beacon.Name + " (" + h.Endpoint + "). Start the game; it calls the host by itself.", Theme.Good);
            }
            catch (Exception ex) { sessionLog.Line("Multiplayer: cannot write MP-LOBBY.txt: " + ex.Message, Theme.Bad); }
            MpRefresh();
        }

        void MpLeave()
        {
            try { Lobby.Leave(null); sessionLog.Line("Multiplayer: left (the game stops its network within 2 s).", Theme.Sub); }
            catch (Exception ex) { sessionLog.Line("Multiplayer: cannot delete MP-LOBBY.txt: " + ex.Message, Theme.Bad); }
            MpRefresh();
        }

        void DisposeMultiplayer()
        {
            mpTick.Stop();
            if (mpListener != null) mpListener.Dispose();
            mpListener = null;
            if (mpInet != null) mpInet.Dispose();
            mpInet = null;
        }
    }
}
