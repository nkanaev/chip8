#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL2/SDL.h>

#define PI 3.14159
#define C8_MEM_START 512
#define C8_SPRITE_WIDTH 8
#define C8_SPRITE_HEIGHT 15
#define C8_SCREEN_WIDTH 64
#define C8_SCREEN_HEIGHT 32
#define C8_PIXEL_SIZE 10
#define C8_MEM_SIZE 4096
#define C8_OP_PER_SEC 200

typedef struct {
    uint8_t cpu[16];
    uint8_t mem[C8_MEM_SIZE];
    uint16_t i;
    uint16_t pc;
    uint8_t screen[C8_SCREEN_WIDTH * C8_SCREEN_HEIGHT];
    uint8_t timer_delay;
    uint8_t timer_sound;
    uint8_t stack_p;
    uint16_t stack[16];
    uint16_t keyboard;
} c8vm;

enum {
    C8_DRAW_FLAG = 1 << 0,
    C8_BEEP_FLAG = 1 << 1,
};

uint8_t fontset[80] = {
  0xf0, 0x90, 0x90, 0x90, 0xf0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xf0, 0x10, 0xf0, 0x80, 0xf0, // 2
  0xf0, 0x10, 0xf0, 0x10, 0xf0, // 3
  0x90, 0x90, 0xf0, 0x10, 0x10, // 4
  0xf0, 0x80, 0xf0, 0x10, 0xf0, // 5
  0xf0, 0x80, 0xf0, 0x90, 0xf0, // 6
  0xf0, 0x10, 0x20, 0x40, 0x40, // 7
  0xf0, 0x90, 0xf0, 0x90, 0xf0, // 8
  0xf0, 0x90, 0xf0, 0x10, 0xf0, // 9
  0xf0, 0x90, 0xf0, 0x90, 0x90, // a
  0xe0, 0x90, 0xe0, 0x90, 0xe0, // b
  0xf0, 0x80, 0x80, 0x80, 0xf0, // c
  0xe0, 0x90, 0x90, 0x90, 0xe0, // d
  0xf0, 0x80, 0xf0, 0x80, 0xf0, // e
  0xf0, 0x80, 0xf0, 0x80, 0x80  // f
};

/*
Keyboard Map:

    chip-8      qwerty
    -------     -------
    1 2 3 c     1 2 3 4
    4 5 6 d     q w e r
    7 8 9 e     a s d f
    a 0 b f     z x c v
*/
uint8_t keymap[16] = {
    120, 49, 50, 51, 113, 119, 101, 97, 115, 100, 122, 99, 52, 114, 102, 118,
};

c8vm* c8init()
{
    int i;
    c8vm *vm = malloc(sizeof(c8vm));
    vm->pc = 0x200;
    vm->i = 0;
    vm->stack_p = 0;
    vm->keyboard = 0;
    for (i = 0; i < 80; i++)
        vm->mem[i] = fontset[i];
    for (i = 0; i < C8_SCREEN_WIDTH * C8_SCREEN_HEIGHT; i++)
        vm->screen[i] = 0;
    return vm;
}

void c8load(c8vm *vm, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    int c, i = vm->pc;
    if (fp) {
        while ((c = fgetc(fp)) != EOF) {
            vm->mem[i] = c;
            i++;
        }
    }
}

