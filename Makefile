CC=gcc -std=gnu11
FLAGS=-mcx16 -O2
LIBS=-lpthread

all:
	$(CC) $(FLAGS) $(LIBS) -o test -g test.c lf_queue.c lf_bqueue.c
