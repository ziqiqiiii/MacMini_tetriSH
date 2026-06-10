# pieces.c — SRS rotation system

Date: 2026-06-10
Status: approved

## Scope

Implement `libtetrisbrain/pieces.c`: `piece_spawn`, `piece_is_valid`, `piece_move`,
`piece_rotate`, `piece_stamp`, plus the SRS shape and wall-kick tables, per
`tetrisbrain.h`. Also `tests/test_pieces.c` (same gcc-and-run pattern as
`test_board.c`).

Out of scope: gravity/lock-delay (`gravity.c`), line clear, scoring, abilities.

## Coordinate convention

Matches `board.c`: col increases right, row increases **down**, row 0 = top of
board, `board_get` returns `CELL_FILLED` for any out-of-bounds (col,row) — so
collision checks need no manual range guards.

The official SRS guideline tables are published in a y-up coordinate system.
All tables below are already converted to this project's row-down convention
via `drow = -dy_guideline`, `dcol = dx_guideline`.

## Representation

```c
typedef struct { int8_t dcol, drow; } cell_offset_t;
```

`SHAPES[piece_type][rotation][4]` — the 4 occupied cells of a piece, as offsets
from `piece_t.col/row`, for rotation states 0 (spawn), 1 (R, CW from spawn),
2 (180°), 3 (L, CCW from spawn). No bounding-box-size field is needed: only
the 4 occupied cells matter for collision/stamp, and the kick tables already
encode each piece's effective box size.

Derivation check: every JLSTZ table below was generated from its rotation-0
shape by repeatedly applying the 3x3 CW transform `(r,c) -> (c, 2-r)`, and the
I piece via the 4x4 transform `(r,c) -> (c, 3-r)`. All four states of each
piece are mutually consistent under this transform.

### SHAPES (dcol,drow per cell)

```
PIECE_I:
  0: (0,1) (1,1) (2,1) (3,1)
  R: (2,0) (2,1) (2,2) (2,3)
  2: (0,2) (1,2) (2,2) (3,2)
  L: (1,0) (1,1) (1,2) (1,3)

PIECE_O:
  0/R/2/L (identical): (0,0) (1,0) (0,1) (1,1)

PIECE_T:
  0: (1,0) (0,1) (1,1) (2,1)
  R: (1,0) (1,1) (2,1) (1,2)
  2: (0,1) (1,1) (2,1) (1,2)
  L: (1,0) (0,1) (1,1) (1,2)

PIECE_S:
  0: (1,0) (2,0) (0,1) (1,1)
  R: (1,0) (1,1) (2,1) (2,2)
  2: (1,1) (2,1) (0,2) (1,2)
  L: (0,0) (0,1) (1,1) (1,2)

PIECE_Z:
  0: (0,0) (1,0) (1,1) (2,1)
  R: (2,0) (1,1) (2,1) (1,2)
  2: (0,1) (1,1) (1,2) (2,2)
  L: (1,0) (0,1) (1,1) (0,2)

PIECE_J:
  0: (0,0) (0,1) (1,1) (2,1)
  R: (1,0) (2,0) (1,1) (1,2)
  2: (0,1) (1,1) (2,1) (2,2)
  L: (1,0) (1,1) (0,2) (1,2)

PIECE_L:
  0: (2,0) (0,1) (1,1) (2,1)
  R: (1,0) (1,1) (1,2) (2,2)
  2: (0,1) (1,1) (2,1) (0,2)
  L: (0,0) (1,0) (1,1) (1,2)
```

### Spawn anchors (rotation = 0)

All chosen so the 4 occupied cells land on absolute row 0 (or rows 0–1),
horizontally centered per guideline convention.

```
PIECE_I: col=3, row=-1   -> occupies row 0, cols 3-6
PIECE_O: col=4, row=0    -> occupies rows 0-1, cols 4-5
PIECE_T/S/Z/J/L: col=3, row=0 -> occupies rows 0-1ish, cols 3-5
```

(`row=-1` for I is fine: only the 4 occupied cells, all at absolute row 0,
are ever checked — the unused parts of the conceptual 4x4 box are never
queried.)

## Wall kicks

8 transitions, indexed 0-7:
`0: 0->R, 1: R->0, 2: R->2, 3: 2->R, 4: 2->L, 5: L->2, 6: L->0, 7: 0->L`

`KICK_TRANSITION[from_rotation][dir_idx]` -> transition index, where
`dir_idx = 0` for CW (`dir == +1`), `1` for CCW (`dir == -1`):

```
from 0: {0, 7}
from R: {2, 1}
from 2: {4, 3}
from L: {6, 5}
```

Each transition has 5 (dcol,drow) tests, tried in order; first one that makes
`piece_is_valid` true wins.

### JLSTZ_KICKS[8][5]

```
0 (0->R): (0,0) (-1,0) (-1,-1) (0,2) (-1,2)
1 (R->0): (0,0) (1,0)  (1,1)   (0,-2) (1,-2)
2 (R->2): (0,0) (1,0)  (1,1)   (0,-2) (1,-2)
3 (2->R): (0,0) (-1,0) (-1,-1) (0,2) (-1,2)
4 (2->L): (0,0) (1,0)  (1,-1)  (0,2) (1,2)
5 (L->2): (0,0) (-1,0) (-1,1)  (0,-2) (-1,-2)
6 (L->0): (0,0) (-1,0) (-1,1)  (0,-2) (-1,-2)
7 (0->L): (0,0) (1,0)  (1,-1)  (0,2) (1,2)
```

