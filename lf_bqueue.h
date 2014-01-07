#ifndef LF_BQUEUE_H
#define LF_BQUEUE_H

#include <stdlib.h>
#include <stdint.h>
#include <time.h>

typedef struct bqueue* bqueue;

bqueue new_bqueue(size_t max);
int bqueue_deq(bqueue queue, void** result);
int bqueue_enq(bqueue queue, void* data);
void bqueue_enq_repeat(bqueue queue, void* data);

#endif
