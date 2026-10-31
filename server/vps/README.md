# HERO VR server - one small VPS (the chosen design)

One Node.js process (`herovr-server.mjs`, no npm packages) does all of it:

| Job | Where | What |
|---|---|---|
| Server list | HTTPS 443 (Caddy) -> 127.0.0.1:8080 | `/v1/list`, `/v1/host`, `/v1/join`, `/v1/leave`, `/v1/health` (logic: `../lobby/src/lobby.mjs`) |
| Meeting point | same API | gives each side the other's public address so the games "punch" through their routers |
| Relay | UDP 3479 | the backup path when routers will not connect directly (`../relay/relay-core.mjs`) |

Public STUN (stun.cloudflare.com, stun.l.google.com) tells each game its public address. It's free and needs no account.

## Deploy (owner steps: docs/LOBBY-OWNER-SETUP.md)

- **Way A, with the owner's API token:**
  `DO_TOKEN=dop_v1_... python3 server/vps/deploy_digitalocean.py`
  This creates the droplet `herovr` (New York, $6/mo, Ubuntu 24.04), waits for `https://IP.sslip.io/v1/health`,
  and writes that URL into `InternetLobby.DefaultUrl`. Then rebuild the launcher.
- **Way B, the owner made the droplet with `cloud-init.sh` pasted as its initialization script:**
  set `InternetLobby.DefaultUrl = "https://A-B-C-D.sslip.io"` (the droplet IP with dashes), then rebuild.
- **On any Ubuntu 24.04 server:** `ssh root@IP 'bash -s' < server/vps/cloud-init.sh`

`cloud-init.sh` is GENERATED (`python3 server/vps/make-cloud-init.py`), and it inlines the server files. It does this:

- installs nodejs, caddy and ufw;
- writes `/etc/herovr.env` with a random `RELAY_SECRET` and `RELAY_ADDR=<public ip>:3479`;
- runs the service as an unprivileged `herovr` user under systemd;
- sets the firewall to 22/tcp, 80/tcp, 443/tcp and 3479/udp only.

## Run it

- **Status:** `ssh root@IP systemctl status herovr caddy`
- **Log:** `journalctl -u herovr -f`
- **Numbers:** `curl -s 127.0.0.1:8080/v1/stats` (only from the server itself)
- **Update the code:** re-run `cloud-init.sh` over ssh. It keeps the secret, overwrites the files, and restarts.
- **Local try-out:** `RELAY_SECRET=x RELAY_ADDR=127.0.0.1:3479 node server/vps/herovr-server.mjs --local`, then
  `set SM2VR_LOBBY_URL=http://127.0.0.1:8080` before starting the launcher.

## Limits (abuse)

- 60 requests a minute per IP.
- 3 listed games per IP, 500 in total.
- Hosts drop off 45 s after their last heartbeat.
- The relay forwards only tokens the list signed (HMAC), for at most 4 endpoints per token and 400 packets/s,
  with a 1400-byte maximum per packet. It never answers a packet itself (so it can't be used for reflection).
- Nothing is written to disk.

## Alternative kept: Cloudflare Worker

`server/lobby/worker.mjs` + `wrangler.toml` run the same list on Cloudflare's free tier (100k requests/day).
That still needs a relay somewhere (`server/relay/relay.mjs`), so it means two services instead of one.