### I_KICKS[8][5]

```
0 (0->R): (0,0) (-2,0) (1,0)  (-2,1)  (1,-2)
1 (R->0): (0,0) (2,0)  (-1,0) (2,-1)  (-1,2)
2 (R->2): (0,0) (-1,0) (2,0)  (-1,-2) (2,1)
3 (2->R): (0,0) (1,0)  (-2,0) (1,2)   (-2,-1)
4 (2->L): (0,0) (2,0)  (-1,0) (2,-1)  (-1,2)
5 (L->2): (0,0) (-2,0) (1,0)  (-2,1)  (1,-2)
6 (L->0): (0,0) (1,0)  (-2,0) (1,2)   (-2,-1)
7 (0->L): (0,0) (-1,0) (2,0)  (-1,-2) (2,1)
```

Sanity check: for every transition pair (A->B, B->A), table[B->A] ==
elementwise-negate(table[A->B]) — confirmed for both tables above.

### O piece

Shape is identical in all 4 rotation states, so `piece_rotate` for
`PIECE_O` just sets `p->rotation = to` and returns `BRAIN_OK` unconditionally
(no kick table, no validity re-check needed — if the piece was valid before,
it's valid after, since occupied cells don't change).

## Function behaviour

- `piece_spawn(type)` — returns `{type, col, row, rotation:0}` from the spawn
  anchor table. Caller (gravity/server) is responsible for checking
  `piece_is_valid` to detect game-over (board full at spawn).

- `piece_is_valid(b, p)` — for each of the 4 cells in
  `SHAPES[p->type][p->rotation]`, compute absolute `(col,row)` and require
  `board_get(b, col, row).type == CELL_EMPTY`. Out-of-bounds returns
  `CELL_FILLED` from `board_get`, so walls/floor/ceiling are rejected for
  free.

- `piece_move(b, p, dcol, drow)` — build a candidate piece with
  `col += dcol, row += drow`; if `piece_is_valid`, commit it and return
  `BRAIN_OK`; else leave `*p` unchanged and return `BRAIN_BLOCKED`.

- `piece_rotate(b, p, dir)` — `dir` is `+1` (CW) or `-1` (CCW).
  `to = (p->rotation + (dir == 1 ? 1 : 3)) % 4`.
  - `PIECE_O`: set rotation to `to`, return `BRAIN_OK`.
  - Else: pick `I_KICKS` or `JLSTZ_KICKS` by type, look up the transition row
    via `KICK_TRANSITION[p->rotation][dir == 1 ? 0 : 1]`, try each of the 5
    `(dcol,drow)` offsets with `rotation = to`; first valid candidate is
    committed (`BRAIN_OK`). If none valid, `*p` unchanged, `BRAIN_BLOCKED`.

- `piece_stamp(b, p)` — for each of the 4 cells in
  `SHAPES[p->type][p->rotation]`, `board_set(b, col, row, {CELL_FILLED,
  (uint8_t)p->type})`.

## Tests (`tests/test_pieces.c`)

Same pattern as `test_board.c` (plain `assert` + `printf("PASS ...")`,
`main()` calling each test):

1. `test_spawn_all_types_valid` — every `piece_type_t` spawns to a piece that
   is `piece_is_valid` on an empty board, with exactly 4 distinct occupied
   cells, all rows >= 0.
2. `test_move_blocked_by_walls_and_floor` — I piece at spawn: move left to the
   wall (`BRAIN_BLOCKED` past col 0), move right to the wall, drop to the
   floor (`BRAIN_BLOCKED` past the bottom row); piece position unchanged on a
   blocked move.
3. `test_rotate_basic_updates_rotation` — T piece spawn, rotate CW, check
   `rotation == 1` and `piece_is_valid`.
4. `test_jlstz_wall_kick` — push a T (or J/L/S/Z) piece flush against the left
   wall, rotate such that the naive (no-kick) rotation would be invalid;
   confirm `piece_rotate` returns `BRAIN_OK` and the piece shifted right per
   `JLSTZ_KICKS`.
5. `test_i_piece_wall_kick` — same idea for the I piece against a wall, using
   `I_KICKS`.
6. `test_rotate_blocked_returns_blocked` — box a piece in on all sides with
   `CELL_FILLED` cells so no kick test succeeds; `piece_rotate` returns
   `BRAIN_BLOCKED` and `*p` is unchanged (compare full struct).
7. `test_o_piece_rotate_is_noop` — O piece rotate CW/CCW: `rotation` updates,
   `col`/`row` unchanged, `BRAIN_OK`, still valid.
8. `test_stamp_writes_cells` — spawn + stamp a piece, verify the 4 occupied
   cells are `CELL_FILLED` with `color == (uint8_t)type`, and everything else
   stays `CELL_EMPTY`.

## Style

Minimal comments, matching `board.c` — no file/function header-comment
blocks. Inline comments only where the SRS coordinate-flip (guideline y-up ->
this project's row-down) or the kick-transition indexing would otherwise be
non-obvious.

## Build

```bash
gcc -I libtetrisbrain/ tests/test_pieces.c libtetrisbrain/pieces.c libtetrisbrain/board.c -o tests/test_pieces
./tests/test_pieces
```
