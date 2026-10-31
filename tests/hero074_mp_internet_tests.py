# SM2 VR (port of SpidyVR to Marvel's Spider-Man 2). GPLv3, as SpidyVR. Made by Agent Mulder.
"""0.0.74 MP INTERNET (owner pt1393u: "it needs to show active servers on a server list from the start, and a pop up
... 'No' or 'Yes, Autosetup' ... as easy as booting up and playing"): the one-VPS server (list + relay in one Node
process, server/vps), the launcher's internet list (InternetLobby.cs) against it, the relay carrying two probe
games whose direct path is dead, the Autosetup pop-up's logic, and the wiring (the window opens on the list)."""
import hashlib
import hmac
import json
import os
import pathlib
import shutil
import socket
import subprocess
import tempfile
import time
import unittest
import urllib.error
import urllib.parse
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / 'tools' / 'launcher' / 'src'
NODE = shutil.which('node')


def find_probe():
    exe = 'spidy_mp_probe.exe' if os.name == 'nt' else 'spidy_mp_probe'
    if os.environ.get('SPIDY_MP_PROBE') and pathlib.Path(os.environ['SPIDY_MP_PROBE']).is_file():
        return pathlib.Path(os.environ['SPIDY_MP_PROBE'])
    for d in ('build/mp', 'build/windows-ninja', 'build/core-ninja', 'build'):
        p = ROOT / d / exe
        if p.is_file():
            return p
    return None


PROBE = find_probe()
SECRET = 'test-secret-0123456789'


def token(secret=SECRET, r4=b'\x01\x02\x03\x04'):
    return (r4 + hmac.new(secret.encode(), r4, hashlib.sha256).digest()[:4]).hex()


def json_lines(text):
    return [json.loads(l) for l in text.splitlines() if l.startswith('{')]


