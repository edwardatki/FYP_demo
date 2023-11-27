.DEFAULT_GOAL := main.bin

CC=gcc
CFLAGS=-lm -fopenmp

clean:
	rm -f *.out *.bin

run: main.bin
	./main.bin

%.bin: %.c
	$(CC) $< -o $@ $(CFLAGS)