# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

tetriSH is a terminal-based Battle Royale Tetris system in C, built for the CoreStack Challenge (50.003 × 50.005) at SUTD. The project is in early development — currently only `libtetrisbrain` is partially implemented.

## Build & Test

No Makefile exists yet. Compile manually:

```bash
# Build and run tests for libtetrisbrain/board.c
gcc -I libtetrisbrain/ tests/test_board.c libtetrisbrain/board.c -o tests/test_board
./tests/test_board

# General pattern for compiling a test against multiple source files:
gcc -I libtetrisbrain/ tests/<test>.c libtetrisbrain/<module>.c [...] -o tests/<test>
```

Final build will use `make` with targets per binary (`tetrish`, `tetrisd`, `tetrisu`, `libtetrissh`, etc.) and OpenSSL (`-lssl -lcrypto`).

## Architecture

Three layers above the kernel:

```
HTTTP (application protocol)
Secure session (cert auth, RSA-OAEP key exchange, AES-256 frames)
TCP (POSIX sockets)
```

**Binaries:**

- `tetrish` — interactive shell (REPL, builtins, `.tetrishrc`, daemon lifecycle via `dspawn`/`dcheck`)
- `tetrisd` — concurrent game server; server-authoritative; manages rooms, game logic, clients
- `tetrislogd` — separate logger process; receives log records over IPC; survives `tetrisd` restarts
- `tetrisctl` — admin CLI; talks to `tetrisd` over a local-only control-plane IPC channel (not the public TCP port)
- `tetrisu` — terminal client; renders board, handles input + network simultaneously

**Libraries (statically linked):**

- `libtetrisbrain/` — pure game logic (no I/O, no networking); linked into `tetrisd` and optionally `tetrisu` for client-side prediction
- `libtetrissh/` — secure session handshake and encrypted framing; linked into both `tetrisd` and `tetrisu`
- `libhtttp/` — HTTTP parser and serialiser; linked into both `tetrisd` and `tetrisu`

## libtetrisbrain API (tetrisbrain.h)

The header declares all modules. Implement each in its own `.c` file under `libtetrisbrain/`:

| File | Responsibility |
|---|---|
| `board.c` | `board_init`, `board_get/set`, `board_in_bounds`, `board_inject_garbage`, `board_copy` — **done** |
| `pieces.c` | `piece_spawn`, `piece_is_valid`, `piece_move`, `piece_rotate`, `piece_stamp` |
| `gravity.c` | `gravity_tick`, `piece_soft_drop`, `piece_hard_drop` |
| `lineclear.c` | `board_clear_lines` (returns lines cleared 0–4) |
| `scoring.c` | `score_on_clear`, `level_from_lines`, `gravity_interval_ms` |
| `abilities.c` | Board manipulation for Battle Royale abilities |

`brain_result_t` return codes: `BRAIN_OK`, `BRAIN_BLOCKED`, `BRAIN_LOCKED`, `BRAIN_GAME_OVER`, `BRAIN_CLEARED`.

Out-of-bounds reads via `board_get` return `CELL_FILLED` (solid wall), so collision checks work uniformly without range guards in every caller.

## Key Design Constraints

- `common.c`/`common.h` (PA2 crypto primitives) are **not modified** — all crypto goes through them.
- No TLS, no `SSL_*` API — handshake is implemented manually in `libtetrissh`.
- `libtetrisbrain` has **no I/O, no side effects** — pure logic only.
- No hard-coded paths anywhere; all paths come from `.tetrishrc`.
- `tetrislogd` and `tetrisd` communicate over IPC with a non-blocking ring buffer — log records are dropped (not blocked) under pressure.
- No mutex held across a blocking syscall. Lock acquisition order must be documented and strictly followed to prevent deadlocks.
- Frame size cap: 64 KiB. HTTTP messages exceeding this → `413 Payload Too Large`.

## HTTTP Protocol

Custom HTTP-like protocol. Only `STATE` is server-originated (pushed); all other methods are client-initiated request/response. The `Player-Id` header is required on every authenticated request. See README.md for the full grammar and method table.
