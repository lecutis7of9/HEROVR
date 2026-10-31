# HERO VR server list logic

- `src/lobby.mjs`: the list, the join hand-off and the relay tokens. It's pure logic, used by:
  - `../vps/herovr-server.mjs` (the chosen deploy, see ../vps/README.md);
  - `src/worker.mjs` (the alternative: Cloudflare Workers + a Durable Object, `wrangler deploy`);
  - `dev-server.mjs` (tests and local try-outs, 127.0.0.1 only).
- The protocol is plain text (`HVL1`). The list never contains an IP address.
