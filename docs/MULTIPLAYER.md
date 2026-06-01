# Multiplayer & Linux server

> Status: **experimental.** The networking in this fork is the original Build-engine
> peer-to-peer lockstep over raw UDP (`Engine/src/mmulti.c`), revived and fixed for
> 64-bit little-endian targets (Apple arm64, Linux x86_64/arm64). It is **not** a
> modern client/server netcode. The config-file `server`/`client` modes are stubs;
> everything runs in **peer mode**.

## How the netcode works

- **Lockstep, deterministic simulation.** Every peer runs the full game simulation.
  Each tic, peers exchange only their input ("sync") packets; the world is recomputed
  identically on every machine. There is no authoritative server state — only inputs
  travel the wire.
- **Peer-to-peer mesh.** Each peer must be able to reach *every other* peer. There is
  no traffic relaying. A peer on a public IP is reachable by everyone; two peers behind
  separate NATs generally cannot reach each other directly.
- **Determinism is mandatory.** All peers must use the **same GRP** and the **same map**
  (a map-CRC mismatch is reported in-game). A single non-deterministic divergence shows
  the "Out Of Sync" banner.

## Game modes

Modes are selected on the command line and honored by the auto-start path
(`Game/src/game.c`, the `numplayers > 1 && boardfilename` block):

| Flag  | Mode                       |
|-------|----------------------------|
| `-c1` | Dukematch (spawn) — weapons/items respawn |
| `-c2` | Cooperative                |
| `-c3` | Dukematch (no spawn)       |
| `-m`  | Monsters off (typical for deathmatch) |
| `-t1` | Respawn monsters           |
| `-f1` / `-f2` / `-f4` | Moves per packet — higher = more inputs batched per packet, tolerates higher latency at the cost of input delay |

Player-vs-player damage turns on automatically in any non-coop mode (`ud.coop != 1`).
Deathmatch uses the exact same netcode as coop; only the rules differ.

## Net config file (`-net <file>`)

Plain text, one directive per line:

```
interface <my-ip>:<my-port>     # the address THIS peer binds/listens on
mode peer                       # only "peer" is functional (server/client are stubs)
allow <other-ip>:<other-port>   # one line per remote peer this peer may talk to
```

Default UDP port is **1635**. Example for a public Linux host + one remote player:

`server.cfg` (on the Linux box, public IP a.b.c.d):
```
interface 0.0.0.0:1635
mode peer
allow <client-public-ip>:1635
```

`client.cfg` (on the player's machine):
```
interface 0.0.0.0:1635
mode peer
allow a.b.c.d:1635
```

For more than two players, add an `allow` line per other peer **on every peer**
(full mesh). See the NAT caveat below.

## Building on Linux

The CMake build is cross-platform. On a Debian/Ubuntu server:

```sh
# Toolchain + SDL3 build dependencies (FetchContent compiles SDL3 / SDL3_mixer)
sudo apt update
sudo apt install -y build-essential cmake git \
    libasound2-dev libpulse-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
    libwayland-dev libxkbcommon-dev libegl1-mesa-dev \
    libfluidsynth-dev          # optional: MIDI music (irrelevant for a headless server)

git clone https://github.com/rcoural/dark_chocolate_duke3d.git
cd dark_chocolate_duke3d
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
# -> build/chocolate-duke3d
```

A headless server needs none of the X11/Wayland/audio backends at runtime (it uses the
SDL `dummy` drivers), but SDL3's CMake probes for them at *build* time; the `-dev`
packages above let it build a full SDL3. On a truly minimal box you can drop the X11/
Wayland/audio dev packages — SDL3 still builds with the always-present `dummy` drivers.

### What was needed to make the Linux build work

- CMake defines `PLATFORM_UNIX` for non-Apple Unix so `platform.h` selects
  `unix_compat.h`.
- `Game/src/global.h` defines `BYTE_ORDER` for `__linux__` (little-endian).
- `Engine/src/display.c` includes `<unistd.h>` for `getpid()`.

> These are applied in the source but the Linux build has **not yet been test-compiled**
> on a Linux host from this fork — verify on first build and report any remaining
> platform gaps.

## Running a headless server

Place `DUKE3D.GRP` next to the binary, then:

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    ./build/chocolate-duke3d -net server.cfg -map E1L1 -c1 -m
```

- `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` — no window, no audio device required.
- `-map E1L1` loads E1L1 (from the GRP) as the multiplayer board. Use any map with
  multiplayer/deathmatch start markers.
- `-c1 -m` — Dukematch, monsters off. Drop `-c1` for cooperative.

Open the UDP port on the firewall:

```sh
sudo ufw allow 1635/udp
```

Set `MP_DIAG=1` to print mode/desync diagnostics to stdout
(`MP_DIAG enterlevel: ... coop=...`, `MP_DIAG OOS count=...`) — useful when debugging a
live game with remote clients.

## Known limitations

- **No real dedicated server.** The headless peer occupies a player slot; it is not a
  spectator/router. True server routing would require implementing the stubbed UDP
  `server` mode in `mmulti.c`.
- **NAT.** Works cleanly when every peer can reach every other peer — a public-IP host
  with directly-reachable clients, a LAN, or a VPN (e.g. WireGuard/Tailscale) that puts
  all players on one routable subnet. Multiple clients behind separate NATs cannot mesh
  without port forwarding or a VPN, because there is no hole-punching or relay.
- **Determinism.** Identical GRP + map on every peer is required.

## Tested

- Two headless peers on loopback, coop and Dukematch (`-c1`): both reach the level,
  `numplayers=2`, 0 out-of-sync over 20+ seconds.
- Headless peer (server model) + windowed peer (client): connect, both in-level, 0
  desync.
- Real WAN play (latency / packet loss across the internet) is **not yet tested**;
  tune with `-f2` / `-f4` if input feels delayed or sync breaks under loss.
