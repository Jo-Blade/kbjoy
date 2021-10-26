all: dir kbjoy-daemon kbjoy-remapper kbjoy-trigger

dir:
	mkdir -p bin/
	touch bin/process.lock
	touch bin/out.lock

kbjoy-daemon: ipc.o server.o main.o input.o config.o
	gcc -o bin/kbjoy-daemon bin/ipc.o bin/server.o bin/main.o bin/input.o bin/config.o

kbjoy-remapper: ipc.o server.o remapper.o config.o
	gcc -o bin/kbjoy-remapper bin/ipc.o bin/remapper.o bin/input.o bin/config.o

kbjoy-trigger: ipc.o trigger.o
	gcc -o bin/kbjoy-trigger bin/ipc.o bin/trigger.o

trigger.o: trigger.c ipc.h
	gcc -o bin/trigger.o -c trigger.c -W -Wall -ansi -pedantic

remapper.o: remapper.c config.h
	gcc -o bin/remapper.o -c remapper.c -W -Wall -ansi -pedantic

ipc.o: ipc.c
	gcc -o bin/ipc.o -c ipc.c -W -Wall -ansi -pedantic

server.o: server.c ipc.h input.h config.h
	gcc -o bin/server.o -c server.c -W -Wall -ansi -pedantic

main.o: main.c ipc.h server.h
	gcc -o bin/main.o -c main.c -W -Wall -ansi -pedantic

input.o: input.c
	gcc -o bin/input.o -c input.c -W -Wall -ansi -pedantic

config.o: config.c
	gcc -o bin/config.o -c config.c -W -Wall -ansi -pedantic

clean:
	rm -rf bin/*.o

mrproper: clean
	rm -rf bin/kbjoy-daemon
