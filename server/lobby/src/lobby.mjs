// HERO VR internet server list - the lobby logic (pure; the Worker in worker.mjs is the glue).
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3. Made by Agent Mulder.
//
// Plain text over HTTPS so the launcher (.NET 4.8, no JSON library) reads it with a split:
//   GET  /v1/list                      -> "HVL1\n" + one line per game: id name hero heroName players max protocol lan (TAB)
//   POST /v1/host  name hero heroName players max protocol addr lan [id key]   (every 5-30 s while the game runs)
//                                      -> "ok\t<id>\t<key>\n" [+ "relay\t<host:port>\t<token>\n"] + "join\t<ip:port>\t<name>\n"...
//   POST /v1/join  id name [addr]      -> "ok\t<host ip:port>\n" [+ relay line]   or "err\t<gone|full|version|...>"
//                                         (first without addr: the joiner's game is not running yet; then again with
//                                         the address its STUN found, which puts it on the host's punch list)
//   POST /v1/leave id key              -> "ok"
// addr is the game's own public address (what STUN told it). The list never shows an address: a host's
// address goes only to a player who presses JOIN, and that player's address only to that host. lan: the game's
// LAN beacon id (8 hex), so a launcher that also hears the game on its own network shows it once (LAN first).
export const PROTOCOL_LIST = 'HVL1';
export const HOST_TTL = 45;          // s without a heartbeat (the launcher sends one every 15 s)
export const JOIN_TTL = 90;          // s a joiner stays on the host's punch list
export const MAX_HOSTS = 500;
export const HOSTS_PER_IP = 3;
export const REQUESTS_PER_MINUTE = 60;

export function cleanName(raw) {
  let s = '';
  for (const ch of String(raw || '').toUpperCase()) {
    const c = ch.charCodeAt(0);
    if (c >= 32 && c < 127 && ch !== '\t') s += ch;
    if (s.length >= 16) break;
  }
  return s.trim();
}

export function parseV4(addr) {
  const m = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}):(\d{1,5})$/.exec(String(addr || '').trim());
  if (!m) return null;
  const o = m.slice(1, 5).map(Number), port = Number(m[5]);
  if (o.some((x) => x > 255) || port < 1 || port > 65535) return null;
  // public only: no 0/8, 10/8, 127/8, 169.254/16, 172.16/12, 192.168/16, 100.64/10 (CGNAT), multicast and up
  const [a, b] = o;
  if (a === 0 || a === 10 || a === 127 || a >= 224 || (a === 169 && b === 254) || (a === 172 && b >= 16 && b <= 31) ||
      (a === 192 && b === 168) || (a === 100 && b >= 64 && b <= 127)) return null;
  return { ip: o.join('.'), port, text: o.join('.') + ':' + port };
}

const int = (v, lo, hi, d) => { const n = Number.parseInt(v, 10); return Number.isFinite(n) && n >= lo && n <= hi ? n : d; };

export class Lobby {
  // random(n): n random bytes (Uint8Array); relay: { addr, token(randomBytes) -> Promise<hex16> } or null
  constructor({ random, relay = null, allowPrivate = false } = {}) {
    this.random = random;
    this.relay = relay;
    this.allowPrivate = allowPrivate;   // tests on one machine (127.0.0.1)
    this.hosts = new Map();             // id -> host
    this.rate = new Map();              // client ip -> { minute, n }
  }

  hex(n) { return [...this.random(n)].map((b) => b.toString(16).padStart(2, '0')).join(''); }

  addr(text) {
    if (this.allowPrivate) {
      const m = /^(\d{1,3}(?:\.\d{1,3}){3}):(\d{1,5})$/.exec(String(text || '').trim());
      if (m && Number(m[2]) >= 1 && Number(m[2]) <= 65535) return { ip: m[1], port: Number(m[2]), text: m[1] + ':' + m[2] };
    }
    return parseV4(text);
  }

  sweep(now) {
    for (const [id, h] of this.hosts) {
      if (now - h.seen > HOST_TTL) this.hosts.delete(id);
      else h.joins = h.joins.filter((j) => now - j.at <= JOIN_TTL);
    }
  }

  limited(now, client) {
    const minute = Math.floor(now / 60);
    const r = this.rate.get(client);
    if (!r || r.minute !== minute) { this.rate.set(client, { minute, n: 1 }); if (this.rate.size > 20000) this.rate.clear(); return false; }
    return ++r.n > REQUESTS_PER_MINUTE;
  }

