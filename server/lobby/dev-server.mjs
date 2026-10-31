#!/usr/bin/env node
// HERO VR lobby on this PC, for tests and a try-out without Cloudflare: the same lobby.mjs over plain HTTP.
//   node dev-server.mjs [port=8787] [--relay=127.0.0.1:3479] [--secret=s] [--for=seconds]
// Private addresses (127.0.0.1, 192.168.x) are allowed here, never in the Worker.
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3. Made by Agent Mulder.
import http from 'node:http';
import crypto from 'node:crypto';
import { Lobby, makeToken } from './src/lobby.mjs';

const arg = (k, d) => { const a = process.argv.find((x) => x.startsWith('--' + k + '=')); return a ? a.slice(k.length + 3) : d; };
const port = Number(process.argv.find((a) => /^\d+$/.test(a)) || 8787);
const relayAddr = arg('relay', ''), secret = arg('secret', 'dev-secret');
const hmac = async (key, data) => new Uint8Array(crypto.createHmac('sha256', Buffer.from(key)).update(Buffer.from(data)).digest());
const lobby = new Lobby({
  random: (n) => new Uint8Array(crypto.randomBytes(n)),
  relay: relayAddr ? { addr: relayAddr, token: (r4) => makeToken(hmac, secret, r4) } : null,
  allowPrivate: true,
});
const server = http.createServer((req, res) => {
  let body = '';
  req.on('data', (c) => { body += c; if (body.length > 4096) req.destroy(); });
  req.on('end', async () => {
    const url = new URL(req.url, 'http://x');
    const form = req.method === 'POST' ? Object.fromEntries(new URLSearchParams(body)) : {};
    const client = (req.socket.remoteAddress || '').replace(/^::ffff:/, '');
    const r = await lobby.handle(req.method, url.pathname, form, client, Date.now() / 1000);
    res.writeHead(r.status, { 'content-type': 'text/plain; charset=utf-8' });
    res.end(r.body);
  });
});
server.listen(port, '127.0.0.1', () => console.log(`HERO VR lobby (dev) on http://127.0.0.1:${port}`));
const forS = Number(arg('for', '0'));
if (forS > 0) setTimeout(() => process.exit(0), forS * 1000);
