#include "../lf_queue.h"
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

queue q;
#define TC 4
#define LE 5

void* thread_func(void* z) {
    pthread_detach(pthread_self());
    uintptr_t t = (uintptr_t) z;

    for (uintptr_t i = LE * t; i < LE * (t + 1); i++) {
        queue_enq(q, (void*) i);
    }

    for (uintptr_t i = LE * t; i < LE * (t + 1); i++) {
        uintptr_t res;

        if (queue_deq(q, (void*)&res)) {
            printf("got %lu from [%lu]\n", res, t);
        }
        else {
            printf("Queue was empty!\n");
        }

        fflush(stdout);
    }
}

int main() {
    q = new_queue();

    for (uintptr_t i = 0; i < TC; i++) {
        pthread_t t;
        pthread_create(&t, NULL, thread_func, (void*) i);
    }

    sleep(1);
}
