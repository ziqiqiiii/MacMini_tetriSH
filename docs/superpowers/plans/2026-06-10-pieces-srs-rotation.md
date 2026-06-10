# pieces.c SRS Rotation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `libtetrisbrain/pieces.c` (piece spawn, validity, move, SRS rotation with wall kicks, stamping) plus `tests/test_pieces.c`, per `docs/superpowers/specs/2026-06-10-pieces-srs-rotation-design.md`.

**Architecture:** All shape/kick data is static `cell_offset_t {int8_t dcol, drow}` tables indexed by `piece_type_t` and rotation state (0/R/2/L). `piece_is_valid`/`piece_stamp` iterate the 4 offsets for `(p->type, p->rotation)`. `piece_rotate` looks up a wall-kick table by piece type and rotation transition, tries each of 5 offsets in order, commits the first valid one.

**Tech Stack:** C11, no external deps. Build/test via the manual gcc pattern from `CLAUDE.md`:
```bash
gcc -I libtetrisbrain/ tests/test_pieces.c libtetrisbrain/pieces.c libtetrisbrain/board.c -o tests/test_pieces
./tests/test_pieces
```

**Reference:** All shape tables, spawn anchors, and kick tables are fully specified in `docs/superpowers/specs/2026-06-10-pieces-srs-rotation-design.md` — this plan inlines the exact same data so each task is self-contained.

---

### Task 1: Spawn, validity, stamp + shape tables

**Files:**
- Create: `libtetrisbrain/pieces.c`
- Create: `tests/test_pieces.c`

- [ ] **Step 1: Write the failing tests**

Create `tests/test_pieces.c`:

```c
// tests/test_pieces.c
#include "tetrisbrain.h"
#include <assert.h>
#include <stdio.h>

void test_spawn_all_types_valid(void) {
  piece_type_t types[] = {PIECE_I, PIECE_O, PIECE_T, PIECE_S,
                           PIECE_Z, PIECE_J, PIECE_L};
  for (int i = 0; i < 7; i++) {
    board_t b;
    board_init(&b);
    piece_t p = piece_spawn(types[i]);
    assert(p.type == types[i]);
    assert(p.rotation == 0);
    assert(piece_is_valid(&b, &p));

    piece_stamp(&b, &p);
    int filled = 0;
    for (int r = 0; r < BOARD_HEIGHT; r++)
      for (int c = 0; c < BOARD_WIDTH; c++)
        if (board_get(&b, c, r).type == CELL_FILLED) filled++;
    assert(filled == 4);
  }
  printf("PASS test_spawn_all_types_valid\n");
}

void test_stamp_writes_cells(void) {
  board_t b;
  board_init(&b);
  // T spawn: col=3,row=0, shape0 offsets (1,0)(0,1)(1,1)(2,1)
  // -> abs cells (4,0) (3,1) (4,1) (5,1)
  piece_t p = piece_spawn(PIECE_T);
  piece_stamp(&b, &p);

  assert(board_get(&b, 4, 0).type == CELL_FILLED);
  assert(board_get(&b, 4, 0).color == (uint8_t)PIECE_T);
  assert(board_get(&b, 3, 1).type == CELL_FILLED);
  assert(board_get(&b, 4, 1).type == CELL_FILLED);
  assert(board_get(&b, 5, 1).type == CELL_FILLED);

  assert(board_get(&b, 0, 0).type == CELL_EMPTY);
  assert(board_get(&b, 3, 0).type == CELL_EMPTY); // not part of T spawn shape

  printf("PASS test_stamp_writes_cells\n");
}

int main(void) {
  test_spawn_all_types_valid();
  test_stamp_writes_cells();
  return 0;
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `gcc -I libtetrisbrain/ tests/test_pieces.c libtetrisbrain/pieces.c libtetrisbrain/board.c -o tests/test_pieces`
Expected: FAIL — `libtetrisbrain/pieces.c: No such file or directory` (file doesn't exist yet).

- [ ] **Step 3: Write `libtetrisbrain/pieces.c`**

```c
#include "tetrisbrain.h"

