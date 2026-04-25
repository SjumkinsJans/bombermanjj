all:
	gcc server.c -o server -lncurses -lpthread
	gcc client.c -o client -lncurses -lpthread

test:
	./server
	./client