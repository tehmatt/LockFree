/*
 * A lock-free bounded queue implmentation based off of the method outlined by
 * Tsigas and Zhang [0] and relying on struct alignment to sizeof(void*) and
 * hardware support for CAS.
 *
 * Implemented as an array with a maximum size for performance reasons.  Note
 * that this algorithm does not solve the ABA problem, instead making it
 * arbitrarily unlikely to occur as the maximum size is made larger.  As such,
 * use a reasonable large size (>512) if the ABA problem could be an issue.
 *
 * If the queue is full, an enqueue operation will fail.  For an enqueue
 * operation that will repeat until it succeeds, using bqueue_enq_repeat, which
 * is implemented using a bounded exponential backoff strategy.
 *
 * [0] - A Simple, Fast and Scalable Non-Blocking Concurrent FIFO Queue for
 *       Shared Memory Multiprocessor Systems - Tsigas and Zhang
 */

#include "lf_bqueue.h"
#include "cas.h"

// Marker flags for the state
#define VALID(x) ((x) & 0x2)
#define NULLTYPE(x) ((x) & 0x1)
#define NULL_A 0x0
#define NULL_B 0x1
#define VALID_A 0x2
#define VALID_B 0x3

typedef struct option {
    void* data;
    uintptr_t state;
} option;

struct bqueue {
    size_t max;
    size_t head;
    size_t tail;
    option* nodes;
};

// Allocate space for a bounded queue with a maximum of `max` elements.
bqueue new_bqueue(size_t max) {
    bqueue new = malloc(sizeof(struct bqueue));
    new->nodes = malloc((max + 1) * sizeof(option));
    new->head = 0;
    new->tail = 1;
    new->max = max;

    // Mark the data as empty for now
    for (size_t i = 1; i <= max; i++) {
        new->nodes[i].state = NULL_B;
    }

    new->nodes[0].state = NULL_A;
    return new;
}

// Enqueue the given data, returning 0 if unsuccessful.
int bqueue_enq(bqueue q, void* data) {
    while (1) {
        size_t tail = q->tail;
        option cell = q->nodes[tail];
        size_t new_tail = (tail + 1) % q->max;

        // Find the actual tail
        while (VALID(cell.state)) {
            // Sanity check for consistency of the tail
            if (tail != q->tail) {
                break;
            }

            // Queue could be full
            if (new_tail == q->head) {
                break;
            }

            // Check the next cell
            cell = q->nodes[new_tail];
            new_tail = (new_tail + 1) % q->max;
        }

        // Sanity check for consistency of tail
        if (tail != q->tail) {
            continue;
        }

        // Check whether queue is full
        if (new_tail == q->head) {
            size_t tail_mod = (new_tail + 1) % q->max;
            cell = q->nodes[new_tail];

            // Check if the cell after the head is occupied and queue is full
            if (VALID(cell.state)) {
                return 0;
            }

            // Help the current dequeue update the head
            CAS(&q->head, new_tail, tail_mod);
            // Retry the enqueue
            continue;
        }

        uintptr_t state = NULLTYPE(cell.state) ? VALID_A : VALID_B;

        // Sanity check for consistency of tail
        if (tail != q->tail) {
            continue;
        }

        // Attempt to enqueue the new data
        if (CAS2(&q->nodes[new_tail], cell.data, cell.state, data, state)) {
            // Move the tail if something else hasn't been enqueued already
            CAS(&q->tail, tail, new_tail);
            return 1;
        }
    }
}

// Repeatedly attempt to add the element to the queue using a bounded
// exponential backoff system.  This should be used if data must be enqueued
// when the queue is full AND elements will be dequeued soon.
void bqueue_enq_repeat(bqueue q, void* data) {
    for (size_t wait = 2; !bqueue_enq(q, data); wait *= 2) {
        if (wait > 128) {
            wait = 128;
        }

        struct timespec timer = {0, wait * 1000};

        nanosleep(&timer, NULL);
    }
}

// Dequeue an element from the queue.
int bqueue_deq(bqueue q, void** result) {
    while (1) {
        size_t head = q->head;
        size_t new_head = (head + 1) % q->max;
        option cell = q->nodes[new_head];

        // Find the correct head
        while (!VALID(cell.state)) {
            // Check the consistency of head
            if (head != q->head) {
                break;
            }

            // Queue is empty
            if (new_head == q->tail) {
                return 0;
            }

            new_head = (new_head + 1) % q->max;
            cell = q->nodes[new_head];
        }

        // Check the consistency of head
        if (head != q->head) {
            continue;
        }

        // Help the current enqueue update the tail
        if (CAS(&q->tail, new_head, (new_head + 1) % q->max)) {
            // Retry the dequeue
            continue;
        }

        uintptr_t new = !NULLTYPE(cell.state);

        // Dequeue the element
        if (CAS2(&q->nodes[new_head], cell.data, cell.state, NULL, new)) {
            // Move the head if someone else hasn't dequeued already
            CAS(&q->head, head, new_head);
            *result = cell.data;
            return 1;
        }
    }
}
