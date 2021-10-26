all: dir binary

dir:
	mkdir -p bin/

binary: client connect

client:
	gcc -o bin/client client.c

connect:
	gcc -o bin/connect connect.c
