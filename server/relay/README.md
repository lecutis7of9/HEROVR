# HERO VR relay

`relay-core.mjs` (`startRelay`) is used in-process by `../vps/herovr-server.mjs`.
`relay.mjs` runs it standalone: `RELAY_SECRET=... node relay.mjs 3479` (use `--open` for local tests only).
The frame is "HVRY" + an 8-byte token + the game packet. Tokens are 4 random bytes + HMAC-SHA256(secret)[0..4],
signed by the list.
