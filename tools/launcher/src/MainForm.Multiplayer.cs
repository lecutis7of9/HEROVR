// SM2 VR Launcher - 0.0.74 MP LOBBY: the Multiplayer sheet (HOST, the games on this network, JOIN). No IP typing:
// a hosting game announces itself on the LAN (Lobby.cs); JOIN writes %APPDATA%\Spidy\MP-LOBBY.txt for the game.
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
        readonly LobbyHost[] mpRowHost = new LobbyHost[MpRows];
        AccentButton mpHost, mpLeave, btnMultiplayer;

        static Rectangle MultiplayerBounds() { return Theme.R(170, 196, 840, 436); }

        void BuildMultiplayer()
        {
            var card = Add(pages[SheetMultiplayer], new GlassPanel("Multiplayer - same network"), 0, 0, 840, 436);
            Add(card, new TextLabel("Your name", Theme.Ui(12), Theme.Sub) { Wrap = false }, 20, 56, 90, 22);
            mpName = Add(card, new Field(), 110, 50, 200, 34);
            mpName.Text = settings.Get("MpName", Lobby.CleanName(Environment.UserName));
            mpName.Box.MaxLength = 16;
            mpName.Box.TextChanged += (s, e) => { settings.Set("MpName", mpName.Text); settings.Save(); };
            mpHost = Add(card, new AccentButton("Host", ButtonKind.Primary) { Glyph = "play", TextFont = Theme.UiBold(13) }, 326, 48, 130, 38);
            mpLeave = Add(card, new AccentButton("Leave", ButtonKind.Secondary) { Glyph = "stop" }, 466, 48, 120, 38);
            mpState = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub), 600, 46, 222, 44);
            mpHost.Click += (s, e) => MpHost();
            mpLeave.Click += (s, e) => MpLeave();
            tip.SetToolTip(mpHost, "Your game shows up in the list of friends on the same Wi-Fi / router while it runs. You do not need to type any address.");
            tip.SetToolTip(mpLeave, "Stops hosting or joining (deletes MP-LOBBY.txt). MP-PEER.txt, the typed-address fallback, is left alone.");

            mpListHead = Add(card, new TextLabel("Games on this network", Theme.UiBold(12.5f), Theme.Text) { Wrap = false }, 20, 104, 800, 22);
            for (int i = 0; i < MpRows; i++)
            {
                int y = 132 + i * 42;
                mpRowText[i] = Add(card, new TextLabel("", Theme.Ui(12.5f), Theme.Text) { Wrap = false }, 20, y + 9, 660, 22);
                mpRowJoin[i] = Add(card, new AccentButton("Join", ButtonKind.Secondary) { Glyph = "play", Visible = false }, 690, y, 130, 36);
                int row = i;
                mpRowJoin[i].Click += (s, e) => MpJoin(mpRowHost[row]);
            }
            mpThisPc = Add(card, new TextLabel("", Theme.Ui(11.5f), Theme.Sub) { Wrap = false }, 20, 350, 560, 20);
            var fw = Add(card, new AccentButton("Allow through firewall", ButtonKind.Ghost) { TextFont = Theme.UiBold(11.5f) }, 620, 344, 200, 30);
            fw.Click += (s, e) => MpFirewall();
            tip.SetToolTip(fw, "Adds one Windows Firewall rule (asks for admin): UDP " + LobbyBeacon.LobbyPort + "-" + (LobbyBeacon.GamePort + 4) + " in, Private networks only.");
            Add(card, new TextLabel(Lobby.FirewallNote + " Over the internet: MP-PEER.txt (docs\\MP-SOLO-TEST.md).", Theme.Ui(11), Theme.Dim), 20, 378, 800, 44);
            mpTick.Tick += (s, e) => MpRefresh();
        }

        // The tick runs while the sheet is open: the listener starts the first time it is.
        void MultiplayerShown(bool visible)
        {
            if (!visible) { mpTick.Stop(); return; }
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
            MpRefresh();
            mpTick.Start();
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
            }
            else { mpState.Text = "Not in a game. HOST, or JOIN one below."; mpState.ForeColor = Theme.Sub; }
            mpLeave.Enabled = mode == "host" || mode == "join";

            var hosts = mpListener == null ? new List<LobbyHost>() : mpListener.Snapshot(DateTime.UtcNow);
            mpListHead.Text = hosts.Count == 0 ? "Games on this network: none yet (a friend presses HOST and starts his game)" : "Games on this network";
            for (int i = 0; i < MpRows; i++)
            {
                var h = i < hosts.Count ? hosts[i] : null;
                mpRowHost[i] = h;
                mpRowText[i].Text = h == null ? "" : MpRowText(h);
                mpRowJoin[i].Visible = h != null;
                if (h != null)
                {
                    mpRowJoin[i].Enabled = !h.ThisPc && !h.Beacon.Full && h.Beacon.SameVersion;
                    mpRowJoin[i].Text = h.ThisPc ? "You" : h.Beacon.Full ? "Full" : !h.Beacon.SameVersion ? "Update" : "Join";
                }
            }
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

        void MpFirewall()
        {
            try
            {
                var psi = new ProcessStartInfo("netsh", Lobby.FirewallArgs()) { Verb = "runas", UseShellExecute = true, WindowStyle = ProcessWindowStyle.Hidden };
                using (var p = Process.Start(psi)) { if (p != null) p.WaitForExit(15000); }
                sessionLog.Line("Multiplayer: firewall rule '" + Lobby.FirewallRule + "' added (Private networks). If you once pressed Block for Spider-Man 2, allow it in Windows Security > Firewall > Allow an app.", Theme.Good);
            }
            catch (Exception ex) { sessionLog.Line("Multiplayer: the firewall rule was not added (" + ex.Message + ").", Theme.Warn); }
        }

        void DisposeMultiplayer()
        {
            mpTick.Stop();
            if (mpListener != null) mpListener.Dispose();
            mpListener = null;
        }
    }
}
