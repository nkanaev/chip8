build:
	cc chip8.c --std=c99 -Wall `sdl2-config --cflags --libs` -lm -o chip8
