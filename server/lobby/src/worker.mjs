// HERO VR internet server list - the Cloudflare Worker (free plan): every request goes to one Durable Object
// (one list for everybody, kept in memory; hosts re-register within 15 s if it restarts). See README.md.
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3. Made by Agent Mulder.
import { Lobby, makeToken } from './lobby.mjs';

const random = (n) => crypto.getRandomValues(new Uint8Array(n));
async function hmac(key, data) {
  const k = await crypto.subtle.importKey('raw', key, { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
  return new Uint8Array(await crypto.subtle.sign('HMAC', k, data));
}

export class LobbyRoom {
  constructor(state, env) {
    const relay = env.RELAY_ADDR && env.RELAY_SECRET
      ? { addr: env.RELAY_ADDR, token: (r4) => makeToken(hmac, env.RELAY_SECRET, r4) } : null;
    this.lobby = new Lobby({ random, relay });
  }
  async fetch(request) {
    const url = new URL(request.url);
    const form = request.method === 'POST' ? Object.fromEntries(new URLSearchParams(await request.text())) : {};
    const client = request.headers.get('CF-Connecting-IP') || '';
    const r = await this.lobby.handle(request.method, url.pathname, form, client, Date.now() / 1000);
    return new Response(r.body, { status: r.status, headers: { 'content-type': 'text/plain; charset=utf-8', 'cache-control': 'no-store' } });
  }
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (!url.pathname.startsWith('/v1/')) return new Response('HERO VR lobby', { status: 200 });
    if (request.headers.get('content-length') > 4096) return new Response('err\ttoo big', { status: 413 });
    return env.LOBBY.get(env.LOBBY.idFromName('global')).fetch(request);
  },
};
