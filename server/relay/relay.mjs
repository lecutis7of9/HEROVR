#!/usr/bin/env node
// HERO VR relay - for the two players whose routers will not let a direct connection through (strict or shared
// NAT). Node.js, no packages. UDP: "HVRY" + 8-byte token + game packet; a packet goes to the other players
// that used the same token in the last 30 s, unchanged. Tokens come from the lobby (4 random bytes + HMAC), so
// it relays nothing for strangers. It never answers a packet itself (no reflection).
// SM2 VR (port of SpidyVR to Marvel's Spider-Man 2), GPLv3. Made by Agent Mulder.
//   RELAY_SECRET=<same as the lobby's> node relay.mjs [port=3479]      (--open: any token, for local tests only)
import { startRelay } from './relay-core.mjs';

const port = Number(process.argv.find((a) => /^\d+$/.test(a)) || process.env.RELAY_PORT || 3479);
const open = process.argv.includes('--open');
const secret = process.env.RELAY_SECRET || '';
if (!secret && !open) { console.error('RELAY_SECRET is not set (or --open for a local test)'); process.exit(1); }
const seconds = Number((process.argv.find((a) => a.startsWith('--for=')) || '--for=0').slice(6));
const relay = startRelay({ port, secret, open, onReady: () => console.log(`HERO VR relay on UDP ${port}${open ? ' (OPEN - tests only)' : ''}`) });
if (seconds > 0) setTimeout(() => { console.log(JSON.stringify(relay.stats())); relay.sock.close(); process.exit(0); }, seconds * 1000);
