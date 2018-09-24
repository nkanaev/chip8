build:
	cc chip8.c `sdl2-config --cflags --libs` -o chip8
