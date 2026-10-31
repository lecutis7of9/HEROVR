#!/usr/bin/env python3
"""HERO VR internet server list - deploy the FREE Cloudflare Worker (no card, no paid add-ons) in one command.

    CLOUDFLARE_API_TOKEN=<token> CLOUDFLARE_ACCOUNT_ID=<id> python3 server/lobby/deploy_cloudflare.py [--smoke] [--no-set-url]
    LOBBY_URL=https://herovr-lobby.<sub>.workers.dev python3 server/lobby/deploy_cloudflare.py --smoke-only

The token: Cloudflare's "Edit Cloudflare Workers" template (docs/LOBBY-OWNER-SETUP.md). The account ID is optional
(found with the token when it can see exactly one account). Steps:
  1. checks the token (never printed, never written to a file);
  2. makes sure the account has a workers.dev subdomain (registers "herovr" or "herovr-<random>" if it has none);
  3. `npx wrangler@4 deploy` in server/lobby (the Worker + its SQLite-backed Durable Object, both on the free plan);
  4. waits for https://herovr-lobby.<subdomain>.workers.dev/v1/health to say ok;
  5. writes that URL into tools/launcher/src/InternetLobby.cs (DefaultUrl) - rebuild the launcher afterwards;
  6. --smoke: lists a fake game from this machine, checks list / join / leave, then one that expires (about 50 s).
Re-running is safe (same Worker, same URL). Removing it: dashboard > Workers & Pages > herovr-lobby > Settings > Delete.
SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3. Made by Agent Mulder.
"""
import json, os, pathlib, re, secrets, subprocess, sys, time, urllib.error, urllib.parse, urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]
LOBBY = ROOT / 'server' / 'lobby'
LAUNCHER = ROOT / 'tools' / 'launcher' / 'src' / 'InternetLobby.cs'
API = 'https://api.cloudflare.com/client/v4'
UA = 'herovr-deploy/1'


def api(method, path, token, body=None):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(API + path, data=data, method=method,
                                 headers={'Authorization': 'Bearer ' + token, 'Content-Type': 'application/json', 'User-Agent': UA})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            return r.status, json.loads(r.read().decode() or '{}')
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.loads(e.read().decode() or '{}')
        except ValueError:
            return e.code, {}


def errors(j):
    return '; '.join(str(e.get('message', e)) for e in (j.get('errors') or [])) or 'no details'


def http(method, url, form=None):
    data = None if form is None else urllib.parse.urlencode(form).encode()
    req = urllib.request.Request(url, data=data, method=method, headers={'User-Agent': UA})
    if data is not None:
        req.add_header('Content-Type', 'application/x-www-form-urlencoded')
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            return r.status, r.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode(errors='replace')
    except Exception as e:  # DNS / TLS not ready yet
        return 0, str(e)


def worker_name():
    m = re.search(r'^name\s*=\s*"([^"]+)"', (LOBBY / 'wrangler.toml').read_text(), re.M)
    return m.group(1)


def set_url(url):
    text = LAUNCHER.read_text(encoding='utf-8')
    new, n = re.subn(r'public const string DefaultUrl = "[^"]*";', 'public const string DefaultUrl = "%s";' % url, text)
    if n != 1:
        sys.exit('could not find DefaultUrl in ' + str(LAUNCHER))
    LAUNCHER.write_text(new, encoding='utf-8')
    print('launcher: DefaultUrl = ' + url + ' (rebuild the launcher)')


class Conn:
    """One kept-alive HTTPS connection: some networks (like the build box) leave through a different IPv4 on every
    new connection, and the lobby only lists the address the request came from."""
    def __init__(self, url):
        import http.client
        self.host = urllib.parse.urlparse(url).hostname
        self.c = http.client.HTTPSConnection(self.host, timeout=15)

    def __call__(self, method, path, form=None):
        body = None if form is None else urllib.parse.urlencode(form)
        hdr = {'User-Agent': UA, 'Connection': 'keep-alive'}
        if body is not None:
            hdr['Content-Type'] = 'application/x-www-form-urlencoded'
        self.c.request(method, path, body=body, headers=hdr)
        r = self.c.getresponse()
        return r.status, r.read().decode()