typedef struct {
  int8_t dcol;
  int8_t drow;
} cell_offset_t;

// SHAPES[type][rotation][4 cells], offsets from piece_t.col/row.
// Coordinates are row-down (matches board.c), converted from the SRS
// guideline's y-up tables via drow = -dy.
static const cell_offset_t SHAPES[7][4][4] = {
  [PIECE_I] = {
    {{0,1},{1,1},{2,1},{3,1}},
    {{2,0},{2,1},{2,2},{2,3}},
    {{0,2},{1,2},{2,2},{3,2}},
    {{1,0},{1,1},{1,2},{1,3}},
  },
  [PIECE_O] = {
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
    {{0,0},{1,0},{0,1},{1,1}},
  },
  [PIECE_T] = {
    {{1,0},{0,1},{1,1},{2,1}},
    {{1,0},{1,1},{2,1},{1,2}},
    {{0,1},{1,1},{2,1},{1,2}},
    {{1,0},{0,1},{1,1},{1,2}},
  },
  [PIECE_S] = {
    {{1,0},{2,0},{0,1},{1,1}},
    {{1,0},{1,1},{2,1},{2,2}},
    {{1,1},{2,1},{0,2},{1,2}},
    {{0,0},{0,1},{1,1},{1,2}},
  },
  [PIECE_Z] = {
    {{0,0},{1,0},{1,1},{2,1}},
    {{2,0},{1,1},{2,1},{1,2}},
    {{0,1},{1,1},{1,2},{2,2}},
    {{1,0},{0,1},{1,1},{0,2}},
  },
  [PIECE_J] = {
    {{0,0},{0,1},{1,1},{2,1}},
    {{1,0},{2,0},{1,1},{1,2}},
    {{0,1},{1,1},{2,1},{2,2}},
    {{1,0},{1,1},{0,2},{1,2}},
  },
  [PIECE_L] = {
    {{2,0},{0,1},{1,1},{2,1}},
    {{1,0},{1,1},{1,2},{2,2}},
    {{0,1},{1,1},{2,1},{0,2}},
    {{0,0},{1,0},{1,1},{1,2}},
  },
};

// Spawn anchors (rotation 0): centered, occupied cells land on row 0 (I,
// JLSTZ) or rows 0-1 (O). I piece uses row=-1 because its spawn shape's
// occupied cells are at local row 1, not row 0.
static const piece_t SPAWN[7] = {
  [PIECE_I] = {PIECE_I, 3, -1, 0},
  [PIECE_O] = {PIECE_O, 4, 0, 0},
  [PIECE_T] = {PIECE_T, 3, 0, 0},
  [PIECE_S] = {PIECE_S, 3, 0, 0},
  [PIECE_Z] = {PIECE_Z, 3, 0, 0},
  [PIECE_J] = {PIECE_J, 3, 0, 0},
  [PIECE_L] = {PIECE_L, 3, 0, 0},
};

piece_t piece_spawn(piece_type_t type) { return SPAWN[type]; }

bool piece_is_valid(const board_t *b, const piece_t *p) {
  for (int i = 0; i < 4; i++) {
    int col = p->col + SHAPES[p->type][p->rotation][i].dcol;
    int row = p->row + SHAPES[p->type][p->rotation][i].drow;
    if (board_get(b, col, row).type != CELL_EMPTY) return false;
  }
  return true;
}

