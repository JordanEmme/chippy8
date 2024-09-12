#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <cstring>

const short DISPLAY_WIDTH = 640;
const short DISPLAY_HEIGHT = 320;

SDL_Window* window;
SDL_Renderer* renderer;

unsigned char memory[4096];

unsigned short pc;
unsigned short i;

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

    unsigned char x;
    unsigned char y;
    unsigned short kk;

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
            x = opcode & 0x0F00 >> 8;
            kk = opcode & 0x00FF;
            if (get_opcode_at(x) == kk) {
                pc += 2;
            }
            break;
        case 0x4000:
            x = opcode & 0x0F00 >> 8;
            kk = opcode & 0x00FF;
            if (get_opcode_at(x) != kk) {
                pc += 2;
            }
        case 0x5000:
            x = opcode & 0x0F00;
            y = opcode & 0x00F0;
            if (get_opcode_at(x) == get_opcode_at(y)) {
                pc += 2;
            }
        case 0x6000:
        case 0x7000:
        case 0x8000:
        case 0x9000:
        case 0xA000:
        case 0xB000:
        case 0xC000:
        case 0xD000:
        case 0xE000:
        case 0xF000:
    }
}

void update_display() {}

int main() {
    load_font_in_memory();
    initialise_display();
    keyboardStates = SDL_GetKeyboardState(NULL);

    while (false) {
        fetch();
        decode_and_execute();
        update_display();
    }

    return 0;
}
