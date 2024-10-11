#include "SDL2/SDL.h"
#include "SDL2/SDL_events.h"
#include "SDL2/SDL_keyboard.h"
#include "SDL2/SDL_log.h"
#include "SDL2/SDL_render.h"
#include "SDL2/SDL_scancode.h"
#include "SDL2/SDL_stdinc.h"
#include "SDL2/SDL_video.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <linux/limits.h>
#include <unistd.h>

#define WHITE 255, 255, 255, 255
#define BLACK 0, 0, 0, 255

const uint8_t DISPLAY_WIDTH = 64;
const uint8_t DISPLAY_HEIGHT = 32;
const uint16_t DISPLAY_LEN = DISPLAY_WIDTH * DISPLAY_HEIGHT;
const uint8_t PIXEL_SIZE = 16;
const uint16_t WINDOW_WIDTH = DISPLAY_WIDTH * PIXEL_SIZE;
const uint16_t WINDOW_HEIGHT = DISPLAY_HEIGHT * PIXEL_SIZE;
const uint16_t ROM_START_ADDRESS = 0x200;
const uint16_t MEM_SIZE = 4096;

SDL_Window* window;
SDL_Renderer* renderer;
bool runningState = true;

uint8_t memory[MEM_SIZE];

uint16_t pc;
uint16_t I;

uint16_t stack[16];
uint16_t sp;

uint8_t V[16];
uint16_t opcode;

bool display[DISPLAY_LEN];

uint8_t soundTimer = 60;
uint8_t delayTimer;

const Uint8* keyboardStates;

const uint8_t font[80] {
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

const SDL_Scancode keyboardMap[16] {
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

int8_t scancode_to_reg_value(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_X:
            return 0;
        case SDL_SCANCODE_1:
            return 1;
        case SDL_SCANCODE_2:
            return 2;
        case SDL_SCANCODE_3:
            return 3;
        case SDL_SCANCODE_Q:
            return 4;
        case SDL_SCANCODE_W:
            return 5;
        case SDL_SCANCODE_E:
            return 6;
        case SDL_SCANCODE_A:
            return 7;
        case SDL_SCANCODE_S:
            return 8;
        case SDL_SCANCODE_D:
            return 9;
        case SDL_SCANCODE_Z:
            return 10;
        case SDL_SCANCODE_C:
            return 11;
        case SDL_SCANCODE_4:
            return 12;
        case SDL_SCANCODE_R:
            return 13;
        case SDL_SCANCODE_F:
            return 14;
        case SDL_SCANCODE_V:
            return 15;
        default:
            return -1;
    }
}

void load_font() {
    memcpy(memory, font, 80 * sizeof(unsigned char));
}

void get_rom_absolute_path(char* absolutePath, const char* userRomPath) {
    if (userRomPath[0] == '/') {
        strcpy(absolutePath, userRomPath);
    } else {
        char wd[PATH_MAX];
        getcwd(wd, 255);
        strcpy(absolutePath, wd);
        strcat(absolutePath, "/");
        strcat(absolutePath, userRomPath);
    }
}

void load_rom(char* userRomPath) {
    char absoluteRomPath[PATH_MAX];
    get_rom_absolute_path(absoluteRomPath, userRomPath);

    std::ifstream file(absoluteRomPath, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        std::streampos size = file.tellg();

        file.seekg(0, std::ios::beg);
        if (ROM_START_ADDRESS + size >= MEM_SIZE) {
            return;
        }
        file.read((char*)(memory + ROM_START_ADDRESS), size);
        file.close();

        pc = ROM_START_ADDRESS;
    }
}

void initialise_display() {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Chippy8", 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
}

uint16_t get_opcode_at_pc() {
    uint8_t opcode1 = memory[pc];
    uint8_t opcode2 = memory[pc + 1];
    return (opcode1 << 8) | opcode2;
}

void fetch() {
    opcode = get_opcode_at_pc();
    pc += 2;
}

void decode_and_execute() {
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;

    uint8_t n = opcode & 0x000F;
    uint8_t kk = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;

    uint8_t vf;

    switch (opcode & 0xF000) {
        case 0x0000:
            if (opcode == 0x00E0) {
                // clear display
                memset(display, 0, DISPLAY_LEN);
            } else if (opcode == 0x00EE) {
                // return from subroutine
                pc = stack[sp--];
            }
            break;
        case 0x1000:
            pc = nnn;
            break;
        case 0x2000:
            // call subroutine at opcode & 0x0FFF
            stack[++sp] = pc;
            pc = nnn;
            break;
        case 0x3000:
            if (V[x] == kk) {
                pc += 2;
            }
            break;
        case 0x4000:
            if (V[x] != kk) {
                pc += 2;
            }
            break;
        case 0x5000:
            if (V[x] == V[y]) {
                pc += 2;
            }
            break;
        case 0x6000:
            V[x] = kk;
            break;
        case 0x7000:
            V[x] += kk;
            break;
        case 0x8000:
            switch (n) {
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
                        vf = 1;

                    } else {
                        vf = 0;
                    }
                    V[x] += V[y];
                    V[0xF] = vf;
                    break;
                case 0x5:
                    vf = V[x] >= V[y];
                    V[x] -= V[y];
                    V[0xF] = vf;
                    break;
                case 0x6:
                    vf = (V[x] & 1);
                    V[x] = V[x] >> 1;
                    V[0xF] = vf;
                    break;
                case 0x7:
                    vf = V[y] >= V[x];
                    V[x] = V[y] - V[x];
                    V[0xF] = vf;
                    break;
                case 0xE:
                    vf = (V[x] & 0x80) >> 7;
                    V[x] = V[x] << 1;
                    V[0xF] = vf;
                    break;
            }
            break;
        case 0x9000:
            if (V[x] != V[y]) {
                pc += 2;
            }
            break;
        case 0xA000:
            I = nnn;
            break;
        case 0xB000:
            pc = nnn + V[0];
            break;
        case 0xC000:
            V[x] = kk & rand();
            break;
        case 0xD000: {
            V[0xF] = 0;
            uint16_t spriteTopLeft = V[y] * DISPLAY_WIDTH + V[x];
            uint8_t spriteHeight = V[y] + n < DISPLAY_HEIGHT ? n : DISPLAY_HEIGHT - V[y];
            uint8_t spriteWidth = V[x] + 8 < DISPLAY_WIDTH ? 8 : DISPLAY_WIDTH - V[x];
            for (uint8_t i = 0; i < spriteHeight; i++) {
                uint16_t pixelCoord = spriteTopLeft + i * DISPLAY_WIDTH;
                uint8_t spriteRow = memory[I + i];
                for (uint8_t bitShift = 0; bitShift < spriteWidth; bitShift++) {
                    bool pixelState = (spriteRow >> (7 - bitShift)) & 1;
                    if ((pixelState && display[pixelCoord])) {
                        display[pixelCoord] = false;
                        V[0xF] = 1;
                    } else if (pixelState || display[pixelCoord]) {
                        display[pixelCoord] = true;
                    }
                    pixelCoord++;
                }
            }
        } break;
        case 0xE000:
            if ((kk == 0x9E) && keyboardStates[keyboardMap[V[x]]]) {
                pc += 2;
            } else if ((kk == 0xA1) && !keyboardStates[keyboardMap[V[x]]]) {
                pc += 2;
            }
            break;
        case 0xF000:
            switch (kk) {
                case 0x07:
                    V[x] = delayTimer;
                    break;
                case 0x0A: {
                    SDL_Event event;
                    int8_t regValue;
                    bool validEvent = false;
                    while (!validEvent) {
                        SDL_WaitEvent(&event);
                        if (event.type == SDL_KEYDOWN) {
                            regValue = scancode_to_reg_value(event.key.keysym.scancode);
                            if (regValue != -1) {
                                V[x] = regValue;
                                validEvent = true;
                            }
                        }
                    }
                }
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
                    I = 5 * V[x];
                    break;
                case 0x33: {
                    uint8_t vx = V[x];
                    for (short i = 0; i < 3; i++) {
                        memory[I + 2 - i] = vx % 10;
                        vx /= 10;
                    }
                } break;
                case 0x55:
                    for (uint8_t i = 0; i <= x; i++) {
                        memory[I + i] = V[i];
                    }
                    break;
                case 0x65:
                    for (uint8_t i = 0; i <= x; i++) {
                        V[i] = memory[I + i];
                    }
                    break;
            }
            break;
    }
}

