#include "../lf_bqueue.h"
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

bqueue bq;
#define TC 4
#define LE 5

void* thread_func(void* z) {
    pthread_detach(pthread_self());
    uintptr_t t = (uintptr_t) z;

    for (uintptr_t i = 10 * t; i < 10 * t + LE; i++) {
        bqueue_enq(bq, (void*) i);
    }

    for (uintptr_t i = LE * t; i < LE * t + LE + 1; i++) {
        uintptr_t res;

        if (bqueue_deq(bq, (void*)&res)) {
            printf("got %lu from (%lu)\n", res, t);
        }
        else {
            printf("BQueue was empty!\n");
        }

        fflush(stdout);
    }
}

int main() {
    bq = new_bqueue(1024);

    for (uintptr_t i = 0; i < TC; i++) {
        pthread_t t;
        pthread_create(&t, NULL, thread_func, (void*) i);
    }

    sleep(1);
}
