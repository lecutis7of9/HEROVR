# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR. Made by Agent Mulder.
"""0.0.74 MP LOBBY (owner pt1384u: "a 'start host' ... and then someone can just join the host on a server list"):
a hosting game's beacon on the LAN, the launcher's list (Lobby.cs) reading the same 52 bytes, JOIN writing an
MP-LOBBY.txt the game reads, and MP-PEER.txt still there as the fallback."""
import json
import os
import pathlib
import shutil
import struct
import subprocess
import tempfile
import time
import unittest
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools' / 'launcher' / 'src'


def find_probe():
    exe = 'spidy_mp_probe.exe' if os.name == 'nt' else 'spidy_mp_probe'
    if os.environ.get('SPIDY_MP_PROBE') and pathlib.Path(os.environ['SPIDY_MP_PROBE']).is_file():
        return pathlib.Path(os.environ['SPIDY_MP_PROBE'])
    for d in ('build/mp', 'build/windows-ninja', 'build/core-ninja', 'build', 'build/gate', 'build/Release'):
        p = ROOT / d / exe
        if p.is_file():
            return p
    return None


PROBE = find_probe()


def beacon(name=b'MARY', hero=0, hero_name=b'SPIDER-MAN', port=39470, players=1, maxp=2, host_id=0x1234, proto=1):
    """The beacon from its description in include/spidy/mp_lobby.hpp (not from the C++), little-endian."""
    body = b'HVLB' + struct.pack('<BBHBBBBI', 1, proto, port, players, maxp, hero, 0, host_id)
    body += name.ljust(16, b'\0') + hero_name.ljust(16, b'\0')
    return body + struct.pack('<I', zlib.crc32(body) & 0xffffffff)


def json_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.startswith('{')]


@unittest.skipUnless(PROBE, 'spidy_mp_probe not built')
class ProbeLobby(unittest.TestCase):
    def test_host_is_listed_then_full_after_a_join(self):
        lp, hp, jp = 39779, 39780, 39790
        ear = subprocess.Popen([str(PROBE), 'lobby', '6', str(lp)], stdout=subprocess.PIPE, text=True)
        time.sleep(.3)
        host = subprocess.Popen([str(PROBE), 'run', str(hp), '5.5', 'host:%d' % lp, 'mary'], stdout=subprocess.PIPE, text=True)
        time.sleep(1.5)
        pal = subprocess.run([str(PROBE), 'run', str(jp), '3', '127.0.0.1:%d' % hp, 'peter'], capture_output=True, text=True, timeout=30)
        hout, _ = host.communicate(timeout=30)
        eout, _ = ear.communicate(timeout=30)
        lists = [l['hosts'] for l in json_lines(eout)]
        seen = [h for hs in lists for h in hs]
        self.assertTrue(seen, eout)
        first = seen[0]
        self.assertEqual((first['name'], first['heroName'], first['max'], first['protocol']), ('MARY', 'SPIDER-MAN', 2, 1))
        self.assertTrue(first['addr'].endswith(':%d' % hp), first)
        self.assertTrue(any(h['players'] == 1 for h in seen), 'listed as 1 of 2 before the join')
        self.assertTrue(any(h['players'] == 2 for h in seen), 'FULL (2 of 2) once the friend joined:\n' + eout)
        self.assertTrue(all(len(hs) <= 1 for hs in lists), 'one host listed once')
        hrows = json_lines(hout)
        self.assertTrue(all(r['hosting'] == 1 for r in hrows) and hrows[-1]['beacons'] >= 4, hout)
        self.assertTrue(any(r['name'] == 'PETER' for r in hrows), hout)
        self.assertTrue(any(r['name'] == 'MARY' for r in json_lines(pal.stdout)), pal.stdout)

    def test_lobby_file_as_the_game_reads_it(self):
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / 'MP-LOBBY.txt'
            f.write_bytes(b'\xef\xbb\xbf# from Notepad\r\nmode=join\r\njoin=192.168.1.20\r\nname=Peter\r\nhost=MARY\r\n')
            r = json.loads(subprocess.run([str(PROBE), 'lobbyfile', str(f)], capture_output=True, text=True).stdout)
            self.assertEqual({k: r[k] for k in ('mode', 'join', 'port', 'name', 'host')},
                             {'mode': 'join', 'join': '192.168.1.20', 'port': 39470, 'name': 'PETER', 'host': 'MARY'})
            self.assertEqual((r['punch'], r['relay']), ([], ''))  # LAN join: no internet extras
            f.write_text('mode=host\nname=mary\n')
            r = json.loads(subprocess.run([str(PROBE), 'lobbyfile', str(f)], capture_output=True, text=True).stdout)
            self.assertEqual((r['mode'], r['name']), ('host', 'MARY'))
            f.write_text('join=10.0.0.1\n')  # no mode: nothing (MP-PEER.txt is the file for a bare address)
            r = json.loads(subprocess.run([str(PROBE), 'lobbyfile', str(f)], capture_output=True, text=True).stdout)
            self.assertEqual(r['mode'], 'none')


