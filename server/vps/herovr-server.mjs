#!/usr/bin/env node
// HERO VR server - everything on one small VPS (docs/LOBBY-OWNER-SETUP.md): the internet server list (HTTP on
// 127.0.0.1:8080; Caddy in front gives it HTTPS) and the relay (UDP 3479) in one process. Node 18+, no packages.
// The list keeps nothing on disk: a restart empties it and the hosting launchers re-list within 5 s.
//   /etc/herovr.env: RELAY_SECRET=<random>  RELAY_ADDR=<public ip>:3479  [HTTP_PORT=8080] [RELAY_PORT=3479]
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3. Made by Agent Mulder.
import http from 'node:http';
import crypto from 'node:crypto';
import { Lobby, makeToken } from '../lobby/src/lobby.mjs';
import { startRelay } from '../relay/relay-core.mjs';

const env = process.env;
const secret = env.RELAY_SECRET || '';
const relayAddr = env.RELAY_ADDR || '';
const httpPort = Number(env.HTTP_PORT || 8080), relayPort = Number(env.RELAY_PORT || 3479);
const local = process.argv.includes('--local');   // tests: private addresses allowed, no proxy
if (!secret) { console.error('RELAY_SECRET is not set (see /etc/herovr.env)'); process.exit(1); }

const hmac = async (key, data) => new Uint8Array(crypto.createHmac('sha256', Buffer.from(key)).update(Buffer.from(data)).digest());
const lobby = new Lobby({
  random: (n) => new Uint8Array(crypto.randomBytes(n)),
  relay: relayAddr ? { addr: relayAddr, token: (r4) => makeToken(hmac, secret, r4) } : null,
  allowPrivate: local,
});
const relay = startRelay({ port: relayPort, secret, onReady: () => console.log(`HERO VR relay on UDP ${relayPort}`) });

const server = http.createServer((req, res) => {
  let body = '';
  req.on('data', (c) => { body += c; if (body.length > 4096) { res.writeHead(413); res.end('err\ttoo big'); req.destroy(); } });
  req.on('end', async () => {
    try {
      const url = new URL(req.url, 'http://x');
      if (url.pathname === '/v1/stats' && req.socket.remoteAddress === '127.0.0.1' && !req.headers['x-forwarded-for']) {
        res.writeHead(200, { 'content-type': 'application/json' });
        res.end(JSON.stringify({ games: lobby.hosts.size, relay: relay.stats() }));
        return;
      }
      const form = req.method === 'POST' ? Object.fromEntries(new URLSearchParams(body)) : {};
      // Behind Caddy on this machine: the player's address is the last X-Forwarded-For entry Caddy added.
      let client = (req.socket.remoteAddress || '').replace(/^::ffff:/, '');
      const fwd = String(req.headers['x-forwarded-for'] || '').split(',').map((s) => s.trim()).filter(Boolean);
      if (!local && client === '127.0.0.1' && fwd.length) client = fwd[fwd.length - 1].replace(/^::ffff:/, '');
      const r = await lobby.handle(req.method, url.pathname, form, client, Date.now() / 1000);
      res.writeHead(r.status, { 'content-type': 'text/plain; charset=utf-8', 'cache-control': 'no-store' });
      res.end(r.body);
    } catch (e) {
      res.writeHead(500); res.end('err\tserver');
    }
  });
});
server.listen(httpPort, '127.0.0.1', () => console.log(`HERO VR list on http://127.0.0.1:${httpPort} (relay ${relayAddr || 'off'})`));
