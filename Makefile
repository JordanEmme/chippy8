CC=g++
CFLAGS= -Wall -Werror -lSDL2

main:
	$(CC) main.cpp $(CFLAGS)

debug:
	$(CC) main.cpp -g -o debug.out $(CFLAGS)
