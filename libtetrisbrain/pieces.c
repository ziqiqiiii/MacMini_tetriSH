#include "tetrisbrain.h"

typedef struct {
  int8_t dcol;
  int8_t drow;
} cell_offset_t;

// SHAPES[type][rotation][4 cells], offsets from piece_t.col/row.
// Coordinates are row-down (matches board.c), converted from the SRS
// guideline's y-up tables via drow = -dy.
static const cell_offset_t SHAPES[7][4][4] = {
    [PIECE_I] =
        {
            {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
            {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
            {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
            {{1, 0}, {1, 1}, {1, 2}, {1, 3}},
        },
    [PIECE_O] =
        {
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
            {{0, 0}, {1, 0}, {0, 1}, {1, 1}},
        },
    [PIECE_T] =
        {
            {{1, 0}, {0, 1}, {1, 1}, {2, 1}},
            {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
            {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
            {{1, 0}, {0, 1}, {1, 1}, {1, 2}},
        },
    [PIECE_S] =
        {
            {{1, 0}, {2, 0}, {0, 1}, {1, 1}},
            {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
            {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
            {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
        },
    [PIECE_Z] =
        {
            {{0, 0}, {1, 0}, {1, 1}, {2, 1}},
            {{2, 0}, {1, 1}, {2, 1}, {1, 2}},
            {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
            {{1, 0}, {0, 1}, {1, 1}, {0, 2}},
        },
    [PIECE_J] =
        {
            {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
            {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
            {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
            {{1, 0}, {1, 1}, {0, 2}, {1, 2}},
        },
    [PIECE_L] =
        {
            {{2, 0}, {0, 1}, {1, 1}, {2, 1}},
            {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
            {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
            {{0, 0}, {1, 0}, {1, 1}, {1, 2}},
        },
};

// Spawn anchors (rotation 0): centered, occupied cells land on row 0 (I,
// JLSTZ) or rows 0-1 (O). I piece uses row=-1 because its spawn shape's
// occupied cells are at local row 1, not row 0.
static const piece_t SPAWN[7] = {
    [PIECE_I] = {PIECE_I, 3, -1, 0}, [PIECE_O] = {PIECE_O, 4, 0, 0},
    [PIECE_T] = {PIECE_T, 3, 0, 0},  [PIECE_S] = {PIECE_S, 3, 0, 0},
    [PIECE_Z] = {PIECE_Z, 3, 0, 0},  [PIECE_J] = {PIECE_J, 3, 0, 0},
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

brain_result_t piece_move(const board_t *b, piece_t *p, int dcol, int drow) {
  piece_t moved = *p;
  moved.col += dcol;
  moved.row += drow;
  if (!piece_is_valid(b, &moved)) return BRAIN_BLOCKED;
  *p = moved;
  return BRAIN_OK;
}

void piece_stamp(board_t *b, const piece_t *p) {
  for (int i = 0; i < 4; i++) {
    int col = p->col + SHAPES[p->type][p->rotation][i].dcol;
    int row = p->row + SHAPES[p->type][p->rotation][i].drow;
    board_set(b, col, row, (cell_t){CELL_FILLED, (uint8_t)p->type});
  }
}
