CFLAGS=-Wall -Wextra
LDFLAGS=-lncurses -lSDL2 -lm

all: hex20

hex20: main.o hd6301.o mem.o console.o rs232.o piezo.o debugger.o crc32.o
	gcc -o hex20 $^ ${LDFLAGS}

main.o: main.c
	gcc -c $^ ${CFLAGS}

hd6301.o: hd6301.c
	gcc -c $^ ${CFLAGS}

mem.o: mem.c
	gcc -c $^ ${CFLAGS}

console.o: console.c
	gcc -c $^ ${CFLAGS}

rs232.o: rs232.c
	gcc -c $^ ${CFLAGS}

piezo.o: piezo.c
	gcc -c $^ ${CFLAGS}

debugger.o: debugger.c
	gcc -c $^ ${CFLAGS}

crc32.o: crc32.c
	gcc -c $^ ${CFLAGS}

.PHONY: clean
clean:
	rm -f *.o hex20

