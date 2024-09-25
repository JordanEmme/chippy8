#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>

#define PIXEL_SIZE 16
#define WHITE 255, 255, 255, 255
#define BLACK 0, 0, 0, 255

const short DISPLAY_WIDTH = 64 * PIXEL_SIZE;
const short DISPLAY_HEIGHT = 32 * PIXEL_SIZE;

SDL_Window* window;
SDL_Renderer* renderer;
unsigned char runningState = 1;

unsigned char memory[4096];

unsigned short pc;
unsigned short I;

unsigned short stack[30];
unsigned short sp;

unsigned char V[16];
unsigned short opcode;

unsigned char display[64 * 32];

unsigned char soundTimer;
unsigned char delayTimer;

const Uint8* keyboardStates;

const unsigned char font[80] {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

const unsigned char keyboardMap[16] {
    SDL_SCANCODE_X,
    SDL_SCANCODE_1,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_Q,
    SDL_SCANCODE_W,
    SDL_SCANCODE_E,
    SDL_SCANCODE_A,
    SDL_SCANCODE_S,
    SDL_SCANCODE_D,
    SDL_SCANCODE_Z,
    SDL_SCANCODE_C,
    SDL_SCANCODE_4,
    SDL_SCANCODE_R,
    SDL_SCANCODE_F,
    SDL_SCANCODE_V
};

void load_font_in_memory() {
    memcpy(memory, font, 80 * sizeof(unsigned char));
}

void initialise_display() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("CHIP-8", 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
}

unsigned short get_opcode_at(unsigned char x) {
    unsigned char opcode1 = V[x];
    unsigned char opcode2 = V[x + 1];
    return (opcode1 << 8) | opcode2;
}

void fetch() {
    opcode = get_opcode_at(pc);
    pc += 2;
}

void decode_and_execute() {
    if (opcode == 0x00E0) {
        // clear display
        return;
    }
    if (opcode == 0x00EE) {
        // return from subroutine
        return;
    }

    unsigned char x = 0;
    unsigned char y = 0;
    unsigned char kk = 0;

    unsigned short n = 0;

    switch (opcode & 0xF000) {
        case 0x0000:
            // jump to machine code routine at opcode & 0x0FFF
            break;
        case 0x1000:
            pc = opcode & 0x0FFF;
            break;
        case 0x2000:
            // call subroutine at opcode & 0x0FFF
            break;
        case 0x3000:
            x = (opcode & 0x0F00) >> 8;
            kk = opcode & 0x00FF;
            if (V[x] == kk) {
                pc += 2;
            }
            break;
        case 0x4000:
            x = (opcode & 0x0F00) >> 8;
            kk = opcode & 0x00FF;
            if (V[x] != kk) {
                pc += 2;
            }
            break;
        case 0x5000:
            x = opcode & 0x0F00;
            y = opcode & 0x00F0;
            if (V[x] == V[y]) {
                pc += 2;
            }
            break;
        case 0x6000:
            x = (opcode & 0x0F00) >> 8;
            V[x] = opcode & 0x00FF;
            break;
        case 0x7000:
            x = opcode & 0x0F00 >> 8;
            kk = opcode & 0x00FF;
            V[x] += kk;
            break;
        case 0x8000:
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0x00F0) >> 4;
            switch (opcode & 0x000F) {
                case 0x0:
                    V[x] = V[y];
                    break;
                case 0x1:
                    V[x] = V[x] | V[y];
                    break;
                case 0x2:
                    V[x] = V[x] & V[y];
                    break;
                case 0x3:
                    V[x] = V[x] ^ V[y];
                    break;
                case 0x4:
                    if (V[y] > 255 - V[x]) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    V[x] += V[y];
                    break;
                case 0x5:
                    // Deviation from cowgod's spec, in line with VF = NOT borrow
                    if (V[x] >= V[y]) {
                        V[x] = V[x] - V[y];
                        V[0xF] = 0;
                    } else {
                        V[x] = V[y] - V[x];
                        V[0xF] = 1;
                    }
                    break;
                case 0x6:
                    // Deviation from standard, because this is how most roms behave according to octo
                    if (V[y] & 1) {
                        V[0xF] = 1;
                    }
                    V[x] = V[y] >> 1;
                    break;
                case 0x7:
                    break;
                case 0xE:
                    if ((V[y] & 0x80) >> 7) {
                        V[0xF] = 1;
                    }
                    V[x] = V[y] << 1;
                    break;
            }
            break;
        case 0x9000:
            if (V[x] != V[y]) {
                pc += 2;
            }
            break;
        case 0xA000:
            I = opcode & 0xFFF;
            break;
        case 0xB000:
            pc = (opcode & 0xFFF) + V[0];
            break;
        case 0xC000:
            V[x] = (opcode & 0xFF) & rand();
            break;
        case 0xD000:
            n = opcode & 0xF;
            x = (opcode & 0xF00) >> 8;
            y = (opcode & 0xF0) >> 4;
            {
                V[0xF] = 0;
                unsigned short displayRowStart = y * 64;
                for (short i = 0; i < n; i++) {
                    unsigned char byte = memory[I + i];
                    for (short bitShift = 0; bitShift < 8; bitShift++) {
                        unsigned short displayCoord = (x + 8 * i + bitShift) % 64;
                        displayCoord += displayRowStart;
                        if ((byte & (1 << bitShift) && display[displayCoord])) {
                            display[displayCoord] = 0;
                            V[0xF] = 1;
                        } else if (byte & (1 << bitShift) || display[displayCoord]) {
                            display[displayRowStart + x + 8 * i + bitShift] = 1;
                        }
                    }
                }
            }
            break;
        case 0xE000:
            x = (opcode & 0xF00) >> 8;
            if (opcode & 0x9E && keyboardStates[keyboardMap[V[x]]]) {
                pc += 2;
            } else if (opcode & 0xA1 && !keyboardStates[keyboardMap[V[x]]]) {
                pc += 2;
            }
            break;
        case 0xF000:
            x = (opcode & 0xF00) >> 8;
            switch (opcode & 0xFF) {
                case 0x07:
                    V[x] = delayTimer;
                    break;
                case 0x0A: {
                    SDL_Event event;
                    while (SDL_WaitEvent(&event)) {
                        if (event.type == SDL_KEYDOWN) {
                            V[x] = keyboardMap[event.key.keysym.scancode];
                        }
                    }
                } break;
                case 0x15:
                    delayTimer = V[x];
                    break;
                case 0x18:
                    soundTimer = V[x];
                    break;
                case 0x1E:
                    I += V[x];
                    break;
                case 0x29:
                    break;
                case 0x33:
                    break;
                case 0x55:
                    break;
                case 0x65:
                    break;
            }
            break;
    }
}

void update_display() {
    SDL_SetRenderDrawColor(renderer, BLACK);
    SDL_RenderClear(renderer);
    short displayArrayOffset = 0;
    for (int y = 0; y < DISPLAY_HEIGHT; y += PIXEL_SIZE) {
        for (int x = 0; x < DISPLAY_WIDTH; x += PIXEL_SIZE) {
            if (display[displayArrayOffset++]) {
                SDL_Rect rectangle {x, y, PIXEL_SIZE, PIXEL_SIZE};
                SDL_SetRenderDrawColor(renderer, WHITE);
                SDL_RenderFillRect(renderer, &rectangle);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

int main() {
    load_font_in_memory();
    initialise_display();
    keyboardStates = SDL_GetKeyboardState(NULL);
    SDL_Event event;
    while (runningState) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    runningState = 0;
            }
        }
        fetch();
        decode_and_execute();
        update_display();
    }

    return 0;
}
