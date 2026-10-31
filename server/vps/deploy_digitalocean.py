#!/usr/bin/env python3
"""The one deploy command (docs/LOBBY-OWNER-SETUP.md, Way A): creates the HERO VR server on DigitalOcean from the
owner's API token, waits until https://IP.sslip.io/v1/health says ok, and sets InternetLobby.DefaultUrl.
    DO_TOKEN=dop_v1_... python3 server/vps/deploy_digitalocean.py [--region nyc3] [--size s-1vcpu-1gb]
It makes ONE droplet named herovr ($6/month; s-1vcpu-512mb-10gb is the $4 one) and nothing else; if a droplet named
herovr exists it is reused, never a second one. Python 3 standard library only.
SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3. Made by Agent Mulder."""
import argparse
import json
import os
import pathlib
import re
import sys
import time
import urllib.error
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
API = 'https://api.digitalocean.com/v2'


def call(token, method, path, body=None):
    req = urllib.request.Request(API + path, method=method, data=None if body is None else json.dumps(body).encode(),
                                 headers={'Authorization': 'Bearer ' + token, 'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read() or b'{}')


def public_ip(d):
    for n in d.get('networks', {}).get('v4', []):
        if n.get('type') == 'public':
            return n['ip_address']
    return None


def set_default_url(url):
    f = ROOT / 'tools/launcher/src/InternetLobby.cs'
    text = f.read_text(encoding='utf-8')
    new = re.sub(r'public const string DefaultUrl = "[^"]*";', 'public const string DefaultUrl = "%s";' % url, text, count=1)
    f.write_text(new, encoding='utf-8')
    return new != text


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--region', default='nyc3')
    ap.add_argument('--size', default='s-1vcpu-1gb')
    ap.add_argument('--no-set-url', action='store_true')
    a = ap.parse_args()
    token = os.environ.get('DO_TOKEN', '').strip()
    if not token:
        sys.exit('DO_TOKEN is not set (the owner\'s API token, docs/LOBBY-OWNER-SETUP.md)')
    found = [d for d in call(token, 'GET', '/droplets?per_page=200')['droplets'] if d['name'] == 'herovr']
    if found:
        d = found[0]
        print('Reusing the droplet herovr (id %s).' % d['id'])
    else:
        d = call(token, 'POST', '/droplets', {
            'name': 'herovr', 'region': a.region, 'size': a.size, 'image': 'ubuntu-24-04-x64',
            'user_data': (HERE / 'cloud-init.sh').read_text(), 'tags': ['herovr'], 'monitoring': False, 'ipv6': False,
        })['droplet']
        print('Created the droplet herovr (id %s) in %s, size %s.' % (d['id'], a.region, a.size))
    ip = None
    for _ in range(60):
        d = call(token, 'GET', '/droplets/%s' % d['id'])['droplet']
        ip = public_ip(d)
        if ip and d['status'] == 'active':
            break
        time.sleep(5)
    if not ip:
        sys.exit('The droplet has no public address after 5 minutes - look at cloud.digitalocean.com.')
    url = 'https://%s.sslip.io' % ip.replace('.', '-')
    print('Address %s - waiting for %s/v1/health (the setup takes 2-5 minutes)...' % (ip, url))
    for _ in range(90):
        try:
            if urllib.request.urlopen(url + '/v1/health', timeout=10).read().decode().strip() == 'ok':
                break
        except (urllib.error.URLError, OSError):
            pass
        time.sleep(10)
    else:
        sys.exit('Not answering after 15 minutes. The server log: ssh root@%s journalctl -u herovr -u caddy' % ip)
    print('UP: %s/v1/list' % url)
    if not a.no_set_url and set_default_url(url):
        print('Set InternetLobby.DefaultUrl = "%s" in tools/launcher/src/InternetLobby.cs - rebuild the launcher.' % url)


if __name__ == '__main__':
    main()