uint8_t c8step(c8vm *vm)
{
    int result = 0;
    int incPc = 1;
    uint16_t op = vm->mem[vm->pc] << 8 | vm->mem[vm->pc + 1];

    switch (op & 0xf000) {
        case 0x0000: {
            switch (op) {
                case 0x00e0: {
                    uint16_t i;
                    for (i = 0; i < C8_SCREEN_WIDTH * C8_SCREEN_HEIGHT; i++) {
                        vm->screen[i] = 0;
                    }
                    result ^= C8_DRAW_FLAG;
                    break;
                }
                case 0x00ee:
                    vm->stack_p--;
                    vm->pc = vm->stack[vm->stack_p];
                    break;
                default:
                    printf("not implemented: rca 180 command\n");
            }
            break;
        }
        case 0x1000:
            vm->pc = op & 0x0fff;
            incPc = 0;
            break;
        case 0x2000:
            vm->stack[vm->stack_p] = vm->pc;
            vm->stack_p++;
            vm->pc = op & 0x0fff;
            incPc = 0;
            break;
        case 0x3000:
            if (vm->cpu[(op & 0x0f00) >> 8] == (op & 0x00ff)) {
                vm->pc += 4;
                incPc = 0;
            }
            break;
        case 0x4000:
            if (vm->cpu[(op & 0x0f00) >> 8] != (op & 0x00ff)) {
                vm->pc += 4;
                incPc = 0;
            }
            break;
        case 0x5000:
            if (vm->cpu[(op & 0x0f00) >> 8] == vm->cpu[(op & 0x00f0) >> 4]) {
                vm->pc += 4;
                incPc = 0;
            }
            break;
        case 0x6000:
            vm->cpu[(op & 0x0f00) >> 8] = op & 0x00ff;
            break;
        case 0x7000:
            vm->cpu[(op & 0x0f00) >> 8] += op & 0x00ff;
            break;
        case 0x8000: {
            uint8_t x = (op & 0x0f00) >> 8;
            uint8_t y = (op & 0x00f0) >> 4;
            switch (op & 0x000f) {
                case 0x0:
                    vm->cpu[x] = vm->cpu[y];
                    break;
                case 0x1:
                    vm->cpu[x] |= vm->cpu[y];
                    break;
                case 0x2:
                    vm->cpu[x] &= vm->cpu[y];
                    break;
                case 0x3:
                    vm->cpu[x] ^= vm->cpu[y];
                    break;
                case 0x4:
                    vm->cpu[0xf] = (vm->cpu[y] > (0xff - vm->cpu[x])) ? 1 : 0;
                    vm->cpu[x] += vm->cpu[y];
                    break;
                case 0x5:
                    vm->cpu[0xf] = (vm->cpu[y] > vm->cpu[x]) ? 0 : 1;
                    vm->cpu[x] -= vm->cpu[y];
                    break;
                case 0x6:
                    vm->cpu[0xf] = vm->cpu[(op & 0x0f00) >> 8] & 1;
                    vm->cpu[x] >>= 1;
                    break;
                case 0x7:
                    vm->cpu[0xf] = (vm->cpu[x] > vm->cpu[y]) ? 0 : 1;
                    vm->cpu[x] = vm->cpu[y] - vm->cpu[x];
                    break;
                case 0xe:
                    vm->cpu[0xf] = vm->cpu[x] >> 7;
                    vm->cpu[x] <<= 1;
                    break;
            }
            break;
        }
        case 0x9000:
            if (vm->cpu[(op & 0x0f00) >> 8] != vm->cpu[(op & 0x00f0) >> 4]) {
                vm->pc += 4;
                incPc = 0;
            }
            break;
        case 0xa000:
            vm->i = op & 0x0fff;
            break;
        case 0xb000:
            vm->pc = vm->cpu[0] + (op & 0x0fff);
            incPc = 0;
            break;
        case 0xc000:
            vm->cpu[(op & 0x0f00) >> 8] = (rand() % 256) & (op & 0x00ff);
            break;
        case 0xd000: {
            uint8_t x, y, n;
            x = vm->cpu[(op & 0x0f00) >> 8];
            y = vm->cpu[(op & 0x00f0) >> 4];
            n = op & 0x000f;
            vm->cpu[0xf] = 0;
            for (uint8_t i = 0; i < n; i++) {
                uint8_t rowSprite = vm->mem[vm->i + i];
                for (uint8_t j = 0; j < 8; j++) {
                    uint8_t bit = (rowSprite & (1 << j)) >> j;
                    uint16_t pos = x + (y + i) * C8_SCREEN_WIDTH + (7 - j);
                    if (vm->screen[pos] == 1 && bit == 1) {
                        vm->cpu[0xf] = 1;
                    }
                    vm->screen[pos] ^= bit;
                }
            }
            result ^= C8_DRAW_FLAG;
            break;
        }
        case 0xe000:
            switch (op & 0x00ff) {
                case 0x009e:
                    if (vm->keyboard & (1 << vm->cpu[(op & 0x0f00) >> 8])) {
                        vm->pc += 4;
                        incPc = 0;
                    }
                    break;
                case 0x00a1:
                    if (!(vm->keyboard & (1 << vm->cpu[(op & 0x0f00) >> 8]))) {
                        vm->pc += 4;
                        incPc = 0;
                    }
                    break;
            }
            break;
        case 0xf000:
            switch (op & 0x00ff) {
                case 0x0007:
                    vm->cpu[(op & 0x0f00) >> 8] = vm->timer_delay;
                    break;
                case 0x000a:
                    incPc = 0;
                    for (uint8_t i = 0; i < 16; i++) {
                        if (vm->keyboard & 1 << i) {
                            vm->cpu[(op & 0x0f00) >> 8] = i;
                            incPc = 1;
                            break;
                        }
                    }
                    break;
                case 0x0015:
                    vm->timer_delay = vm->cpu[(op & 0x0f00) >> 8];
                    break;
                case 0x0018:
                    vm->timer_sound = vm->cpu[(op & 0x0f00) >> 8];
                    break;
                case 0x001e:
                    vm->i += vm->cpu[(op & 0x0f00) >> 8];
                    break;
                case 0x0029:
                    vm->i = vm->cpu[(op & 0x0f00) >> 8] * 5;
                    break;
                case 0x0033: {
                    uint8_t x = vm->cpu[(op & 0x0f00) >> 8];
                    vm->mem[vm->i] = x / 100;
                    vm->mem[vm->i + 1] = x / 10 % 10;
                    vm->mem[vm->i + 2] = x % 10;
                    break;
                }
                case 0x0055:
                    for (uint16_t i = 0; i <= ((op & 0x0f00) >> 8); i++) {
                        vm->mem[vm->i + i] = vm->cpu[i];
                    }
                    break;
                case 0x0065:
                    for (uint16_t i = 0; i <= ((op & 0x0f00) >> 8); i++) {
                        vm->cpu[i] = vm->mem[vm->i + i];
                    }
                    break;
            }
            break;
    }
    if (incPc) vm->pc += 2;
    if (vm->timer_delay > 0) vm->timer_delay--;
    if (vm->timer_sound > 0) vm->timer_sound--;
    if (vm->timer_sound != 0) result ^= C8_BEEP_FLAG;

    return result;
}


