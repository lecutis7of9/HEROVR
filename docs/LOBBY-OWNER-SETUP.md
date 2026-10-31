# HERO VR internet server list - owner setup (FREE, no card)

Players do nothing: they open the launcher, see the games, and press JOIN.
The list runs free on Cloudflare: one small "Worker" with its own free storage.
No payment card, no server to rent, nothing to keep running.

**Live now:** `https://herovr-lobby.herovr.workers.dev` (deployed 30 Oct 2026; `/v1/health` says `ok`).
The launcher uses it by default (`InternetLobby.DefaultUrl`).

## What it does, and what it does not do

- **It does:** the server list, and the meeting point. Two games learn each other's
  internet address (from free public STUN) and call each other at the same time
  ("hole punching"). Most home routers let that through.
- **It does not do:** a relay. Two strict routers, or some mobile hotspots and
  carrier networks, will not connect directly. Expect roughly 1 in 5 to 1 in 10 pairs.
  Then the game stops trying after 30 s, and the launcher shows ONE line:
  "Could not connect to MARY: the two home networks did not let each other in.
  Press JOIN to try again, or HOST and let MARY join you." It does not hang.
  Swapping who hosts often helps. Same-network (LAN) games always work.
- A relay can be added later without a launcher update (see "Later: a relay").

## Free plan: checked against Cloudflare's docs (October 2026)

- **No card:** Workers' free plan is the default. "Start building for free - no credit
  card required" (cloudflare.com/developer-platform/products/workers). A card is only
  needed to upgrade to Workers Paid ($5/month), which we do not use.
- **Storage:** a SQLite-backed Durable Object (the only kind on the free plan;
  `new_sqlite_classes` in `server/lobby/wrangler.toml`). The list lives in its memory,
  so it uses no storage writes. KV is not used: its free plan allows only 1,000 writes a day.
- **Free limits per day** (reset at 00:00 UTC = 8 PM New York in summer, 7 PM in winter):
  - Workers: 100,000 requests, 10 ms CPU per request.
  - Durable Objects: 100,000 requests, 13,000 GB-s.
  - Over the limit, the list stops answering until the reset ("cannot reach the list"
    in the launcher). Nothing is charged; LAN games keep working.
- **Is it enough?** A launcher left open reads the list every 15 s: about 240
  requests an hour. A hosted game sends a heartbeat every 5 s (every 30 s when full):
  about 720 an hour. About 400 player-hours a day fit, for example 20 players for
  3 hours a day each is about 60,000 requests. Plenty for a small player base.
  If it ever fills up, Workers Paid ($5/month) raises it to 10 million a month.

## Your steps (about 5 minutes; no card)

Tip for reading: press **Ctrl and +** in the browser to make the page bigger.

1. Go to **https://dash.cloudflare.com/sign-up**. Type your email and a password
   (your password manager can make one), then click **Sign up**. No card is asked for.
2. Open the email from Cloudflare and click the link to **verify your email**.
   (Cloudflare will not give out the free `workers.dev` address before this.)
3. **The token.** In the dashboard, go to **Manage Account > Account API Tokens**
   (or click your profile icon, top right, then **My Profile > API Tokens**).
   - Click **Create Token**.
   - Pick the **Edit Cloudflare Workers** template: on the account page it is in the
     list of templates (or the **Custom** dropdown under **Permission policies**);
     on the My Profile page, click **Use template** next to it.
   - **Account Resources:** Include > your account. **Zone Resources:** All zones
     (you have no website, so this gives nothing extra).
   - Click **Continue to summary**, then **Create Token**.
   - The token is shown **once**. Click **Copy** and paste it into the private
     masked box we send you. Do not paste it into the normal chat.
4. **The account ID.** Click **Workers & Pages** on the left. On the right, under
   **Account Details**, click the copy icon next to **Account ID**.
   (Or press **Ctrl+K** in the dashboard and type "Copy account ID".) Send it to us; it is not secret.
5. That is all. When it works, you may delete the token
   (same API Tokens page, the **...** menu, **Delete**). The list keeps running without it.
   We only need a new token to update the Worker later.

## What we do (the one command, run from the build box)

    CLOUDFLARE_API_TOKEN=<token> CLOUDFLARE_ACCOUNT_ID=<account id> python3 server/lobby/deploy_cloudflare.py --smoke

It checks the token, registers the free `workers.dev` subdomain (`herovr`) if the account
has none, runs `npx wrangler@4 deploy` (the Worker + its Durable Object), waits for
`/v1/health` to say `ok`, writes the URL into `InternetLobby.DefaultUrl`, and with `--smoke`
lists a fake game from the box, joins it, removes it, and lists another one and waits for it
to vanish (45 s). The token is read from the environment only: never printed, never saved in a file.
Afterwards: rebuild the launcher. The game itself needs no setting (it gets everything from the launcher).

Check it in a browser: `https://herovr-lobby.herovr.workers.dev/v1/health` says `ok`.

## Later: a relay (optional, costs money)

For the pairs that cannot connect directly, a small VPS can run the relay
(`server/vps`, about $6/month at DigitalOcean; it needs a card). Then:

- run the relay on the VPS (`server/vps/cloud-init.sh`), with its `RELAY_SECRET`;
- on the Worker: set `RELAY_ADDR = "<vps ip>:3479"` in `server/lobby/wrangler.toml`,
  `npx wrangler secret put RELAY_SECRET` (the same value), and re-run the deploy command.

The Worker then hands out relay tokens, and games that could not meet directly go
through the relay after 15 s. The launcher needs no change (keep using
`deploy_digitalocean.py --no-set-url`, so the free Worker stays the list).

## Safety and privacy, short

- The list shows only names, heroes and player counts, never addresses.
- A host's internet address goes only to the one player who presses JOIN,
  and that player's address only to that host. This is the same as any
  peer-to-peer game.
- A game can only list the address it calls from (no listing someone else's).
- Limits: 60 requests a minute per address, 3 games per address, 500 games in total.
- Nothing is stored on disk. Games vanish from the list 45 s after they stop.

## If you want to stop

Cloudflare dashboard > **Workers & Pages** > `herovr-lobby` > **Settings** > **Delete**.
The mod falls back to "same network" (LAN) games only. Nothing breaks, nothing is billed.
