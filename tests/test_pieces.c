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

int main(void) {
  test_spawn_all_types_valid();
  test_stamp_writes_cells();
  test_move_blocked_by_walls_and_floor();
  return 0;
}