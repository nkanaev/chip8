build:
	cc chip8.c --std=c99 -Wall `sdl2-config --cflags --libs` -o chip8