  // client: the caller's IP as Cloudflare saw it (CF-Connecting-IP). Returns { status, body }.
  async handle(method, path, form, client, now) {
    this.sweep(now);
    if (this.limited(now, client)) return { status: 429, body: 'err\tslow down' };
    if (method === 'GET' && path === '/v1/health') return { status: 200, body: 'ok' };
    if (method === 'GET' && path === '/v1/list') {
      const rows = [...this.hosts.values()].sort((a, b) => a.name.localeCompare(b.name) || a.id.localeCompare(b.id))
        .map((h) => [h.id, h.name, h.hero, h.heroName, h.players, h.max, h.protocol, h.lan].join('\t'));
      return { status: 200, body: PROTOCOL_LIST + '\n' + rows.join('\n') + (rows.length ? '\n' : '') };
    }
    if (method !== 'POST') return { status: 404, body: 'err\tnot found' };
    if (path === '/v1/host') return this.host(form, client, now);
    if (path === '/v1/join') return this.join(form, client, now);
    if (path === '/v1/leave') {
      const h = this.hosts.get(form.id);
      if (h && h.key === form.key) this.hosts.delete(form.id);
      return { status: 200, body: 'ok' };
    }
    return { status: 404, body: 'err\tnot found' };
  }

  // An IPv4 caller must be the address it lists (no listing someone else's IP); an IPv6 caller (the launcher's
  // HTTPS may go over IPv6 while the game's UDP is IPv4) is taken at its STUN word.
  matches(client, a) { return !/^\d+\.\d+\.\d+\.\d+$/.test(client) || client === a.ip || this.allowPrivate; }

  async relayLine(h) {
    if (!this.relay || !this.relay.addr) return '';
    if (!h.token) h.token = await this.relay.token(this.random(4));
    return 'relay\t' + this.relay.addr + '\t' + h.token + '\n';
  }

  async host(f, client, now) {
    const a = this.addr(f.addr);
    if (!a) return { status: 400, body: 'err\taddress' };
    if (!this.matches(client, a)) return { status: 403, body: 'err\taddress is not yours' };
    let h = f.id ? this.hosts.get(f.id) : null;
    if (h && h.key !== f.key) return { status: 403, body: 'err\tkey' };
    if (!h) {
      if (this.hosts.size >= MAX_HOSTS) return { status: 503, body: 'err\tlist full' };
      if ([...this.hosts.values()].filter((x) => x.client === client).length >= HOSTS_PER_IP) return { status: 429, body: 'err\ttoo many games' };
      h = { id: this.hex(6), key: this.hex(12), joins: [], token: '' };
      this.hosts.set(h.id, h);
    }
    Object.assign(h, {
      client, addr: a.text, seen: now,
      name: cleanName(f.name) || 'HERO VR', hero: int(f.hero, 0, 255, 255), heroName: cleanName(f.heroName),
      players: int(f.players, 1, 16, 1), max: int(f.max, 1, 16, 2), protocol: int(f.protocol, 0, 255, 0),
      lan: /^[0-9a-f]{1,8}$/i.test(String(f.lan || '')) ? String(f.lan).toLowerCase() : '',
    });
    let body = 'ok\t' + h.id + '\t' + h.key + '\n' + (await this.relayLine(h));
    for (const j of h.joins) body += 'join\t' + j.addr + '\t' + j.name + '\n';
    return { status: 200, body };
  }

  async join(f, client, now) {
    const h = this.hosts.get(f.id);
    if (!h) return { status: 404, body: 'err\tgone' };
    if (int(f.protocol, 0, 255, h.protocol) !== h.protocol) return { status: 409, body: 'err\tversion' };
    if (h.players >= h.max && !(f.addr && h.joins.find((j) => j.addr === String(f.addr).trim())))
      return { status: 409, body: 'err\tfull' };
    if (!f.addr) return { status: 200, body: 'ok\t' + h.addr + '\n' + (await this.relayLine(h)) };
    const a = this.addr(f.addr);
    if (!a) return { status: 400, body: 'err\taddress' };
    if (!this.matches(client, a)) return { status: 403, body: 'err\taddress is not yours' };
    const again = h.joins.find((j) => j.addr === a.text);
    if (again) again.at = now;
    else {
      h.joins.push({ addr: a.text, name: cleanName(f.name) || 'PLAYER', at: now });
      if (h.joins.length > 8) h.joins.shift();
    }
    return { status: 200, body: 'ok\t' + h.addr + '\n' + (await this.relayLine(h)) };
  }
}

// The relay token: 4 random bytes + the first 4 bytes of HMAC-SHA256(secret, those 4 bytes), hex. The relay
// (server/relay) checks it, so only tokens this lobby gave out are relayed. hmac(keyBytes, dataBytes) -> bytes.
export async function makeToken(hmac, secret, rand4) {
  const mac = await hmac(new TextEncoder().encode(secret), rand4);
  return [...rand4, ...mac.slice(0, 4)].map((b) => b.toString(16).padStart(2, '0')).join('');
}