void update_display() {
    SDL_SetRenderDrawColor(renderer, BLACK);
    SDL_RenderClear(renderer);
    short displayArrayOffset = 0;
    for (uint16_t y = 0; y < WINDOW_HEIGHT; y += PIXEL_SIZE) {
        for (uint16_t x = 0; x < WINDOW_WIDTH; x += PIXEL_SIZE) {
            if (display[displayArrayOffset++]) {
                SDL_Rect rectangle {x, y, PIXEL_SIZE, PIXEL_SIZE};
                SDL_SetRenderDrawColor(renderer, WHITE);
                SDL_RenderFillRect(renderer, &rectangle);
            }
        }
    }
    SDL_RenderPresent(renderer);
}

void update_timers() {
    if (delayTimer) {
        delayTimer--;
    }
    if (soundTimer) {
        //TODO: Play buzzer
        soundTimer--;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        SDL_LogError(
            SDL_LOG_CATEGORY_ERROR,
            "Invalid number of arguments, please specify the rom path"
        );
        return 1;
    }
    load_rom(argv[1]);
    load_font();
    initialise_display();
    keyboardStates = SDL_GetKeyboardState(NULL);
    SDL_Event event;
    while (runningState) {
        // Hacky way to have a 720Hz proc, assuming the display is 60Hz
        for (uint8_t i = 0; i < 12; i++) {
            // This is so the keyboard states are updated before every fetch and decode cycle
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        runningState = false;
                }
            }
            fetch();
            decode_and_execute();
        }
        update_timers();
        update_display();
    }
    SDL_Quit();
    return 0;
}
