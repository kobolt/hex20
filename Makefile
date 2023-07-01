
OBJECTS=main.o hd6301.o mem.o console.o rs232.o cassette.o serial.o debugger.o crc32.o
CFLAGS=-Wall -Wextra
LDFLAGS=-lncurses

# Check for SDL2 and enable Piezo speaker emulation if it exists.
SDL2_LDFLAGS=$(shell sdl2-config --libs)
ifneq (${SDL2_LDFLAGS},)
CFLAGS+=-DPIEZO_AUDIO_ENABLE
LDFLAGS+=${SDL2_LDFLAGS} -lm
OBJECTS+=piezo.o
endif

all: hex20

hex20: ${OBJECTS}
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

cassette.o: cassette.c
	gcc -c $^ ${CFLAGS}

serial.o: serial.c
	gcc -c $^ ${CFLAGS}

debugger.o: debugger.c
	gcc -c $^ ${CFLAGS}

crc32.o: crc32.c
	gcc -c $^ ${CFLAGS}

.PHONY: clean
clean:
	rm -f *.o hex20

