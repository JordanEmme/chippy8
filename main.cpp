#include "SDL.h"
#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_log.h"
#include "SDL_render.h"
#include "SDL_scancode.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <linux/limits.h>
#include <unistd.h>

#define WHITE 255, 255, 255, 255
#define BLACK 0, 0, 0, 255

constexpr uint8_t DISPLAY_WIDTH = 64;
constexpr uint8_t DISPLAY_HEIGHT = 32;
constexpr uint16_t DISPLAY_LEN = DISPLAY_WIDTH * DISPLAY_HEIGHT;
constexpr uint8_t PIXEL_SIZE = 16;
constexpr uint16_t WINDOW_WIDTH = DISPLAY_WIDTH * PIXEL_SIZE;
constexpr uint16_t WINDOW_HEIGHT = DISPLAY_HEIGHT * PIXEL_SIZE;
constexpr uint16_t ROM_START_ADDRESS = 0x200;
constexpr uint16_t MEM_SIZE = 4096;
constexpr uint16_t CLOCK_SPEED = 700;  //in Hz

constexpr uint8_t font[80] {
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

constexpr SDL_Scancode keyboardMap[16] {
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

struct {
    uint16_t displayRefreshRate;
    SDL_Window* window;
    SDL_Renderer* renderer;
    bool runningState = true;
    const Uint8* keyboardStates;
} app;

struct {
    uint8_t memory[MEM_SIZE];
    bool display[DISPLAY_LEN];
    uint16_t stack[16];
    uint8_t V[16];
    uint16_t pc;
    uint16_t I;
    uint16_t sp;
    uint16_t opcode;
    uint8_t soundTimer = 60;
    uint8_t delayTimer;
} chip8;

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
    memcpy(chip8.memory, font, sizeof(font));
}

void load_rom(const char* cliArg) {
    std::filesystem::path userRomPath = std::filesystem::path {cliArg};

    const std::filesystem::path& absoluteRomPath =
        userRomPath.is_absolute() ? userRomPath : std::filesystem::current_path() / userRomPath;

    std::ifstream file(absoluteRomPath, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        std::streampos size = file.tellg();

        file.seekg(0, std::ios::beg);
        if (ROM_START_ADDRESS + size >= MEM_SIZE) {
            return;
        }
        file.read((char*)(chip8.memory + ROM_START_ADDRESS), size);
        file.close();

        chip8.pc = ROM_START_ADDRESS;
    }
}

void initialise_display() {
    SDL_Init(SDL_INIT_VIDEO);
    app.window = SDL_CreateWindow("Chippy8", 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    app.renderer =
        SDL_CreateRenderer(app.window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    SDL_DisplayMode mode;
    SDL_GetCurrentDisplayMode(0, &mode);
    app.displayRefreshRate = mode.refresh_rate;
}

uint16_t get_opcode_at_pc() {
    uint8_t opcode1 = chip8.memory[chip8.pc];
    uint8_t opcode2 = chip8.memory[chip8.pc + 1];
    return (opcode1 << 8) | opcode2;
}

void fetch() {
    chip8.opcode = get_opcode_at_pc();
    chip8.pc += 2;
}

void decode_and_execute() {
    uint8_t x = (chip8.opcode & 0x0F00) >> 8;
    uint8_t y = (chip8.opcode & 0x00F0) >> 4;

    uint8_t n = chip8.opcode & 0x000F;
    uint8_t kk = chip8.opcode & 0x00FF;
    uint16_t nnn = chip8.opcode & 0x0FFF;

    uint8_t vf;

    switch (chip8.opcode & 0xF000) {
        case 0x0000:
            if (chip8.opcode == 0x00E0) {
                // clear display
                memset(chip8.display, 0, DISPLAY_LEN);
            } else if (chip8.opcode == 0x00EE) {
                // return from subroutine
                chip8.pc = chip8.stack[chip8.sp--];
            }
            break;
        case 0x1000:
            chip8.pc = nnn;
            break;
        case 0x2000:
            // call subroutine at chip8.opcode & 0x0FFF
            chip8.stack[++chip8.sp] = chip8.pc;
            chip8.pc = nnn;
            break;
        case 0x3000:
            if (chip8.V[x] == kk) {
                chip8.pc += 2;
            }
            break;
        case 0x4000:
            if (chip8.V[x] != kk) {
                chip8.pc += 2;
            }
            break;
        case 0x5000:
            if (chip8.V[x] == chip8.V[y]) {
                chip8.pc += 2;
            }
            break;
        case 0x6000:
            chip8.V[x] = kk;
            break;
        case 0x7000:
            chip8.V[x] += kk;
            break;
        case 0x8000:
            switch (n) {
                case 0x0:
                    chip8.V[x] = chip8.V[y];
                    break;
                case 0x1:
                    chip8.V[x] = chip8.V[x] | chip8.V[y];
                    break;
                case 0x2:
                    chip8.V[x] = chip8.V[x] & chip8.V[y];
                    break;
                case 0x3:
                    chip8.V[x] = chip8.V[x] ^ chip8.V[y];
                    break;
                case 0x4:
                    if (chip8.V[y] > 255 - chip8.V[x]) {
                        vf = 1;

                    } else {
                        vf = 0;
                    }
                    chip8.V[x] += chip8.V[y];
                    chip8.V[0xF] = vf;
                    break;
                case 0x5:
                    vf = chip8.V[x] >= chip8.V[y];
                    chip8.V[x] -= chip8.V[y];
                    chip8.V[0xF] = vf;
                    break;
                case 0x6:
                    vf = (chip8.V[x] & 1);
                    chip8.V[x] = chip8.V[x] >> 1;
                    chip8.V[0xF] = vf;
                    break;
                case 0x7:
                    vf = chip8.V[y] >= chip8.V[x];
                    chip8.V[x] = chip8.V[y] - chip8.V[x];
                    chip8.V[0xF] = vf;
                    break;
                case 0xE:
                    vf = (chip8.V[x] & 0x80) >> 7;
                    chip8.V[x] = chip8.V[x] << 1;
                    chip8.V[0xF] = vf;
                    break;
            }
            break;
        case 0x9000:
            if (chip8.V[x] != chip8.V[y]) {
                chip8.pc += 2;
            }
            break;
        case 0xA000:
            chip8.I = nnn;
            break;
        case 0xB000:
            chip8.pc = nnn + chip8.V[0];
            break;
        case 0xC000:
            chip8.V[x] = kk & rand();
            break;
        case 0xD000: {
            chip8.V[0xF] = 0;
            uint16_t spriteTopLeft = chip8.V[y] * DISPLAY_WIDTH + chip8.V[x];
            uint8_t spriteHeight =
                chip8.V[y] + n < DISPLAY_HEIGHT ? n : DISPLAY_HEIGHT - chip8.V[y];
            uint8_t spriteWidth = chip8.V[x] + 8 < DISPLAY_WIDTH ? 8 : DISPLAY_WIDTH - chip8.V[x];
            for (uint8_t i = 0; i < spriteHeight; i++) {
                uint16_t pixelCoord = spriteTopLeft + i * DISPLAY_WIDTH;
                uint8_t spriteRow = chip8.memory[chip8.I + i];
                for (uint8_t bitShift = 0; bitShift < spriteWidth; bitShift++) {
                    bool pixelState = (spriteRow >> (7 - bitShift)) & 1;
                    if ((pixelState && chip8.display[pixelCoord])) {
                        chip8.display[pixelCoord] = false;
                        chip8.V[0xF] = 1;
                    } else if (pixelState || chip8.display[pixelCoord]) {
                        chip8.display[pixelCoord] = true;
                    }
                    pixelCoord++;
                }
            }
        } break;
        case 0xE000:
            if ((kk == 0x9E) && app.keyboardStates[keyboardMap[chip8.V[x]]]) {
                chip8.pc += 2;
            } else if ((kk == 0xA1) && !app.keyboardStates[keyboardMap[chip8.V[x]]]) {
                chip8.pc += 2;
            }
            break;
        case 0xF000:
            switch (kk) {
                case 0x07:
                    chip8.V[x] = chip8.delayTimer;
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
                                chip8.V[x] = regValue;
                                validEvent = true;
                            }
                        }
                    }
                }
                case 0x15:
                    chip8.delayTimer = chip8.V[x];
                    break;
                case 0x18:
                    chip8.soundTimer = chip8.V[x];
                    break;
                case 0x1E:
                    chip8.I += chip8.V[x];
                    break;
                case 0x29:
                    chip8.I = 5 * chip8.V[x];
                    break;
                case 0x33: {
                    uint8_t vx = chip8.V[x];
                    for (short i = 0; i < 3; i++) {
                        chip8.memory[chip8.I + 2 - i] = vx % 10;
                        vx /= 10;
                    }
                } break;
                case 0x55:
                    for (uint8_t i = 0; i <= x; i++) {
                        chip8.memory[chip8.I + i] = chip8.V[i];
                    }
                    break;
                case 0x65:
                    for (uint8_t i = 0; i <= x; i++) {
                        chip8.V[i] = chip8.memory[chip8.I + i];
                    }
                    break;
            }
            break;
    }
}

