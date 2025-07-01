#include <linux/spinlock.h>
#include <linux/types.h>

void record(int n);

void record_init(void);

void record_init_all(void);

int *record_get_move(void);

uint64_t *record_get_board(void);
