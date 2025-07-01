#include "record.h"

static uint64_t board[16];
static int board_num = 0;
static int turn[16];

void record(int n)
{
    board[board_num] |= (n << (turn[board_num] << 2));
    printk("meow %d", n);
    turn[board_num]++;
}

void record_init(void)
{
    if (!(board_num ^ 15)) {
        for (int i = 0; i < 15; i++) {
            board[i] = board[i + 1];
            turn[i] = turn[i + 1];
        }

        board[15] = 0;
        turn[15] = 0;

        return;
    }

    board_num++;

    turn[board_num] = 0;
    board[board_num] = 0;
    return;
}

void record_init_all(void)
{
    board_num = 0;
    board[board_num] = 0;
    turn[0] = 0;
}

int *record_get_move(void)
{
    return turn;
}

uint64_t *record_get_board(void)
{
    return board;
}