HARNESS = r'''
using System; using System.IO; using System.Linq; using System.Net; using System.Net.Sockets; using System.Threading;
namespace Sm2VrLauncher { static class H {
  static byte[] Hex(string s) { var b = new byte[s.Length / 2]; for (int i = 0; i < b.Length; i++) b[i] = Convert.ToByte(s.Substring(2 * i, 2), 16); return b; }
  static int Main(string[] a) {
    if (a[0] == "parse") {
      var b = Hex(a[1]); var p = LobbyBeacon.Parse(b, b.Length);
      Console.WriteLine(p == null ? "null" : p.Name + "|" + p.HeroName + "|" + p.Hero + "|" + p.Port + "|" + p.Players + "/" + p.MaxPlayers + "|" + p.HostId + "|" + p.Full + "|" + p.SameVersion);
      return 0; }
    if (a[0] == "listen") {
      var l = new LobbyListener(int.Parse(a[1]));
      if (!l.Start()) { Console.WriteLine("error " + l.Error); return 1; }
      var second = new LobbyListener(int.Parse(a[1]));   // the game and another launcher share the port
      if (!second.Start()) { Console.WriteLine("error shared " + second.Error); return 1; }
      second.Dispose();
      var until = DateTime.UtcNow.AddSeconds(double.Parse(a[2], System.Globalization.CultureInfo.InvariantCulture));
      while (DateTime.UtcNow < until) {
        var s = l.Snapshot(DateTime.UtcNow);
        if (s.Count > 0) { foreach (var h in s) Console.WriteLine("host " + h.Endpoint + " " + MainFormRow(h) + " thispc=" + h.ThisPc); l.Dispose(); return 0; }
        Thread.Sleep(50); }
      l.Dispose(); Console.WriteLine("none"); return 0; }
    if (a[0] == "write") {
      var h = new LobbyHost { Address = IPAddress.Parse("192.168.1.20"), Beacon = new LobbyBeacon { Name = "MARY", Port = 39471, MaxPlayers = 2, Players = 1 } };
      Lobby.Write(a[1], Lobby.JoinText("peter #1 = me", h));
      Lobby.Write(a[1], Lobby.JoinText("peter", h));   // replaces it
      var d = Lobby.Read(a[1]);
      Console.WriteLine(d["mode"] + "|" + d["join"] + "|" + d["name"] + "|" + d["host"] + "|" + Lobby.CleanName("") + "|" + Lobby.CleanName("a#b=c and a very long name"));
      File.Copy(Lobby.FilePath(a[1]), Path.Combine(a[1], "join.txt"));
      Lobby.Write(a[1], Lobby.HostText("mary"));
      File.Copy(Lobby.FilePath(a[1]), Path.Combine(a[1], "host.txt"));
      Lobby.Leave(a[1]); Lobby.Leave(a[1]);
      Console.WriteLine(File.Exists(Lobby.FilePath(a[1])) ? "still there" : "left");
      Console.WriteLine(Lobby.FirewallArgs());
      return 0; }
    return 2; }
  static string MainFormRow(LobbyHost h) { var b = h.Beacon; return b.Name + "/" + b.HeroName + "/" + b.Players + "of" + b.MaxPlayers; }
} }'''


