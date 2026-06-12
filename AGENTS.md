# AGENTS.md — CoreStack Challenge (tetriSH + TetriSocial)

**Team:** Mac Mini Team — Sanjan Krishna Sarat (1009153) · Thong Zi Qi (1009160)
**Repo:** <https://github.com/ziqiqiiii/tetriSH.git>

---

## When to read other docs

| Situation | File to read |
|-----------|-------------|
| User asks a specific project/architecture question | `cat docs/corestack.md` |
| "How does the handshake work?", "Explain the IPC design", "What does X component do?" | `cat docs/corestack.md` |
| Need full specs for a component you're about to implement | `cat docs/corestack.md` |
| An AGENTS.md rule references something and you need more depth | `cat docs/corestack.md` |
| Need Q&A prep answers, checkoff structure, or the live extension task pool | `cat docs/checkoff-prep.md` |
| Need the full FR/NFR tables, sprint plan, or binary linkage | `cat docs/requirements.md` |
| Need the full architecture prose (handshake steps, thread diagrams, ability flow) | `cat docs/architecture.md` |

**Do not proactively load these on every message.** Read `AGENTS.md` rules first. Open the reference docs only when the current task requires the detail inside them.

---

## What this project is

A terminal-based Battle Royale Tetris system written entirely in C, spanning two courses:

- **50.005** → `tetriSH`: shell + daemon + secure client-server game (replaces PA1 + PA2)
- **50.003** → `TetriSocial`: live chat (`chatd`) + points marketplace (`marketd`) as independent daemons

Both share a corestack of static libraries. Everything builds from one `Makefile`.

---

## Binaries and who owns them

| Binary | Owner | Purpose |
|--------|-------|---------|
| `tetrish` | Sanjan | Interactive shell, reads `.tetrishrc`, spawns daemons |
| `tetrisd` | Sanjan | Concurrent game server — the core of the system |
| `tetrislogd` | Sanjan | Dedicated logger daemon (separate process, not thread) |
| `tetrisctl` | Zi Qi | Admin CLI — talks to `tetrisd` via Unix socket, never via TCP |
| `tetrisu` | Both | ncurses terminal client — renders board, sends HTTTP moves |
| `chatd` | Sanjan | Live chat daemon (TetriSocial) |
| `chatctl` | Sanjan | Admin CLI for `chatd` |
| `marketd` | Zi Qi | Points marketplace daemon (TetriSocial) |
| `marketctl` | Zi Qi | Admin CLI for `marketd` |

## Libraries and boundaries

| Library | Owner | Rule |
|---------|-------|------|
| `libtetrissh` | Zi Qi | Secure session: nonce → cert → RSA-PSS → RSA-OAEP → AES-256 frames |
| `libhtttp` | Zi Qi | HTTTP parser + serialiser. Wire format is fixed — do not change method names/headers |
| `libtetrisbrain` | Both | **PURE LOGIC ONLY** — no I/O, no POSIX, no global state. See rule below |
| `libcoreipc` | Sanjan | Ring buffer, mq helpers, Unix socket wrappers |
| `libchatcore` | Sanjan | Room registry, token-bucket rate limiter, role management, event formatter |
| `libmarketcore` | Zi Qi | Points ledger, inventory, catalogue, SQLite persistence |

---

## Hard rules — never violate these

### 1. libtetrisbrain is pure

No I/O, no networking, no POSIX calls, no mutexes, no global mutable state. If you find yourself adding any of this to `libtetrisbrain/`, stop — move it to `tetrisd/ability.c`, `tetrisd/ticker.c`, or the calling daemon.

### 2. Never hold a mutex across a blocking syscall

Pattern is always: **lock → copy to local buffer → unlock → syscall**. Holding `room->mutex` across `send()` will freeze all players in a room under TCP backpressure.

### 3. All IPC from tetrisd → chatd/marketd uses MSG_DONTWAIT

Every `sendto()` from the game loop to a social daemon socket must pass `MSG_DONTWAIT`. If the socket is full or missing, log the drop and continue — never retry, never block.

### 4. The server is authoritative

Never update client-side game state without a round-trip to `tetrisd`. Every move/rotate/drop/ability goes through the full stack: `tetrisu → libhtttp → libtetrissh → TCP → tetrisd → libtetrisbrain → STATE back`.

### 5. common.c / common.h must not be modified

These are provided by course staff. All crypto primitives come from here. Read-only.

### 6. game_event.h is frozen after Week 4

Do not change field offsets. New `event_type_t` values may be appended. New payload union fields may be appended. Nothing else. Flag any change to this file explicitly.

### 7. Shared boundary files require both members' review

Any PR touching `game_event.h`, `include/loadout_ipc.h`, `include/htttp.h`, or `corestack/` must be explicitly approved by both Sanjan and Zi Qi before merge.

---

## IPC channels — quick reference

| From | To | Mechanism | Blocking? |
|------|----|-----------|-----------|
| `tetrisd` | `tetrislogd` | Unix `SOCK_DGRAM` | `MSG_DONTWAIT` — drop on full |
| `tetrisd` | `chatd` | Unix `SOCK_DGRAM` | `MSG_DONTWAIT` — drop on full |
| `tetrisd` | `marketd` | Unix `SOCK_DGRAM` | `MSG_DONTWAIT` — drop on full |
| `tetrisctl` | `tetrisd` | Unix `SOCK_STREAM` | Blocking request/response |
| `chatctl` | `chatd` | Unix `SOCK_STREAM` | Blocking request/response |
| `marketctl` | `marketd` | Unix `SOCK_STREAM` | Blocking request/response |
| `tetrisd` | `marketd` (loadout query) | Unix `SOCK_STREAM` | Blocking — at JOIN time only |
| Battle Royale garbage | room → room | POSIX mq (`O_NONBLOCK`) | Never blocks game loop |
| `tetrisu` → `chatd` | second TCP connection | `libtetrissh` + HTTTP | Independent of game socket |
| All client connections | `tetrisd` / `chatd` / `marketd` | TCP + `libtetrissh` + HTTTP | Full secure session required |

