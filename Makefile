CC=gcc -std=gnu11
FLAGS=-mcx16 -O2
LIBS=-lpthread

all: bq msq basketq

bq: lf_bqueue.c tests/bqueue.c lf_bqueue.h
	$(CC) $(FLAGS) $(LIBS) -o bq -g tests/bqueue.c lf_bqueue.c

msq: lf_ms_queue.c tests/queue.c lf_queue.h
	$(CC) $(FLAGS) $(LIBS) -o msq -g tests/queue.c lf_ms_queue.c

basketq: lf_basket_queue.c tests/queue.c lf_queue.h
	$(CC) $(FLAGS) $(LIBS) -o basketq -g tests/queue.c lf_basket_queue.c

clean:
	rm msq bq basketq
