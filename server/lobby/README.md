# HERO VR server list logic

- `src/lobby.mjs`: the list, the join hand-off and the relay tokens. It's pure logic, used by:
  - `src/worker.mjs` (THE DEPLOY, free: Cloudflare Workers + a SQLite-backed Durable Object, no card, no relay).
    Live at `https://herovr-lobby.herovr.workers.dev`. Deploy / update:
    `CLOUDFLARE_API_TOKEN=... CLOUDFLARE_ACCOUNT_ID=... python3 server/lobby/deploy_cloudflare.py --smoke`
    (owner steps and the free limits: docs/LOBBY-OWNER-SETUP.md);
  - `../vps/herovr-server.mjs` (optional, later: a VPS with the relay, see ../vps/README.md);
  - `dev-server.mjs` (tests and local try-outs, 127.0.0.1 only).
- The protocol is plain text (`HVL1`). The list never contains an IP address.