def smoke(url):
    c = Conn(url)
    st, trace = c('GET', '/cdn-cgi/trace')   # the address Cloudflare sees on this connection
    m = re.search(r'^ip=(\d+\.\d+\.\d+\.\d+)$', trace, re.M)
    if not m:
        sys.exit('smoke: this machine reaches Cloudflare without IPv4; skipped')
    ip = m.group(1)
    tag = 'ZZTEST' + secrets.token_hex(2).upper()
    form = {'name': tag, 'hero': '0', 'heroName': 'SPIDER-MAN', 'players': '1', 'max': '2', 'protocol': '1', 'addr': ip + ':39999'}
    st, body = c('POST', '/v1/host', form)
    assert st == 200 and body.startswith('ok\t'), ('host', st, body)
    _, gid, key = body.split('\n')[0].split('\t')
    st, body = c('GET', '/v1/list')
    assert st == 200 and body.startswith('HVL1\n') and tag in body and ip not in body, ('list', st, body)
    print('smoke: listed %s (id %s); the list shows no address' % (tag, gid))
    st, body = c('POST', '/v1/join', {'id': gid, 'name': 'peter', 'protocol': '1'})
    assert st == 200 and body.startswith('ok\t' + ip + ':39999'), ('join', st, body)
    print('smoke: join hands out the host address; relay line: %s' % ('yes' if 'relay\t' in body else 'none (direct only)'))
    c('POST', '/v1/leave', {'id': gid, 'key': key})
    st, body = c('GET', '/v1/list')
    assert tag not in body, ('leave', body)
    print('smoke: leave takes it off the list')
    tag2 = tag + 'X'
    st, body = c('POST', '/v1/host', dict(form, name=tag2))
    assert st == 200 and body.startswith('ok\t'), ('host2', st, body)
    gid2 = body.split('\t')[1]
    st, body = c('GET', '/v1/list')
    assert tag2 in body, ('list2', body)
    print('smoke: listed %s; waiting 50 s for it to expire (no heartbeats)...' % tag2)
    for _ in range(10):  # keep the list read (not the game) alive, as launchers do
        time.sleep(5)
        http('GET', url + '/v1/health')
    st, body = http('GET', url + '/v1/list')
    assert st == 200 and tag2 not in body, ('expire', body)
    st, body = http('POST', url + '/v1/join', {'id': gid2, 'name': 'peter', 'protocol': '1'})
    assert body.startswith('err\tgone'), ('expired join', body)
    print('smoke: expired after 45 s without a heartbeat; joining it says "gone". OK')


def main():
    args = sys.argv[1:]
    if '--smoke-only' in args:   # LOBBY_URL=https://... (no token needed)
        smoke(os.environ['LOBBY_URL'].rstrip('/'))
        return
    token = os.environ.get('CLOUDFLARE_API_TOKEN', '').strip()
    if not token:
        sys.exit('set CLOUDFLARE_API_TOKEN (the "Edit Cloudflare Workers" token) first')
    account = os.environ.get('CLOUDFLARE_ACCOUNT_ID', '').strip()
    if not account:
        st, j = api('GET', '/accounts', token)
        ids = [a['id'] for a in j.get('result') or []]
        if len(ids) != 1:
            sys.exit('set CLOUDFLARE_ACCOUNT_ID (the token sees %d accounts: %s)' % (len(ids), errors(j)))
        account = ids[0]
    # 1. the token (user token or account token)
    st, j = api('GET', '/accounts/%s/tokens/verify' % account, token)
    if not (st == 200 and j.get('success')):
        st, j = api('GET', '/user/tokens/verify', token)
    if not (st == 200 and j.get('success')):
        sys.exit('the token is not valid: ' + errors(j))
    print('token: active; account %s' % account)
    # 2. the account's workers.dev subdomain
    st, j = api('GET', '/accounts/%s/workers/subdomain' % account, token)
    sub = (j.get('result') or {}).get('subdomain') if st == 200 else None
    if not sub:
        for want in ['herovr', 'herovr-lobby'] + ['herovr-' + secrets.token_hex(3) for _ in range(3)]:
            st, j = api('PUT', '/accounts/%s/workers/subdomain' % account, token, {'subdomain': want})
            if st == 200 and j.get('success'):
                sub = j['result']['subdomain']
                print('workers.dev: registered %s.workers.dev' % sub)
                break
            print('workers.dev: %s not possible (%s)' % (want, errors(j)))
            if 'verif' in errors(j).lower():
                sys.exit('Cloudflare wants the account e-mail verified first: click the link in the sign-up e-mail, then run this again.')
        if not sub:
            sys.exit('could not register a workers.dev subdomain')
    else:
        print('workers.dev: %s.workers.dev' % sub)
    # 3. deploy (wrangler reads CLOUDFLARE_API_TOKEN / CLOUDFLARE_ACCOUNT_ID from the environment)
    env = dict(os.environ, CLOUDFLARE_ACCOUNT_ID=account, WRANGLER_SEND_METRICS='false', CI='1')
    r = subprocess.run(['npx', '--yes', 'wrangler@4', 'deploy'], cwd=str(LOBBY), env=env, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(r.stdout.replace(token, '<token>'))
    if r.returncode != 0:
        sys.exit('wrangler deploy failed (above)')
    url = 'https://%s.%s.workers.dev' % (worker_name(), sub)
    # 4. health (a brand-new workers.dev name can take a few minutes to resolve)
    deadline = time.time() + 600
    while True:
        st, body = http('GET', url + '/v1/health')
        if st == 200 and body.strip() == 'ok':
            print('health: %s/v1/health -> ok' % url)
            break
        if time.time() > deadline:
            sys.exit('health: no "ok" from %s after 10 min (%s %s)' % (url, st, body[:120]))
        time.sleep(10)
    # 5. the launcher's default
    if '--no-set-url' not in args:
        set_url(url)
    # 6. a round trip
    if '--smoke' in args:
        smoke(url)
    print('LOBBY URL: ' + url)


if __name__ == '__main__':
    main()
