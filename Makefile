CFLAGS=-Wall -g

all: nim nim-server

clean:
	-rm -f nim.o nim-server.o nim nim-server

nim: nim.o
	gcc -o nim nim.o $(CFLAGS)

nim.o: nim.c
	gcc -c $(CFLAGS) nim.c

nim-server: nim-server.o
	gcc -o nim-server nim-server.o $(CFLAGS)

nim-server.o: nim-server.c
	gcc -c $(CFLAGS) nim-server.c