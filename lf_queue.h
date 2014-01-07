#ifndef LF_QUEUE_H
#define LF_QUEUE_H

#include <stdlib.h>
#include <stdint.h>

typedef struct queue* queue;

queue new_queue();
int queue_deq(queue queue, void** result);
void queue_enq(queue queue, void* data);

#endif