class Server:
    """server/vps/herovr-server.mjs as on the VPS (--local: 127.0.0.1 addresses allowed, no Caddy in front)."""

    def __init__(self, http_port, relay_port):
        env = dict(os.environ, RELAY_SECRET=SECRET, RELAY_ADDR='127.0.0.1:%d' % relay_port,
                   HTTP_PORT=str(http_port), RELAY_PORT=str(relay_port))
        self.url = 'http://127.0.0.1:%d' % http_port
        self.p = subprocess.Popen([NODE, str(ROOT / 'server/vps/herovr-server.mjs'), '--local'], env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        for _ in range(100):
            try:
                if self.get('/v1/health') == 'ok':
                    return
            except OSError:
                time.sleep(.05)
        raise RuntimeError('server did not start')

    def get(self, path):
        return urllib.request.urlopen(self.url + path, timeout=5).read().decode()

    def post(self, path, **form):
        data = urllib.parse.urlencode(form).encode()
        try:
            return urllib.request.urlopen(self.url + path, data=data, timeout=5).read().decode()
        except urllib.error.HTTPError as e:
            return e.read().decode()

    def close(self):
        self.p.terminate()
        self.p.wait(timeout=10)


@unittest.skipUnless(NODE, 'node')
class OneServer(unittest.TestCase):
    def setUp(self):
        self.s = Server(39871, 39872)

    def tearDown(self):
        self.s.close()

    def test_host_list_join_and_no_addresses_in_the_list(self):
        s = self.s
        self.assertEqual(s.get('/v1/list'), 'HVL1\n')
        r = s.post('/v1/host', name='mary', hero='3', heroName='MILES MORALES', players='1', max='2', protocol='1',
                   addr='127.0.0.1:40001', lan='abc123')
        ok, hid, key = r.splitlines()[0].split('\t')
        self.assertEqual(ok, 'ok')
        relay = r.splitlines()[1].split('\t')
        self.assertEqual(relay[:2], ['relay', '127.0.0.1:39872'])
        tok = bytes.fromhex(relay[2])
        self.assertEqual(tok[4:], hmac.new(SECRET.encode(), tok[:4], hashlib.sha256).digest()[:4])
        lst = s.get('/v1/list')
        self.assertEqual(lst, 'HVL1\n%s\tMARY\t3\tMILES MORALES\t1\t2\t1\tabc123\n' % hid)
        self.assertNotIn('127.0.0.1', lst)
        j = s.post('/v1/join', id=hid, protocol='1')
        self.assertTrue(j.startswith('ok\t127.0.0.1:40001\nrelay\t127.0.0.1:39872\t' + relay[2]), j)
        self.assertEqual(s.post('/v1/join', id=hid, protocol='2'), 'err\tversion')
        s.post('/v1/join', id=hid, protocol='1', addr='127.0.0.1:40002', name='peter')
        r = s.post('/v1/host', id=hid, key=key, name='mary', players='1', protocol='1', addr='127.0.0.1:40001')
        self.assertIn('join\t127.0.0.1:40002\tPETER', r)
        self.assertEqual(s.post('/v1/host', id=hid, key='wrong', addr='127.0.0.1:40001'), 'err\tkey')
        s.post('/v1/leave', id=hid, key=key)
        self.assertEqual(s.get('/v1/list'), 'HVL1\n')
        self.assertEqual(s.post('/v1/join', id=hid), 'err\tgone')
        stats = json.loads(s.get('/v1/stats'))
        self.assertEqual(stats['games'], 0)

    def test_relay_carries_only_tokens_the_list_gave_out(self):
        a, b = socket.socket(socket.AF_INET, socket.SOCK_DGRAM), socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        for x in (a, b):
            x.bind(('127.0.0.1', 0))
            x.settimeout(1)
        good, bad = bytes.fromhex(token()), bytes.fromhex(token(secret='someone else'))
        relay = ('127.0.0.1', 39872)
        for x in (a, b):
            x.sendto(b'HVRY' + good, relay)              # keepalives: the relay learns both ends
        time.sleep(.1)
        a.sendto(b'HVRY' + good + b'hello', relay)
        self.assertEqual(b.recvfrom(2000)[0], b'HVRY' + good + b'hello')
        for x in (a, b):
            x.sendto(b'HVRY' + bad, relay)
        time.sleep(.1)
        a.sendto(b'HVRY' + bad + b'spam', relay)
        with self.assertRaises(socket.timeout):
            b.recvfrom(2000)
        st = json.loads(self.s.get('/v1/stats'))['relay']
        self.assertEqual(st['forwarded'], 1)
        self.assertGreaterEqual(st['dropped'], 3)
        a.close(); b.close()

    @unittest.skipUnless(PROBE, 'spidy_mp_probe not built')
    def test_two_games_meet_through_the_relay_when_the_direct_way_is_dead(self):
        tok = token(r4=b'\x09\x08\x07\x06')
        host = subprocess.Popen([str(PROBE), 'run', '39880', '9', 'host:39889', 'mary',
                                 'relay=127.0.0.1:39872', 'token=' + tok, 'lan=1'], stdout=subprocess.PIPE, text=True)
        time.sleep(.5)
        pal = subprocess.run([str(PROBE), 'run', '39890', '7', '127.0.0.1:39899', 'peter',
                              'relay=127.0.0.1:39872', 'token=' + tok, 'relayafter=1', 'lan=1'],
                             capture_output=True, text=True, timeout=60)
        hout, _ = host.communicate(timeout=60)
        prow = json_lines(pal.stdout)
        self.assertTrue(any(r['buddy'] and r['relayed'] and r['name'] == 'MARY' for r in prow), pal.stdout)
        self.assertTrue(any(r['buddy'] and r['name'] == 'PETER' for r in json_lines(hout)), hout)
        self.assertGreater(json.loads(self.s.get('/v1/stats'))['relay']['forwarded'], 20)


HARNESS = r'''
using System; using System.Collections.Generic; using System.IO; using System.Linq; using System.Net; using System.Threading;
namespace Sm2VrLauncher { static class H {
  static void Pub(string dir, string addr, int players) {
    File.WriteAllText(Path.Combine(dir, "MP-PUBLIC.txt"), "public=" + addr + "\nport=39470\nplayers=" + players + "\nhero=3\n"); }
  static int Main(string[] a) {
    if (a[0] == "flow") {
      string url = a[1], hd = a[2], jd = a[3];
      var host = new InternetSession(url, hd) { HostEvery = 0, ListEvery = 0, JoinEvery = 0 };
      var pal = new InternetSession(url, jd) { HostEvery = 0, ListEvery = 0, JoinEvery = 0 };
      Lobby.Write(hd, Lobby.HostText("mary"));
      Pub(hd, "127.0.0.1:40001", 1);
      host.SetLocal("MILES MORALES", 0xabc123);
      host.Step(DateTime.UtcNow);
      var hf = Lobby.Read(hd);
      Console.WriteLine("hostfile " + hf["mode"] + " " + hf["relay"] + " " + hf["token"].Length);
      Console.WriteLine("msgs " + string.Join(" / ", host.TakeMessages().ToArray()));
      pal.Step(DateTime.UtcNow);
      string problem; DateTime at;
      var games = pal.Games(out problem, out at);
      Console.WriteLine("list " + games.Count + " " + string.Join(",", games.Select(g => g.Name + "/" + g.HeroName + "/" + g.LanId.ToString("x")).ToArray()) + " problem=" + problem);
      var rows = InternetLobby.Merge(new List<LobbyHost>(), games, new uint[0]);
      Console.WriteLine("row " + InternetLobby.RowText(rows[0]) + " canjoin=" + rows[0].CanJoin);
      pal.Join(games[0], "peter");
      pal.Step(DateTime.UtcNow);
      var jf = Lobby.Read(jd);
      Console.WriteLine("joinfile " + jf["mode"] + " " + jf["join"] + " " + jf["host"] + " " + (jf["inet"] == games[0].Id) + " " + jf["relay"] + " " + (jf["token"] == hf["token"]));
      Console.WriteLine("msgs " + string.Join(" / ", pal.TakeMessages().ToArray()));
      Pub(jd, "127.0.0.1:40002", 1);
      pal.Step(DateTime.UtcNow);
      host.Step(DateTime.UtcNow);
      Console.WriteLine("punch " + Lobby.Read(hd)["punch"]);
      Lobby.Leave(hd);
      host.Step(DateTime.UtcNow);
      pal.Refresh(); pal.Step(DateTime.UtcNow);
      Console.WriteLine("after leave " + pal.Games(out problem, out at).Count);
      var off = new InternetSession("http://127.0.0.1:1", jd);
      off.Step(DateTime.UtcNow);
      off.Games(out problem, out at);
      Console.WriteLine("offline " + (problem.StartsWith("cannot reach") ? "says so" : problem));
      return 0; }
    if (a[0] == "parse") {
      Console.WriteLine(InternetLobby.ParseList("nope") == null ? "notalist" : "?");
      var l = InternetLobby.ParseList("HVL1\nid1\tmary\t3\tmiles\t2\t2\t1\tff\nbad\nid2\tx\t1\t\t1\t2\t9\n");
      Console.WriteLine(l.Count + " " + l[0].Full + " " + l[0].LanId + " " + l[1].SameVersion);
      var h = InternetLobby.ParseHost("ok\tI\tK\nrelay\t1.2.3.4:3479\t0011223344556677\njoin\t5.6.7.8:9\tP\njoin\tnot an addr\tQ\n");
      Console.WriteLine(h.Ok + " " + h.Relay + " " + h.Joins.Count);
      Console.WriteLine(InternetLobby.ParseJoin("err\tfull").Error + " " + InternetLobby.ValidAddr("1.2.3.4:5") + InternetLobby.ValidAddr("::1:5") + InternetLobby.ValidAddr("1.2.3:5"));
      var lan = new LobbyHost { Address = IPAddress.Parse("192.168.1.5"), Beacon = new LobbyBeacon { Name = "MARY", HostId = 0xff, Players = 1, MaxPlayers = 2, Protocol = LobbyBeacon.MpProtocol } };
      var m = InternetLobby.Merge(new[] { lan }, l, new uint[0]);
      Console.WriteLine("merged " + string.Join(",", m.Select(e => e.Where + ":" + e.Name).ToArray()));
      Console.WriteLine(Environment.GetEnvironmentVariable("SM2VR_LOBBY_URL") ?? "");
      return 0; }
    if (a[0] == "fail") {   // 0.0.74 MP FREE LOBBY: a JOIN that got no answer
      File.WriteAllText(Path.Combine(a[1], "MP-PUBLIC.txt"), "public=1.2.3.4:39470\nport=39470\nplayers=1\nfailed=1\n");
      var p = InternetLobby.ReadPublic(a[1], DateTime.UtcNow);
      Console.WriteLine(p.Fresh + " " + p.Failed);
      File.WriteAllText(Path.Combine(a[1], "MP-PUBLIC.txt"), "public=1.2.3.4:39470\nport=39470\nplayers=2\nfriend=MARY\nfailed=0\n");
      p = InternetLobby.ReadPublic(a[1], DateTime.UtcNow);
      Console.WriteLine(p.Failed + " " + p.Friend);
      Console.WriteLine(InternetLobby.JoinFailText("MARY", false));
      Console.WriteLine(InternetLobby.JoinFailText("MARY", true));
      var t1 = Lobby.NewTry(); Thread.Sleep(20); var t2 = Lobby.NewTry();
      Console.WriteLine((t1 != t2) + " " + (t1.Length <= 16) + " " + t1.All(char.IsLetterOrDigit));
      var lan = new LobbyHost { Address = IPAddress.Parse("192.168.1.5"), Beacon = new LobbyBeacon { Port = 39470, Name = "MARY", HostId = 0xff, Players = 1, MaxPlayers = 2, Protocol = LobbyBeacon.MpProtocol } };
      Console.WriteLine(Lobby.JoinText("peter", lan).Contains("\r\ntry="));
      return 0; }
    if (a[0] == "setup") {
      var fw = Autosetup.FirewallItem();
      fw.IsDone = () => false;
      Console.WriteLine(Autosetup.Needed(new[] { fw }).Count + " " + Autosetup.ElevatedArgs(new[] { fw }, null));
      Console.WriteLine(Autosetup.Text(new[] { fw }).Replace("\n", "|"));
      string path = Path.Combine(a[1], "payload.bin");
      File.WriteAllText(path, "hello");
      var dl = new SetupItem { Title = "Thing", Kind = SetupKind.Download, Url = new Uri(path).AbsoluteUri, FileName = "got.exe", InstallArgs = "/quiet",
        Sha256 = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824" };
      string problem, got = Autosetup.DownloadChecked(dl, Path.Combine(a[1], "dl"), out problem);
      Console.WriteLine("good " + (got != null) + " " + problem);
      Console.WriteLine(Autosetup.ElevatedArgs(new[] { dl, fw }, new Dictionary<SetupItem, string> { { dl, got } }).Replace(a[1], "TMP"));
      dl.Sha256 = new string('0', 64);
      got = Autosetup.DownloadChecked(dl, Path.Combine(a[1], "dl"), out problem);
      Console.WriteLine("bad " + (got == null) + " " + File.Exists(Path.Combine(a[1], "dl", "got.exe")) + " " + problem.Contains("not the expected file"));
      return 0; }
    return 2; }
} }'''


@unittest.skipUnless(shutil.which('mcs') and shutil.which('mono'), 'mono')
class LauncherInternet(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.mkdtemp()
        (pathlib.Path(cls.dir) / 'h.cs').write_text(HARNESS)
        r = subprocess.run(['mcs', '-nologo', '-langversion:5', '-r:System.dll', '-r:System.Core.dll', '-out:' + cls.dir + '/h.exe',
                            str(SRC / 'Lobby.cs'), str(SRC / 'Settings.cs'), str(SRC / 'ModVersion.cs'), str(SRC / 'InternetLobby.cs'), str(SRC / 'Autosetup.cs'), cls.dir + '/h.cs'],
                           capture_output=True, text=True)
        assert r.returncode == 0, r.stdout + r.stderr
        cls.exe = cls.dir + '/h.exe'

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.dir, ignore_errors=True)

    def run_h(self, *args):
        r = subprocess.run(['mono', self.exe] + list(args), capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r.stdout.strip().splitlines()

    @unittest.skipUnless(NODE, 'node')
    def test_host_and_join_through_the_server_list(self):
        s = Server(39873, 39874)
        try:
            with tempfile.TemporaryDirectory() as hd, tempfile.TemporaryDirectory() as jd:
                out = self.run_h('flow', s.url, hd, jd)
        finally:
            s.close()
        self.assertEqual(out[0], 'hostfile host 127.0.0.1:39874 16')
        self.assertIn('your game is on the server list as MARY', out[1])
        self.assertEqual(out[2], 'list 1 MARY/MILES MORALES/abc123 problem=')
        self.assertTrue(out[3].startswith('row MARY   MILES MORALES   1/2   Internet') and out[3].endswith('canjoin=True'), out[3])
        self.assertEqual(out[4], 'joinfile join 127.0.0.1:40001 MARY True 127.0.0.1:39874 True')
        self.assertIn('joining MARY', out[5])
        self.assertEqual(out[6], 'punch 127.0.0.1:40002')
        self.assertEqual(out[7], 'after leave 0')
        self.assertEqual(out[8], 'offline says so')

    def test_parsers_and_merge(self):
        out = self.run_h('parse')
        self.assertEqual(out[0], 'notalist')
        self.assertEqual(out[1], '2 True 255 False')
        self.assertEqual(out[2], 'True 1.2.3.4:3479 1')
        self.assertEqual(out[3], 'full TrueFalseFalse')
        self.assertEqual(out[4], 'merged LAN:MARY,Internet:X')   # the internet MARY is the same game (LAN id ff): shown once, as LAN

    def test_a_join_that_got_no_answer_says_so_in_one_line(self):
        with tempfile.TemporaryDirectory() as d:
            out = self.run_h('fail', d)
        self.assertEqual(out[0], 'True True')
        self.assertEqual(out[1], 'False MARY')
        self.assertEqual(out[2], 'Could not connect to MARY: the two home networks did not let each other in. '
                                 'Press JOIN to try again, or HOST and let MARY join you.')
        self.assertIn('(not even through the relay)', out[3])
        self.assertEqual(out[4], 'True True True')   # every JOIN press is a new try= (the game restarts its call)
        self.assertEqual(out[5], 'True')

    def test_autosetup_one_prompt_and_checked_downloads(self):
        with tempfile.TemporaryDirectory() as d:
            out = self.run_h('setup', d)
        self.assertEqual(out[0], '1 /c "netsh advfirewall firewall delete rule name="HERO VR multiplayer" >nul 2>&1 & '
                                 'netsh advfirewall firewall add rule name="HERO VR multiplayer" dir=in action=allow protocol=UDP '
                                 'localport=39469-39474 profile=private,public"')
        self.assertIn('Let multiplayer through the Windows Firewall', out[1])
        self.assertIn('Yes, Autosetup', out[1])
        self.assertIn('No: skip it', out[1])
        self.assertEqual(out[2], 'good True ')
        self.assertTrue(out[3].startswith('/c ""TMP/dl/got.exe" /quiet & netsh advfirewall firewall delete rule'), out[3])
        self.assertEqual(out[4], 'bad True False True')


class Wiring(unittest.TestCase):
    def test_list_from_the_start_and_the_popup(self):
        main = (SRC / 'MainForm.cs').read_text(encoding='utf-8')
        mp = (SRC / 'MainForm.Multiplayer.cs').read_text(encoding='utf-8')
        dialog = (SRC / 'AutosetupDialog.cs').read_text(encoding='utf-8')
        self.assertIn('tabs.Selected = SheetMultiplayer;', main)     # the classic window opens on the server list
        self.assertIn('MultiplayerStart();', main)                    # listening from the start, not when shown
        self.assertIn('MpAutosetup(false)', main)                     # the pop-up at start
        self.assertIn('new InternetSession(InternetLobby.Url(settings)', mp)
        self.assertIn('InternetLobby.Merge(', mp)
        self.assertIn('"Yes, Autosetup"', dialog)
        self.assertIn('Text = "No"', dialog)
        lobby = (SRC / 'InternetLobby.cs').read_text(encoding='utf-8')
        self.assertIn('public const string DefaultUrl = ', lobby)   # the endpoint is one constant (docs/LOBBY-OWNER-SETUP.md)
        import re
        self.assertTrue(re.search(r'public const string DefaultUrl = "[^"]*";', lobby), 'deploy_cloudflare.py sets it by this pattern')
        # 0.0.74 MP FREE LOBBY: the free Cloudflare Worker is the default list (docs/LOBBY-OWNER-SETUP.md)
        self.assertTrue(re.search(r'public const string DefaultUrl = "https://herovr-lobby\.[a-z0-9-]+\.workers\.dev";', lobby))
        self.assertIn('InternetLobby.JoinFailText(', mp)
        g = (ROOT / 'src' / 'game_xr.cpp').read_text(encoding='utf-8', errors='replace')
        self.assertIn('cfg.giveUpAfter = 30;', g)
        self.assertIn('"\\nfailed="', g)

    def test_worker_is_free_plan_only(self):
        toml = (ROOT / 'server/lobby/wrangler.toml').read_text()
        self.assertIn('new_sqlite_classes = ["LobbyRoom"]', toml)   # SQLite-backed Durable Object: the one kind on Workers Free
        self.assertIn('workers_dev = true', toml)
        self.assertIn('RELAY_ADDR = ""', toml)                       # no relay until the owner adds one
        self.assertNotIn('new_classes', toml)                        # KV-backed Durable Objects need Workers Paid
        dep = (ROOT / 'server/lobby/deploy_cloudflare.py').read_text()
        self.assertIn("os.environ.get('CLOUDFLARE_API_TOKEN'", dep)   # the token only from the environment

    @unittest.skipUnless(PROBE, 'spidy_mp_probe not built')
    def test_game_gives_up_on_a_dead_join_in_one_line(self):
        r = subprocess.run([str(PROBE), 'run', '39895', '4', '127.0.0.1:39897', 'peter', 'giveup=1'],
                           capture_output=True, text=True, timeout=60)
        rows = json_lines(r.stdout)
        self.assertTrue(rows and rows[-1]['gaveUp'] == 1 and rows[-1]['status'] == 'no connection', r.stdout)
        self.assertEqual((r.stdout + r.stderr).count('could not connect to 127.0.0.1:39897'), 1, r.stdout + r.stderr)

    def test_cloud_init_is_current(self):
        gen = ROOT / 'server/vps/cloud-init.sh'
        before = gen.read_text()
        subprocess.run(['python3', str(ROOT / 'server/vps/make-cloud-init.py')], check=True, capture_output=True)
        self.assertEqual(gen.read_text(), before, 'run server/vps/make-cloud-init.py after changing a server file')
        for line in ('ufw allow 3479/udp', 'reverse_proxy 127.0.0.1:8080', 'openssl rand -hex 24', 'sslip.io'):
            self.assertIn(line, before)

    def test_game_writes_its_public_address_for_the_launcher(self):
        g = (ROOT / 'src' / 'game_xr.cpp').read_text(encoding='utf-8', errors='replace')
        self.assertIn('MP-PUBLIC.txt', g)
        self.assertIn('stun.cloudflare.com:3478', g)
        self.assertIn('mpSession->setTargets(', g)


if __name__ == '__main__':
    unittest.main()
