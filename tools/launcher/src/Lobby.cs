// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3; SpidyVR by Ilya Mezerowsky.
// Made by Agent Mulder. 0.0.74 MP LOBBY: the launcher's side of the LAN host list (no IP typing).
// A HOSTING game sends a 52-byte beacon to UDP 39469 once a second (include/spidy/mp_lobby.hpp, the same bytes);
// this listens there, keeps the hosts heard in the last 4 s, and writes %APPDATA%\Spidy\MP-LOBBY.txt for HOST / JOIN
// (the game reads it every 2 s; LEAVE deletes it). MP-PEER.txt (typing the friend's IP) stays as the fallback.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace Sm2VrLauncher
{
    class LobbyBeacon
    {
        public const int Bytes = 52, LobbyPort = 39469, GamePort = 39470;
        public const byte LobbyVersion = 1, MpProtocol = 1, NoHero = 255;
        public byte Protocol, Players, MaxPlayers, Hero;
        public ushort Port;
        public uint HostId;
        public string Name = "", HeroName = "";

        public static uint Crc32(byte[] d, int count)
        {
            uint c = 0xffffffffu;
            for (int i = 0; i < count; i++)
            {
                c ^= d[i];
                for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? (c >> 1) ^ 0xedb88320u : c >> 1;
            }
            return ~c;
        }

        static uint U32(byte[] d, int at) { return (uint)(d[at] | (d[at + 1] << 8) | (d[at + 2] << 16)) | ((uint)d[at + 3] << 24); }

        static bool Text(byte[] d, int at, out string s)
        {
            var sb = new StringBuilder();
            s = "";
            for (int i = 0; i < 16 && d[at + i] != 0; i++)
            {
                if (d[at + i] < 32 || d[at + i] > 126) return false;
                sb.Append((char)d[at + i]);
            }
            s = sb.ToString();
            return true;
        }

        /// <summary>null: not a beacon of this lobby version (size, magic 'HVLB', version, checksum or values).</summary>
        public static LobbyBeacon Parse(byte[] d, int size)
        {
            if (d == null || size != Bytes || d.Length < Bytes) return null;
            if (d[0] != 'H' || d[1] != 'V' || d[2] != 'L' || d[3] != 'B' || d[4] != LobbyVersion) return null;
            if (U32(d, 48) != Crc32(d, 48)) return null;
            var b = new LobbyBeacon { Protocol = d[5], Port = (ushort)(d[6] | (d[7] << 8)), Players = d[8], MaxPlayers = d[9], Hero = d[10], HostId = U32(d, 12) };
            if (b.Port == 0 || b.MaxPlayers == 0 || b.Players > 16 || b.MaxPlayers > 16) return null;
            string n, h;
            if (!Text(d, 16, out n) || !Text(d, 32, out h)) return null;
            b.Name = n; b.HeroName = h;
            return b;
        }

        public bool Full { get { return Players >= MaxPlayers; } }
        public bool SameVersion { get { return Protocol == MpProtocol; } }
    }

    class LobbyHost
    {
        public IPAddress Address;
        public LobbyBeacon Beacon;
        public DateTime LastSeen;
        public bool ThisPc;
        public string Endpoint { get { return Address + ":" + Beacon.Port; } }
    }

    /// <summary>The hosts heard on UDP 39469 (a background thread; Snapshot() from the UI).</summary>
    class LobbyListener : IDisposable
    {
        public double ForgetSeconds = 4.0;
        readonly object gate = new object();
        readonly List<LobbyHost> hosts = new List<LobbyHost>();
        UdpClient udp;
        Thread thread;
        volatile bool stop;
        public string Error = "";
        public readonly int Port;

        public LobbyListener(int port) { Port = port; }

        public bool Start()
        {
            try
            {
                udp = new UdpClient();
                // The game (hosting) and other launchers may listen on the same port on this PC.
                udp.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                udp.Client.Bind(new IPEndPoint(IPAddress.Any, Port));
                udp.Client.ReceiveTimeout = 250;
            }
            catch (Exception e) { Error = e.Message; if (udp != null) udp.Close(); udp = null; return false; }
            thread = new Thread(Loop) { IsBackground = true, Name = "HERO VR lobby" };
            thread.Start();
            return true;
        }

        void Loop()
        {
            var mine = Lobby.LocalAddresses();
            while (!stop)
            {
                try
                {
                    var from = new IPEndPoint(IPAddress.Any, 0);
                    byte[] d = udp.Receive(ref from);
                    Heard(from.Address, LobbyBeacon.Parse(d, d.Length), DateTime.UtcNow, mine);
                }
                catch (SocketException) { }
                catch (ObjectDisposedException) { return; }
                catch (Exception) { }
            }
        }

        public void Heard(IPAddress from, LobbyBeacon b, DateTime now, IList<IPAddress> mine)
        {
            if (b == null || from == null) return;
            lock (gate)
            {
                var h = hosts.FirstOrDefault(x => x.Beacon.HostId == b.HostId && x.Beacon.Port == b.Port);
                if (h == null)
                {
                    if (hosts.Count >= 32) return;
                    h = new LobbyHost { Address = from };
                    hosts.Add(h);
                }
                else if ((now - h.LastSeen).TotalSeconds > ForgetSeconds) h.Address = from;
                h.Beacon = b;
                h.LastSeen = now;
                h.ThisPc = IPAddress.IsLoopback(h.Address) || (mine != null && mine.Any(m => m.Equals(h.Address)));
            }
        }

        public List<LobbyHost> Snapshot(DateTime now)
        {
            lock (gate)
                return hosts.Where(h => (now - h.LastSeen).TotalSeconds <= ForgetSeconds)
                    .OrderBy(h => h.Beacon.Name, StringComparer.Ordinal).ThenBy(h => h.Address.ToString(), StringComparer.Ordinal).ToList();
        }

        public void Dispose()
        {
            stop = true;
            try { if (udp != null) udp.Close(); } catch { }
            udp = null;
        }
    }

    static class Lobby
    {
        public static string Folder()
        {
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Spidy");
        }
        public static string FilePath(string folder) { return Path.Combine(folder ?? Folder(), "MP-LOBBY.txt"); }

        /// <summary>Upper case, printable ASCII, 16 at most (the game's mp::cleanName); empty: "HERO VR".</summary>
        public static string CleanName(string s)
        {
            var sb = new StringBuilder();
            foreach (char c in (s ?? "").Trim())
            {
                if (sb.Length >= 16) break;
                char u = char.ToUpperInvariant(c);
                sb.Append(u >= 32 && u < 127 && u != '#' && u != '=' ? u : ' ');
            }
            string r = sb.ToString().Trim();
            return r.Length == 0 ? "HERO VR" : r;
        }

        public static string HostText(string name)
        {
            return "# HERO VR multiplayer (written by the launcher's Multiplayer sheet; LEAVE deletes it)\r\nmode=host\r\nname=" + CleanName(name) + "\r\n";
        }

        public static string JoinText(string name, LobbyHost h)
        {
            return "# HERO VR multiplayer (written by the launcher's Multiplayer sheet; LEAVE deletes it)\r\nmode=join\r\njoin=" + h.Endpoint +
                "\r\nname=" + CleanName(name) + "\r\nhost=" + CleanName(h.Beacon.Name) + "\r\n";
        }

        /// <summary>Writes MP-LOBBY.txt (UTF-8, no mark; replaced in one step so the game never reads half).</summary>
        public static void Write(string folder, string text)
        {
            folder = folder ?? Folder();
            Directory.CreateDirectory(folder);
            string path = FilePath(folder), tmp = path + ".tmp";
            File.WriteAllText(tmp, text, new UTF8Encoding(false));
            if (File.Exists(path)) File.Delete(path);
            File.Move(tmp, path);
        }

        public static void Leave(string folder)
        {
            string path = FilePath(folder);
            if (File.Exists(path)) File.Delete(path);
        }

        /// <summary>"host" / "join" / "" (none) and the rest of MP-LOBBY.txt, for the sheet's state line.</summary>
        public static Dictionary<string, string> Read(string folder)
        {
            var d = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            try
            {
                string path = FilePath(folder);
                if (!File.Exists(path)) return d;
                foreach (var raw in File.ReadAllLines(path))
                {
                    string line = raw;
                    int hash = line.IndexOf('#');
                    if (hash >= 0) line = line.Substring(0, hash);
                    int eq = line.IndexOf('=');
                    if (eq <= 0) continue;
                    d[line.Substring(0, eq).Trim().TrimStart(new[] { '\uFEFF' })] = line.Substring(eq + 1).Trim();
                }
            }
            catch { }
            return d;
        }

        public static List<IPAddress> LocalAddresses()
        {
            var r = new List<IPAddress>();
            try
            {
                foreach (var a in Dns.GetHostAddresses(Dns.GetHostName()))
                    if (a.AddressFamily == AddressFamily.InterNetwork && !IPAddress.IsLoopback(a) && !r.Contains(a)) r.Add(a);
            }
            catch { }
            return r;
        }

        public const string FirewallNote = "The first time, Windows asks to let Spider-Man 2 and the launcher through the firewall: tick Private networks and press Allow (look at the desktop if you are in the headset).";
        public const string FirewallRule = "HERO VR multiplayer";

        /// <summary>netsh arguments for the elevated "Allow through firewall" button (the lobby port and the game's five).</summary>
        public static string FirewallArgs()
        {
            return "advfirewall firewall add rule name=\"" + FirewallRule + "\" dir=in action=allow protocol=UDP localport=" +
                LobbyBeacon.LobbyPort + "-" + (LobbyBeacon.GamePort + 4) + " profile=private";
        }
    }
}