void update_display() {
    SDL_SetRenderDrawColor(app.renderer, BLACK);
    SDL_RenderClear(app.renderer);
    short displayArrayOffset = 0;
    for (uint16_t y = 0; y < WINDOW_HEIGHT; y += PIXEL_SIZE) {
        for (uint16_t x = 0; x < WINDOW_WIDTH; x += PIXEL_SIZE) {
            if (chip8.display[displayArrayOffset++]) {
                SDL_Rect rectangle {x, y, PIXEL_SIZE, PIXEL_SIZE};
                SDL_SetRenderDrawColor(app.renderer, WHITE);
                SDL_RenderFillRect(app.renderer, &rectangle);
            }
        }
    }
    SDL_RenderPresent(app.renderer);
}

void update_timers() {
    if (chip8.delayTimer) {
        chip8.delayTimer--;
    }
    if (chip8.soundTimer) {
        //TODO: Play buzzer
        chip8.soundTimer--;
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
    app.keyboardStates = SDL_GetKeyboardState(NULL);
    SDL_Event event;
    uint8_t cyclePerFrame = CLOCK_SPEED / app.displayRefreshRate;

    while (app.runningState) {
        for (uint8_t i = 0; i < cyclePerFrame; i++) {
            // This is so the keyboard states are updated before every fetch and decode cycle
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        app.runningState = false;
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
