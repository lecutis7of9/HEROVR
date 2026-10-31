// HERO VR relay core (used by relay.mjs on its own and by server/vps/herovr-server.mjs). See relay.mjs.
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3. Made by Agent Mulder.
import dgram from 'node:dgram';
import crypto from 'node:crypto';

export const KEEP = 30, MAX_TOKENS = 5000, PER_TOKEN = 4, MAX_PACKET = 1400, PER_SECOND = 400;

export function tokenOk(tok, key) {
  const mac = crypto.createHmac('sha256', key).update(tok.subarray(0, 4)).digest();
  return crypto.timingSafeEqual(mac.subarray(0, 4), tok.subarray(4, 8));
}

// startRelay({ port, secret, open, host }) -> { sock, stats() }
export function startRelay({ port = 3479, secret = '', open = false, host = '0.0.0.0', onReady } = {}) {
  const tokens = new Map();   // token hex -> { peers: Map(endpoint -> {address, port, seen}), second, n }
  let forwarded = 0, dropped = 0;
  const sock = dgram.createSocket('udp4');
  sock.on('error', (e) => console.error('relay: ' + e.message));
  sock.on('message', (msg, rinfo) => {
    if (msg.length < 12 || msg.length > MAX_PACKET || msg.toString('latin1', 0, 4) !== 'HVRY') { dropped++; return; }
    const tok = msg.subarray(4, 12);
    if (!open && !tokenOk(tok, secret)) { dropped++; return; }
    const key = tok.toString('hex'), now = Date.now() / 1000, me = rinfo.address + ':' + rinfo.port;
    let t = tokens.get(key);
    if (!t) {
      if (tokens.size >= MAX_TOKENS) { dropped++; return; }
      t = { peers: new Map(), second: 0, n: 0 };
      tokens.set(key, t);
    }
    const sec = Math.floor(now);
    if (t.second !== sec) { t.second = sec; t.n = 0; }
    if (++t.n > PER_SECOND) { dropped++; return; }
    for (const [k, p] of t.peers) if (now - p.seen > KEEP) t.peers.delete(k);
    if (!t.peers.has(me) && t.peers.size >= PER_TOKEN) { dropped++; return; }
    t.peers.set(me, { address: rinfo.address, port: rinfo.port, seen: now });
    if (msg.length === 12) return;   // a keepalive: only the address is learned
    for (const [k, p] of t.peers) if (k !== me) { sock.send(msg, p.port, p.address); forwarded++; }
  });
  setInterval(() => {
    const now = Date.now() / 1000;
    for (const [k, t] of tokens) {
      for (const [e, p] of t.peers) if (now - p.seen > KEEP) t.peers.delete(e);
      if (!t.peers.size) tokens.delete(k);
    }
  }, 10000).unref();
  sock.bind(port, host, () => { if (onReady) onReady(); });
  return { sock, stats: () => ({ forwarded, dropped, tokens: tokens.size }) };
}
