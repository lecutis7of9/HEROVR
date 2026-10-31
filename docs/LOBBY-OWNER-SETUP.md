# HERO VR internet server list - owner setup (about $4-6 a month)

Players do nothing: they open the launcher, see the games, and press JOIN.
For that, the mod needs ONE small server on the internet. You rent it once.

## The choice: one DigitalOcean server

- One server does all three jobs: the server list, the "meeting point" that
  connects two players directly, and the relay (the backup path when two home
  routers will not connect directly).
- One bill, one address, one thing to keep running. The traffic is tiny.
- The relay makes joining work for almost everyone, including strict routers
  and mobile hotspots. A free Cloudflare list cannot relay game traffic, so it
  would still need a second server somewhere else. That is why it is all on one server.
- DigitalOcean has the clearest website of the cheap providers, and it has a
  New York region close to you (US Eastern).

**Plan:** Droplet, Basic, Regular, **$6/month** (1 GB memory, 1 TB traffic).
The $4/month one (512 MB) also works, but $6 gives room to spare.
**Region:** New York (NYC1 or NYC3).
**Cost:** $6 a month, billed by the hour. Delete the server and the billing stops.

## Your steps (about 10 minutes)

Tip for reading: press **Ctrl and +** in the browser to make the page bigger.

1. Go to **https://www.digitalocean.com** and click **Sign up**.
   - Sign up with your Google or GitHub account (fewest steps), or with an email.
   - Add a payment card when asked. DigitalOcean may show a small
     temporary charge to check the card.
2. When you see the dashboard, stop there. You do **not** need to create a
   server yourself. Choose **ONE** of these two ways.

### Way A (easiest): give us a token, and we set up everything

3. On the left, click **API**. (If you can't see it, open
   https://cloud.digitalocean.com/account/api/tokens)
4. Click **Generate New Token**.
   - Name: `herovr`
   - Expiration: **30 days**
   - Scopes: **Full Access**
   - Click **Generate Token**.
5. A long code starting with `dop_v1_` appears **once**. Click the copy
   icon next to it and send it to us in the private chat.
6. That is all. We create the New York $6 server, install the HERO VR
   server on it, and put its address into the launcher.
   When it works, you can delete the token on the same API page (the
   trash icon). The server keeps running without it.

### Way B: you create the server, and send us its address

3. Click the green **Create** button (top right), then **Droplets**.
4. Region: **New York**. Image: **Ubuntu 24.04 (LTS) x64**.
5. Size: **Basic**, then **Regular**, then the **$6/mo** box.
6. Authentication: choose **Password** and type a long password. Keep it
   (your password manager is fine).
7. Click **Advanced Options**, then tick **Add Initialization scripts**.
   Paste the whole contents of `server/vps/cloud-init.sh` (we give it to you as one block to copy).
8. Hostname: `herovr`. Click **Create Droplet**.
9. After about a minute, the droplet shows an address like `164.90.x.x`.
   Send us that address. Do not send the password; we don't need it,
   because the script sets everything up by itself.

## What happens next (we do this)

- Way A, the one command: `DO_TOKEN=<the token> python3 server/vps/deploy_digitalocean.py`
- Way B: we put the address into the launcher (no command needed, the server set itself up).

- The server gets a free HTTPS name of the form `IP-WITH-DASHES.sslip.io`
  (for example `164-90-1-2.sslip.io`). You don't need to buy a domain.
- The launcher's server list address (`InternetLobby.DefaultUrl`) is set to
  `https://164-90-1-2.sslip.io`, and the next test build ships with it.
- Check it in a browser: open `https://164-90-1-2.sslip.io/v1/health`. It should say `ok`.

## Safety and privacy, short

- The list shows only names, heroes and player counts, never addresses.
- A host's internet address goes only to the one player who presses JOIN,
  and that player's address only to that host. This is the same as any
  peer-to-peer game.
- The relay passes on traffic only for games the list set up, using a
  secret code on the server. Strangers cannot use it to send other traffic.
- There are limits on requests per address and on games per address.
- The server does not store anything on disk. Games vanish from the list 45 s after they stop.

## If you want to stop

DigitalOcean > Droplets > `herovr` > **Destroy**. Billing stops. The mod
falls back to "same network" (LAN) games only. Nothing breaks.
