#include <SDL2/SDL.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>

const short DISPLAY_WIDTH = 640;
const short DISPLAY_HEIGHT = 320;

SDL_Window* window;
SDL_Renderer* renderer;

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
    unsigned short kk = 0;

    unsigned int sum = 0;
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
            if (get_opcode_at(x) == kk) {
                pc += 2;
            }
            break;
        case 0x4000:
            x = (opcode & 0x0F00) >> 8;
            kk = opcode & 0x00FF;
            if (get_opcode_at(x) != kk) {
                pc += 2;
            }
            break;
        case 0x5000:
            x = opcode & 0x0F00;
            y = opcode & 0x00F0;
            if (get_opcode_at(x) == get_opcode_at(y)) {
                pc += 2;
            }
            break;
        case 0x6000:
            x = (opcode & 0x0F00) >> 8;
            V[x] = (opcode & 0x00F0) >> 4;
            V[x + 1] = opcode & 0x000F;
            break;
        case 0x7000:
            x = opcode & 0x0F00 >> 8;
            kk = opcode & 0x00FF;
            kk += get_opcode_at(x);
            V[x] = kk >> 4;
            V[x + 1] = kk & 0xF;
            break;
        case 0x8000:
            x = (opcode & 0x0F00) >> 8;
            y = (opcode & 0x00F0) >> 4;
            switch (opcode & 0x000F) {
                case 0x0:
                    kk = get_opcode_at(y);
                    break;
                case 0x1:
                    kk = get_opcode_at(x) | get_opcode_at(y);
                    break;
                case 0x2:
                    kk = get_opcode_at(x) & get_opcode_at(y);
                    break;
                case 0x3:
                    kk = get_opcode_at(x) ^ get_opcode_at(y);
                    break;
                case 0x4:
                    sum = get_opcode_at(x) + get_opcode_at(y);
                    if (sum > 255) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    kk = sum & 0xFF;
                    break;
                case 0x5:
                    if (get_opcode_at(x) >= get_opcode_at(y
                        )) {  // Deviation from cowgod's spec, in line with VF = NOT borrow
                        kk = get_opcode_at(x) - get_opcode_at(y);
                        V[0xF] = 0;
                    } else {
                        kk = get_opcode_at(y) - get_opcode_at(x);
                        V[0xF] = 1;
                    }
                    break;
                case 0x6:
                    if (get_opcode_at(y)
                        & 1) {  // Deviation from standard, because this is how most roms behave according to octo
                        V[0xF] = 1;
                    }
                    kk = get_opcode_at(y) >> 1;
                    break;
                case 0x7:
                    break;
                case 0xE:
                    if ((V[y] & 0x80) >> 7) {
                        V[0xF] = 1;
                    }
                    kk = get_opcode_at(y) << 1;
                    break;
            }
            V[x] = kk >> 4;
            V[x + 1] = kk & 0x0F;
            break;
        case 0x9000:
            if (get_opcode_at(x) != get_opcode_at(y)) {
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
            kk = (opcode & 0xFF) & rand();
            V[x] = kk >> 4;
            V[x + 1] = kk & 0x0F;
            break;
        case 0xD000:
            n = opcode & 0xF;
            x = (opcode & 0xF00) >> 8;
            y = (opcode & 0xF0) >> 4;
            {
                unsigned char pixelOff = 0;
                unsigned short displayRowStart = y * 64;
                for (short i = 0; i < n; i++) {
                    unsigned char byte = memory[I + i];
                    for (short bitShift = 0; bitShift < 8; bitShift++) {
                        unsigned short displayCoord = displayRowStart;
                        if (x + 8 * i + bitShift >= 64) {
                            displayCoord += x + 8 * i + bitShift - 64;
                        } else {
                            displayCoord += x + 8 * i + bitShift;
                        }
                        if ((byte & (1 << bitShift)
                             && display[displayRowStart + x + 8 * i + bitShift])) {
                            display[displayRowStart + x + 8 * i + bitShift] = 0;
                            pixelOff = 1;
                        } else if (byte & (1 << bitShift)
                                   || display[displayRowStart + x + 8 * i + bitShift]) {
                            display[displayRowStart + x + 8 * i + bitShift] = 1;
                        }
                    }
                }
                V[0xF] = pixelOff;
            }
            break;
        case 0xE000:
            break;
        case 0xF000:
            break;
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