void piece_stamp(board_t *b, const piece_t *p) {
  for (int i = 0; i < 4; i++) {
    int col = p->col + SHAPES[p->type][p->rotation][i].dcol;
    int row = p->row + SHAPES[p->type][p->rotation][i].drow;
    board_set(b, col, row, (cell_t){CELL_FILLED, (uint8_t)p->type});
  }
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `gcc -I libtetrisbrain/ tests/test_pieces.c libtetrisbrain/pieces.c libtetrisbrain/board.c -o tests/test_pieces && ./tests/test_pieces`
Expected:
```
PASS test_spawn_all_types_valid
PASS test_stamp_writes_cells
```

- [ ] **Step 5: Commit**

```bash
git add libtetrisbrain/pieces.c tests/test_pieces.c
git commit -m "[libtetrisbrain] add piece_spawn/piece_is_valid/piece_stamp with SRS shape tables"
```

---

### Task 2: piece_move

**Files:**
- Modify: `libtetrisbrain/pieces.c` (append)
- Modify: `tests/test_pieces.c`

- [ ] **Step 1: Write the failing test**

Add to `tests/test_pieces.c`, before `main`:

```c
void test_move_blocked_by_walls_and_floor(void) {
  board_t b;
  board_init(&b);
  piece_t p = piece_spawn(PIECE_I); // col=3,row=-1, occupies cols 3-6, row 0

  for (int i = 0; i < 3; i++)
    assert(piece_move(&b, &p, -1, 0) == BRAIN_OK);
  assert(p.col == 0);
  assert(piece_move(&b, &p, -1, 0) == BRAIN_BLOCKED);
  assert(p.col == 0); // unchanged on block

  for (int i = 0; i < 6; i++)
    assert(piece_move(&b, &p, 1, 0) == BRAIN_OK);
  assert(p.col == 6);
  assert(piece_move(&b, &p, 1, 0) == BRAIN_BLOCKED);
  assert(p.col == 6);

  for (int i = 0; i < 19; i++)
    assert(piece_move(&b, &p, 0, 1) == BRAIN_OK);
  assert(p.row == 18);
  assert(piece_move(&b, &p, 0, 1) == BRAIN_BLOCKED);
  assert(p.row == 18);

  printf("PASS test_move_blocked_by_walls_and_floor\n");
}
```

And add the call in `main`, after `test_stamp_writes_cells();`:

```c
  test_move_blocked_by_walls_and_floor();
```

- [ ] **Step 2: Run to verify it fails**

Run: `gcc -I libtetrisbrain/ tests/test_pieces.c libtetrisbrain/pieces.c libtetrisbrain/board.c -o tests/test_pieces`
Expected: FAIL — linker error `undefined reference to 'piece_move'`.

- [ ] **Step 3: Implement `piece_move`**

Append to `libtetrisbrain/pieces.c`:

```c
brain_result_t piece_move(const board_t *b, piece_t *p, int dcol, int drow) {
  piece_t moved = *p;
  moved.col += dcol;
  moved.row += drow;
  if (!piece_is_valid(b, &moved)) return BRAIN_BLOCKED;
  *p = moved;
  return BRAIN_OK;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `gcc -I libtetrisbrain/ tests/test_pieces.c libtetrisbrain/pieces.c libtetrisbrain/board.c -o tests/test_pieces && ./tests/test_pieces`
Expected:
```
PASS test_spawn_all_types_valid
PASS test_stamp_writes_cells
PASS test_move_blocked_by_walls_and_floor
```

- [ ] **Step 5: Commit**

```bash
git add libtetrisbrain/pieces.c tests/test_pieces.c
git commit -m "[libtetrisbrain] add piece_move with wall/floor collision"
```

---

### Task 3: piece_rotate (SRS wall kicks)

**Files:**
- Modify: `libtetrisbrain/pieces.c` (append)
- Modify: `tests/test_pieces.c`

- [ ] **Step 1: Write the failing tests**

Add to `tests/test_pieces.c`, before `main`:

```c
void test_rotate_basic_updates_rotation(void) {
  board_t b;
  board_init(&b);
  piece_t p = piece_spawn(PIECE_T); // col=3,row=0,rotation=0

  assert(piece_rotate(&b, &p, 1) == BRAIN_OK); // 0->R, no kick needed
  assert(p.rotation == 1);
  assert(p.col == 3 && p.row == 0);
  assert(piece_is_valid(&b, &p));

  printf("PASS test_rotate_basic_updates_rotation\n");
}

void test_jlstz_wall_kick(void) {
  board_t b;
  board_init(&b);
  // T piece, R orientation, flush against the left wall.
  piece_t p = {PIECE_T, -1, 0, 1};
  assert(piece_is_valid(&b, &p));

  // R->2: naive (0,0) kick puts a cell at col -1 (invalid).
  // JLSTZ_KICKS[R->2] test2 = (+1,0) shifts the piece right by 1.
  assert(piece_rotate(&b, &p, 1) == BRAIN_OK);
  assert(p.rotation == 2);
  assert(p.col == 0 && p.row == 0);
  assert(piece_is_valid(&b, &p));

  printf("PASS test_jlstz_wall_kick\n");
}

void test_i_piece_wall_kick(void) {
  board_t b;
  board_init(&b);
  // I piece, L orientation (single column at col+1), flush against left wall.
  piece_t p = {PIECE_I, -1, 0, 3};
  assert(piece_is_valid(&b, &p));

  // L->0: naive (0,0) kick puts a cell at col -1 (invalid).
  // I_KICKS[L->0] test2 = (+1,0) shifts the piece right by 1.
  assert(piece_rotate(&b, &p, 1) == BRAIN_OK);
  assert(p.rotation == 0);
  assert(p.col == 0 && p.row == 0);
  assert(piece_is_valid(&b, &p));

  printf("PASS test_i_piece_wall_kick\n");
}

void test_rotate_blocked_returns_blocked(void) {
  board_t b;
  for (int r = 0; r < BOARD_HEIGHT; r++)
    for (int c = 0; c < BOARD_WIDTH; c++)
      board_set(&b, c, r, (cell_t){CELL_FILLED, 0});

  // Carve out exactly the T spawn shape's 4 cells: (4,0)(3,1)(4,1)(5,1).
  board_set(&b, 4, 0, (cell_t){CELL_EMPTY, 0});
  board_set(&b, 3, 1, (cell_t){CELL_EMPTY, 0});
  board_set(&b, 4, 1, (cell_t){CELL_EMPTY, 0});
  board_set(&b, 5, 1, (cell_t){CELL_EMPTY, 0});

  piece_t p = piece_spawn(PIECE_T); // col=3,row=0,rotation=0
  assert(piece_is_valid(&b, &p));

  piece_t before = p;
  assert(piece_rotate(&b, &p, 1) == BRAIN_BLOCKED);
  assert(p.type == before.type && p.col == before.col &&
         p.row == before.row && p.rotation == before.rotation);

  printf("PASS test_rotate_blocked_returns_blocked\n");
}

void test_o_piece_rotate_is_noop(void) {
  board_t b;
  board_init(&b);
  piece_t p = piece_spawn(PIECE_O); // col=4,row=0,rotation=0

  assert(piece_rotate(&b, &p, 1) == BRAIN_OK);
  assert(p.rotation == 1);
  assert(p.col == 4 && p.row == 0);
  assert(piece_is_valid(&b, &p));

  assert(piece_rotate(&b, &p, -1) == BRAIN_OK);
  assert(p.rotation == 0);
  assert(p.col == 4 && p.row == 0);

  printf("PASS test_o_piece_rotate_is_noop\n");
}
```

And add the calls in `main`, after `test_move_blocked_by_walls_and_floor();`:

```c
  test_rotate_basic_updates_rotation();
  test_jlstz_wall_kick();
  test_i_piece_wall_kick();
  test_rotate_blocked_returns_blocked();
  test_o_piece_rotate_is_noop();
```

- [ ] **Step 2: Run to verify it fails**

Run: `gcc -I libtetrisbrain/ tests/test_pieces.c libtetrisbrain/pieces.c libtetrisbrain/board.c -o tests/test_pieces`
Expected: FAIL — linker error `undefined reference to 'piece_rotate'`.

- [ ] **Step 3: Implement `piece_rotate` and kick tables**

Append to `libtetrisbrain/pieces.c`:

```c
// Transition indices: 0=0->R 1=R->0 2=R->2 3=2->R 4=2->L 5=L->2 6=L->0 7=0->L
// KICK_TRANSITION[from_rotation][dir_idx], dir_idx: 0=CW(+1), 1=CCW(-1).
static const int8_t KICK_TRANSITION[4][2] = {
  {0, 7}, // from 0
  {2, 1}, // from R
  {4, 3}, // from 2
  {6, 5}, // from L
};

static const cell_offset_t JLSTZ_KICKS[8][5] = {
  {{0,0},{-1,0},{-1,-1},{0,2},{-1,2}},   // 0->R
  {{0,0},{1,0},{1,1},{0,-2},{1,-2}},     // R->0
  {{0,0},{1,0},{1,1},{0,-2},{1,-2}},     // R->2
  {{0,0},{-1,0},{-1,-1},{0,2},{-1,2}},   // 2->R
  {{0,0},{1,0},{1,-1},{0,2},{1,2}},      // 2->L
  {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}},  // L->2
  {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}},  // L->0
  {{0,0},{1,0},{1,-1},{0,2},{1,2}},      // 0->L
};