@unittest.skipUnless(shutil.which('mcs') and shutil.which('mono'), 'mono')
class LauncherLobby(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.mkdtemp()
        (pathlib.Path(cls.dir) / 'h.cs').write_text(HARNESS)
        r = subprocess.run(['mcs', '-nologo', '-langversion:5', '-r:System.dll', '-r:System.Core.dll', '-out:' + cls.dir + '/h.exe',
                            str(SRC / 'Lobby.cs'), cls.dir + '/h.cs'], capture_output=True, text=True)
        assert r.returncode == 0, r.stdout + r.stderr
        cls.exe = cls.dir + '/h.exe'

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.dir, ignore_errors=True)

    def run_h(self, *args):
        r = subprocess.run(['mono', self.exe] + list(args), capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout.strip()

    def test_parses_the_described_beacon(self):
        out = self.run_h('parse', beacon(name=b'MARY JANE', hero=3, hero_name=b'MILES MORALES', port=39471, host_id=0xdeadbeef).hex())
        self.assertEqual(out, 'MARY JANE|MILES MORALES|3|39471|1/2|3735928559|False|True')
        self.assertTrue(self.run_h('parse', beacon(players=2).hex()).endswith('|True|True'))  # FULL
        self.assertTrue(self.run_h('parse', beacon(proto=2).hex()).endswith('|False|False'))  # another mod version
        bad = bytearray(beacon())
        bad[20] ^= 1
        self.assertEqual(self.run_h('parse', bytes(bad).hex()), 'null')
        self.assertEqual(self.run_h('parse', beacon()[:51].hex()), 'null')
        self.assertEqual(self.run_h('parse', (b'HVLB\x02' + beacon()[5:]).hex()), 'null')

    @unittest.skipUnless(PROBE, 'spidy_mp_probe not built')
    def test_hears_a_hosting_game(self):
        lp, hp = 39799, 39800
        host = subprocess.Popen([str(PROBE), 'run', str(hp), '4', 'host:%d' % lp, 'mary'], stdout=subprocess.DEVNULL)
        try:
            out = self.run_h('listen', str(lp), '3.5')
        finally:
            host.wait(timeout=30)
        self.assertIn('host 127.0.0.1:%d MARY/SPIDER-MAN/1of2 thispc=True' % hp, out)

    def test_join_file_is_what_the_game_reads(self):
        with tempfile.TemporaryDirectory() as d:
            out = self.run_h('write', d).splitlines()
            self.assertEqual(out[0], 'join|192.168.1.20:39471|PETER|MARY|HERO VR|A B C AND A VERY')
            self.assertEqual(out[1], 'left')
            self.assertEqual(out[2], 'advfirewall firewall add rule name="HERO VR multiplayer" dir=in action=allow protocol=UDP '
                                     'localport=39469-39474 profile=private')
            self.assertFalse((pathlib.Path(d) / 'MP-LOBBY.txt.tmp').exists())
            if PROBE:
                r = json.loads(subprocess.run([str(PROBE), 'lobbyfile', d + '/join.txt'], capture_output=True, text=True).stdout)
                self.assertEqual({k: r[k] for k in ('mode', 'join', 'port', 'name', 'host')},
                                 {'mode': 'join', 'join': '192.168.1.20', 'port': 39471, 'name': 'PETER', 'host': 'MARY'})
                self.assertEqual((r['punch'], r['relay']), ([], ''))  # LAN join: no internet extras
                r = json.loads(subprocess.run([str(PROBE), 'lobbyfile', d + '/host.txt'], capture_output=True, text=True).stdout)
                self.assertEqual((r['mode'], r['name']), ('host', 'MARY'))


class Wiring(unittest.TestCase):
    def test_launcher_sheet_and_entry_points(self):
        main = (SRC / 'MainForm.cs').read_text(encoding='utf-8')
        hero = (SRC / 'MainForm.Hero.cs').read_text(encoding='utf-8')
        mp = (SRC / 'MainForm.Multiplayer.cs').read_text(encoding='utf-8')
        self.assertIn('MultiplayerBounds(),', main)              # the 6th sheet (classic)
        self.assertIn('MultiplayerBounds() };', hero)            # and in the new design's centred layout
        self.assertIn('S = SheetMultiplayer', hero)              # the new design's link row
        self.assertIn('btnMultiplayer = Add(this', main)         # the classic header (the bar has no room)
        self.assertIn('BuildMultiplayer();', main)
        self.assertIn('DisposeMultiplayer();', main)
        for word in ('"Host"', '"Leave"', '"Join"', '"Full"', 'Allow through firewall', 'Lobby.FirewallNote'):
            self.assertIn(word, mp)
        lobby = (SRC / 'Lobby.cs').read_text(encoding='utf-8')
        note = lobby.split('FirewallNote = "')[1].split('";')[0]
        self.assertEqual(note.count('. '), 0, 'the firewall is explained in one line: ' + note)
        self.assertIn('Private networks', note)
        self.assertIn('profile=private', lobby)

    def test_game_reads_lobby_first_and_keeps_mp_peer(self):
        g = (ROOT / 'src' / 'game_xr.cpp').read_text(encoding='utf-8', errors='replace')
        a, b = g.index('readSmall(L"MP-LOBBY.txt"'), g.index('readSmall(L"MP-PEER.txt"')
        self.assertLess(a, b)
        self.assertIn('mpLobbyOn ? mpLobbyKey : mpPeerText', g)   # MP-LOBBY.txt wins, MP-PEER.txt is the fallback
        self.assertIn('cfg.host = mpLobby.mode == spidy::mp::LobbySetting::host', g)
        self.assertIn('MP-PEER.txt has no address in it', g)


if __name__ == '__main__':
    unittest.main()
