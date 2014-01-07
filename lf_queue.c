/*
 * A lock-free queue implementation based off of the method outlined by
 * Michael and Scott [0] and relying on hardware support for CAS.
 *
 * Implemented as a linked list to allow for arbitrary size.  If a maximum size
 * is known in advance, use a bounded queue instead.
 *
 * [0] Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue
 *     Algorithms  -  Michael and Scott
 */

#include "lf_queue.h"
#include "cas.h"

typedef struct cell {
    struct cell* next;
    void* data;
} cell;

struct queue {
    cell* head;
    uintptr_t head_count;
    cell* tail;
    uintptr_t tail_count;
};

// Allocate space for the queue and fill it with a dummy node
queue new_queue() {
    queue new = malloc(sizeof(struct queue));
    // Create a dummy node
    cell* dummy = malloc(sizeof(cell));
    dummy->next = NULL;
    // Head and tail point to dummy
    new->head = dummy;
    new->head_count = 0;
    new->tail = dummy;
    new->tail_count = 0;
    return new;
}

// Enqueue the given data into the queue.
void queue_enq(queue queue, void* data) {
    // Create new cell
    cell* c = malloc(sizeof(cell));
    c->data = data;
    c->next = NULL;
    cell* tail = NULL;
    uintptr_t tail_count = 0;

    // Loop until we append properly
    while (1) {
        tail_count = queue->tail_count;
        tail = queue->tail;

        // Try to add the new node to the end of the list
        if (CAS(&tail->next, NULL, c)) {
            break;
        }

        // tail wasn't pointing to the last cell, try to move  to the next cell
        CAS2(&queue->tail, tail, tail_count, tail->next, tail_count + 1);
    }

    // Finished enque, set tail to enqueued element if still newest
    CAS2(&queue->tail, tail, tail_count, c, tail_count + 1);
}

// Dequeue an element from the queue, returning 0 if unsuccessful
int queue_deq(queue queue, void** result) {
    cell* head = NULL;

    while (1) {
        uintptr_t head_count = queue->head_count;
        uintptr_t tail_count = queue->tail_count;
        head = queue->head;
        cell* tail = queue->tail;
        cell* next = head->next;

        // Check consistency of previous operations as a short circuit
        if (head != queue->head) {
            continue;
        }

        // Is queue empty or tail falling behind?
        if (head == tail) {
            if (next == NULL) { // Queue is empty
                return 0;
            }

            // Tail is falling behind, try to catch up
            CAS2(&queue->tail, tail, tail_count, next, tail_count + 1);
        }
        else if (next != NULL) {
            *result = next->data;

            // Successfully dequeued
            if (CAS2(&queue->head, head, head_count, next, head_count + 1)) {
                break;
            }
        }
    }

    free(head);
    return 1;
}