static const cell_offset_t I_KICKS[8][5] = {
  {{0,0},{-2,0},{1,0},{-2,1},{1,-2}},    // 0->R
  {{0,0},{2,0},{-1,0},{2,-1},{-1,2}},    // R->0
  {{0,0},{-1,0},{2,0},{-1,-2},{2,1}},    // R->2
  {{0,0},{1,0},{-2,0},{1,2},{-2,-1}},    // 2->R
  {{0,0},{2,0},{-1,0},{2,-1},{-1,2}},    // 2->L
  {{0,0},{-2,0},{1,0},{-2,1},{1,-2}},    // L->2
  {{0,0},{1,0},{-2,0},{1,2},{-2,-1}},    // L->0
  {{0,0},{-1,0},{2,0},{-1,-2},{2,1}},    // 0->L
};

brain_result_t piece_rotate(const board_t *b, piece_t *p, int dir) {
  int to = (p->rotation + (dir == 1 ? 1 : 3)) % 4;

  // O piece's shape is identical in every rotation state: if the current
  // position is valid, it stays valid after relabeling the rotation.
  if (p->type == PIECE_O) {
    p->rotation = to;
    return BRAIN_OK;
  }

  int transition = KICK_TRANSITION[p->rotation][dir == 1 ? 0 : 1];
  const cell_offset_t *kicks =
      (p->type == PIECE_I) ? I_KICKS[transition] : JLSTZ_KICKS[transition];

  for (int i = 0; i < 5; i++) {
    piece_t cand = *p;
    cand.rotation = to;
    cand.col += kicks[i].dcol;
    cand.row += kicks[i].drow;
    if (piece_is_valid(b, &cand)) {
      *p = cand;
      return BRAIN_OK;
    }
  }
  return BRAIN_BLOCKED;
}
```

- [ ] **Step 4: Run to verify all tests pass**

Run: `gcc -I libtetrisbrain/ tests/test_pieces.c libtetrisbrain/pieces.c libtetrisbrain/board.c -o tests/test_pieces && ./tests/test_pieces`
Expected:
```
PASS test_spawn_all_types_valid
PASS test_stamp_writes_cells
PASS test_move_blocked_by_walls_and_floor
PASS test_rotate_basic_updates_rotation
PASS test_jlstz_wall_kick
PASS test_i_piece_wall_kick
PASS test_rotate_blocked_returns_blocked
PASS test_o_piece_rotate_is_noop
```

- [ ] **Step 5: Commit**

```bash
git add libtetrisbrain/pieces.c tests/test_pieces.c
git commit -m "[libtetrisbrain] add piece_rotate with SRS wall kicks"
```

---

## Spec coverage check

- Shapes/spawn/kick tables, `piece_spawn`, `piece_is_valid`, `piece_stamp` -> Task 1
- `piece_move` (wall/floor blocking) -> Task 2
- `piece_rotate` (basic, JLSTZ kick, I kick, blocked, O no-op) -> Task 3
- `tests/test_pieces.c`, gcc build pattern -> all tasks
- Minimal/board.c-style comments -> followed throughout (only the coordinate-flip and transition-indexing notes are commented)