void c8draw(c8vm *vm, SDL_Surface *surface)
{
    SDL_Rect pixel;
    pixel.w = C8_PIXEL_SIZE;
    pixel.h = C8_PIXEL_SIZE;

    Uint32 colorBg = SDL_MapRGB(surface->format, 19, 19, 19);
    Uint32 colorFg = SDL_MapRGB(surface->format, 255, 255, 255);

    SDL_FillRect(surface, NULL, colorBg);

    for (uint8_t y = 0; y < C8_SCREEN_HEIGHT; y++) {
        for (uint8_t x = 0; x < C8_SCREEN_WIDTH; x++) {
            if (vm->screen[y * C8_SCREEN_WIDTH + x]) {
                pixel.x = x * C8_PIXEL_SIZE;
                pixel.y = y * C8_PIXEL_SIZE;
                SDL_FillRect(surface, &pixel, colorFg);
            }
        }
    }
}

int chip8Key(SDL_Keycode key)
{
    for (int i = 0; i < 16; i++) {
        if (keymap[i] == key) return i;
    }
    return -1;
}

void c8audioCallback(void *userdata, Uint8 *stream, int len)
{
    static uint16_t sample_nr = 0;
    Sint16 *buffer = (Sint16*)stream;
    int length = len / 2;

    for (int i = 0; i < length; i++, sample_nr++) {
        double time = (double)sample_nr / (double)44100;
        float sign = sinf(2.0f * PI * 90.0f * time) > 0 ? 1 : 0;
        buffer[i] = (Sint16)(28000 * sign);
    }
}

int main(int argc, char **argv)
{
    uint32_t lastTime = 0, currentTime;
    int quit = 0, audioPlaying = 0;

    srand(time(NULL));
    c8vm *vm = c8init();
    if (argc != 2) {
      printf("usage: %s <romfile>\n", argv[0]);
      exit(1);
    }

    c8load(vm, argv[1]);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    SDL_Event event;
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID audioDev;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 2048;
    want.callback = c8audioCallback;
    audioDev = SDL_OpenAudioDevice(
        NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);

    SDL_Window *window = SDL_CreateWindow(
        "CHIP-8",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        C8_SCREEN_WIDTH * C8_PIXEL_SIZE, C8_SCREEN_HEIGHT * C8_PIXEL_SIZE,
        SDL_WINDOW_OPENGL);

    SDL_Surface *surface = SDL_GetWindowSurface(window);

    while (!quit) {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            } else if (event.type == SDL_KEYDOWN) {
                int key = chip8Key(event.key.keysym.sym);
                if (key != -1) {
                    vm->keyboard |= 1 << key;
                }
            } else if (event.type == SDL_KEYUP) {
                int key = chip8Key(event.key.keysym.sym);
                if (key != -1) {
                    vm->keyboard &= ~(1 << key);
                }
            }
        }
        currentTime = SDL_GetTicks();
        if (currentTime >= lastTime + 1000 / C8_OP_PER_SEC) {
            lastTime = currentTime;
            uint8_t result = c8step(vm);
            if (result & C8_DRAW_FLAG) {
                c8draw(vm, surface);
                SDL_UpdateWindowSurface(window);
            }
            if (result & C8_BEEP_FLAG) {
                if (!audioPlaying) SDL_PauseAudioDevice(audioDev, 0);
                audioPlaying = 1;
            } else {
                if (audioPlaying) SDL_PauseAudioDevice(audioDev, 1);
                audioPlaying = 0;
            }
        }
    }
    SDL_CloseAudioDevice(audioDev);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
