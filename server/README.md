# Dark Chocolate Duke3D relay server

A tiny dedicated **relay** for online play. The original Build netcode is
peer-to-peer lockstep, which the internet's NAT makes impractical: every player
would have to reach every other player directly. This relay fixes that without
touching the game logic. Clients make a single **outbound** UDP connection to
the relay (NAT-friendly), and the relay forwards each client's packets to the
other. The relay is **not** a player - it runs no game engine, needs no SDL and
no `DUKE3D.GRP`, so it builds and runs on a bare headless box.

Phase 1 hosts **2 players** per match. When a match goes idle the slots free up
for the next pair automatically; no restart needed.

## Build

```sh
make                      # -> ./duke3d-relay
# or, by hand:
cc -O2 -Wall -o duke3d-relay duke3d-relay.c
```

Pure C with only POSIX socket headers - builds on any Linux (incl. old LTS),
macOS, or BSD with a C compiler. No dependencies.

## Run

```sh
./duke3d-relay            # listens on 0.0.0.0:1635/udp
./duke3d-relay 1700       # custom port
```

Ctrl-C to stop.

## Deploy on a Linux VPS

```sh
# 1. copy the source up (run from your dev machine)
scp duke3d-relay.c duke3d-relay.service root@<SERVER_IP>:/root/

# 2. on the server: build
sudo apt-get update && sudo apt-get install -y gcc
cc -O2 -Wall -o /usr/local/bin/duke3d-relay /root/duke3d-relay.c

# 3. open the UDP port (host firewall + any cloud firewall too)
sudo ufw allow 1635/udp

# 4a. quick test, foreground
/usr/local/bin/duke3d-relay

# 4b. or run it always-on under systemd
sudo cp /root/duke3d-relay.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now duke3d-relay
sudo systemctl status duke3d-relay
journalctl -u duke3d-relay -f          # watch the log
```

> If your VPS is behind a cloud firewall (DigitalOcean, AWS, GCP, ...), you must
> also allow inbound **UDP 1635** there, not just in `ufw`.

## Connecting clients

**Manual:** point each client's net config `allow` line at the relay and use
`mode client`:

```
interface 0.0.0.0:0
mode client
allow <SERVER_IP>:1635
```

```sh
./chocolate-duke3d -net client.cfg -map E1L1 -c1 -m
```

**"Play Online":** the client's `-online` flag fetches the current relay address
over plain HTTP, so you can move the server without re-shipping the game. Host a
text file containing one line - `<SERVER_IP>:1635` - somewhere on the plain-HTTP
web, then tell the client where it lives. The discovery URL is **not** hard-coded
in the source (so your server's address never lands in a commit); configure it
per build instead:

```sh
# bake it into your own build (one of):
echo 'http://your.site/server.txt' > online_url.txt   # gitignored; cmake reads it
cmake -S . -B build -DDUKE_ONLINE_URL=http://your.site/server.txt

# or override at runtime, no rebuild:
DUKE_ONLINE_URL=http://your.site/server.txt ./chocolate-duke3d -online
```

> Must be plain **HTTP** returning `200` with the address in the body — the client
> can't follow HTTPS redirects (no TLS).

## Protocol (for reference)

Handshake packets lead with `0,0,0` so the lockstep game never mistakes them for
gameplay traffic. Layout matches `Engine/src/mmulti.c`:

| packet  | bytes | contents                                  |
|---------|-------|-------------------------------------------|
| HELLO   | 4     | `0,0,0,246`                               |
| WELCOME | 6     | `0,0,0,247, slot(1-based), numplayers`    |

Client sends HELLO until it gets a WELCOME (which assigns its player slot); then
every non-handshake packet is relayed verbatim to the other client.
