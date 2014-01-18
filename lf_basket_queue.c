/*
 * A lock-free queue implementation based off of the Baskets Queue outlined by
 * Hoffman, Shalev, and Shavit [0] to provide a wait-free queue optimized for
 * high performance when doing multiple enqueues or dequeues at the same time.
 *
 * May have more overhead than alternatives, such as the optimistic queue, when
 * not at high levels of concurrency.  If this is the case, use an optimistic
 * queue instead.  Additionally, this method may use significantly more memory
 * than other queue implementations.
 *
 * [0] The Baskets Queue - Hoffman, Shalev, Shavit
 */

#include "lf_queue.h"
#include "cas.h"
#include <sched.h>

typedef struct pointer {
	struct cell* ptr;
	uintptr_t tag;
} pointer;

// Properly set the deleted bit inside the tag ptr
static inline uintptr_t delete_tag(int deleted, uintptr_t tag) {
	return (0xFFFFFFFFFFFFFFFF >> (!deleted)) & tag;
}

// Check if the deleted bit is set inside the tag ptr
static inline int deleted(uintptr_t tag) {
	return !!(((uintptr_t)1 << 63) & tag);
}

typedef struct cell {
    struct pointer next;
    void* data;
} cell;

struct queue {
    pointer head;
    pointer tail;
};

// Returns 1 if the two pointers are equal, else 0
static inline int pointer_cmp(pointer a, pointer b) {
	if (a.ptr != b.ptr) {
		return 0;
	}
	if (a.tag != b.tag) {
		return 0;
	}
	return 1;
}

// Allocate space for the queue and fill it with a dummy node
queue new_queue() {
    queue new = malloc(sizeof(struct queue));
    // Create a dummy node
    cell* dummy = malloc(sizeof(cell));
    dummy->next = (pointer) {NULL, 0};
    // Head and tail point to dummy
	new->tail = (pointer) {dummy, 0};
	new->head = (pointer) {dummy, 0};
    return new;
}

// Enqueue the given data into the queue.
void queue_enq(queue q, void* data) {
    // Create new cell
    cell* c = malloc(sizeof(cell));
    c->data = data;

	pointer tail;
	pointer next;

	while (1) {
		tail = q->tail;
		next = tail.ptr->next;

		if (!pointer_cmp(tail, q->tail)) {
			continue;
		}
		if (next.ptr == NULL) {
			c->next = (pointer) {NULL, delete_tag(0, tail.tag + 2)};
			uintptr_t tag = delete_tag(0, tail.tag + 1);
			if (CAS2(&tail.ptr->next, next.ptr, next.tag, c, tag)) {
				// Successfully appended
				CAS2(&q->tail, tail.ptr, tail.tag, c, tag);
				return;
			}
			for (next = tail.ptr->next;
				 (next.tag == tail.tag + 1) && !deleted(next.tag);
				 next = tail.ptr->next) {
				sched_yield();
				c->next = next;
				uintptr_t tag = delete_tag(0, tail.tag + 1);
				if (CAS2(&tail.ptr->next, next.ptr, next.tag, c, tag)) {
					return;
				}
			}
		}
		else {
			while (next.ptr->next.ptr != NULL && pointer_cmp(q->tail, tail)) {
				next = next.ptr->next;
			}
			uintptr_t tag = delete_tag(0, tail.tag + 1);
			CAS2(&q->tail, tail.ptr, tail.tag, next.ptr, tag);
		}
	}
}

void free_chain(queue q, pointer head, pointer new) {
	uintptr_t tag = delete_tag(0, head.tag + 1);
	if (CAS2(&q->head, head.ptr, head.tag, new.ptr, tag)) {
		while (head.ptr != new.ptr) {
			pointer next = head.ptr->next;
			free(head.ptr);
			head = next;
		}
	}
}

// Dequeue an element from the queue, returning 0 if unsuccessful
int queue_deq(queue q, void** result) {
	// Maximum distance from deleted node to head for dequeue
	const size_t max_hops = 3;
	while (1) {
		pointer head = q->head;
		pointer tail = q->tail;
		pointer next = head.ptr->next;

		if (!pointer_cmp(head, q->head)) {
			continue;
		}

		if (head.ptr == tail.ptr) {
			if (next.ptr == NULL) {
				return 0;
			}
			while (next.ptr->next.ptr != NULL && pointer_cmp(q->tail, tail)) {
				next = next.ptr->next;
			}
			uintptr_t tag = delete_tag(0, tail.tag + 1);
			CAS2(&q->tail, tail.ptr, tail.tag, next.ptr, tag);
		}
		else {
			pointer iter = head;
			size_t hops = 0;
			while (deleted(next.tag) && iter.ptr != tail.ptr &&
				   pointer_cmp(q->head, head)) {
				iter = next;
				next = iter.ptr->next;
				hops++;
			}
			if (!pointer_cmp(q->head, head)) {
				continue;
			}
			else if (iter.ptr == tail.ptr) {
				free_chain(q, head, iter);
			}
			else {
				void* value = next.ptr->data;
				uintptr_t tag = delete_tag(1, next.tag + 1);
				if (CAS2(&iter.ptr->next, next.ptr, next.tag, next.ptr, tag)) {
					if (hops >= max_hops) {
						free_chain(q, head, next);
					}
					*result = value;
					return 1;
				}
				sched_yield();
			}
		}
	}
}