---

## Thread model — tetrisd

```
listener_thread       → spawns client_thread per TCP connection
client_thread[N]      → handshake, HTTTP dispatch, room_t mutations (one per player)
ticker_thread[M]      → gravity, line clear, garbage inject, STATE broadcast (one per room)
logshipper_thread     → drains ring_buffer → SOCK_DGRAM to tetrislogd
ctl_listener_thread   → Unix socket for tetrisctl (isolated from public TCP)
signal_thread         → sigwaitinfo() loop: SIGTERM, SIGHUP, SIGUSR1
```

Same pattern for `chatd`. `marketd` replaces `ticker_thread` with `event_consumer_thread` and `store_thread`.

**Lock order rule:** when acquiring two mutexes simultaneously, always take them in ascending pointer-address order. Document this order in the README.

---

## Commenting — short, useful, checkoff-ready

The live checkoff Q&A can ask about **any line of submitted code**. Comments should be short enough that teammates will read them, but specific enough to answer "why is this here?" quickly.

Use style already in `libtetrisbrain/board.c` and `libtetrisbrain/pieces.c`:

- Comment non-obvious rules, edge cases, data tables, IPC choices, locking choices, and failure behavior.
- Prefer 1-4 line comments near code they explain.
- Explain intent and systems reason, not syntax.
- Do not add banner comments to every small helper if code is obvious.
- Add bigger comments only for dense tables, protocols, thread loops, or lock/IPC code.

Good examples:

```c
return (cell_t){CELL_FILLED, 0}; // out of bounds = solid wall
```

```c
// shift everything up by `lines` rows (rows 0..lines-1 fall off the top)
```

```c
// A corrupted piece_t (e.g. from a malformed network message) could carry a
// type/rotation outside the SHAPES/KICK_TRANSITION tables; guard every entry
// point that indexes those tables by p->type/p->rotation.
```

```c
// Wall kicks: when a rotation's naive shape would overlap a wall, floor, or
// existing block, SRS retries with a small set of (dcol,drow) offsets applied
// to the *new* (post-rotation) shape, in order, until one fits.
```

For lock and IPC code, always state blocking reason:

```c
pthread_mutex_unlock(&room->mutex);
/* release before send(): TCP backpressure on one slow client must not stall
 * the whole room ticker. */
```

```c
if (sendto(fd, &ev, sizeof(ev), MSG_DONTWAIT, addr, addrlen) == -1) {
    /* DGRAM socket full/missing: social events are best-effort, so log/drop
     * instead of blocking the game loop. */
    stats->event_drops++;
}
```

### AI-assisted code comments

If an AI assistant writes a non-trivial function, add one short comment near function or tricky block:

```c
/* AI-assisted: validates network-derived piece fields before table lookup.
 * Failure returns BRAIN_BLOCKED so malformed input cannot index past SHAPES. */
```

---

## How to explain every change you make

After writing or modifying any code, always produce this summary. The checkoff treats every line equally — the team needs to be able to answer questions about AI-written code.

```
### Change: <title>

**Files changed:** list each file and function

**Why:** <systems-level reason — which requirement, which IPC contract, which rule from AGENTS.md>

**Line-by-line:**
  <paste the key lines>
  ^ <explain what each does and the systems reason behind it — not syntax, the WHY>

**Mutex/IPC:** <which lock is held, which channel is used, blocking mode>

**Failure mode:** <what happens when this goes wrong — what is dropped, what is returned>
```

**Always flag shared boundary changes:**

```
⚠️ SHARED BOUNDARY: modifies <file> — both members must review before merge.
Impact: <which other files are affected and how>
```

---

## Build and memory safety

```bash
make all              # build everything
make test             # all three test tiers under valgrind
make test-unit        # libtetrisbrain, libhtttp, libchatcore, libmarketcore
make test-integration # game_event.h pipeline across real sockets
make clean && make all # always verify clean build before checkoff
```

**Valgrind is mandatory on every PR:**

```bash
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./bin/<target>
```

A visible leak at the checkoff loses marks regardless of feature completeness.

**Compilation flags:** `-std=c11 -Wall -Wextra -Werror -pedantic` — a warning is a build failure.

---

## File layout (key paths)

```
src/tetrisd/
    ticker.c          ← room tick loop
    ability.c         ← ALL ability logic lives here (not in libtetrisbrain)
    event_pub.c       ← game_event.h publisher to chatd + marketd
    garbage.c         ← Battle Royale POSIX mq integration
    loadout.c         ← synchronous loadout query to marketd at JOIN time
src/libtetrisbrain/
    abilities.c       ← ability-aware BOARD TRANSFORMS ONLY (pure, no IPC)
include/
    game_event.h      ← FROZEN after Week 4
    loadout_ipc.h     ← FROZEN once both sides in review
    common_types.h
docs/
    architecture.md   ← full architecture, IPC, thread model detail
    checkoff-prep.md  ← Q&A question pool + how to answer each one
    requirements.md   ← all FR/NFR tables for tetriSH + TetriSocial
```

---

## Commit message format

```
[component] short description of intent

e.g.:
[tetrisd] add garbage injection to ticker_thread via POSIX mq
[libtetrisbrain] implement SRS rotation for all 7 tetrominoes
[chatd] wire event_consumer_thread to game_event.h SOCK_DGRAM socket
[marketd] add ledger replay on startup from append-only binary file
```

The git history is part of prize evaluation. No "fix stuff", no "update", no squashed final commits.
