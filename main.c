/************************************************************
 * Check license.txt in project root for license information *
 *********************************************************** */

#include <stdio.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <GL/gl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "defs.h"
#include "fileload.h"
#include "cmath.h"

u8 memory[4096];
u8 VRegisters[16];

u16 IReqister;   //0x000 to 0xFFF
u16 pc;          //0x000 to 0xFFF

// 0x000-0x1FF - Chip 8 interpreter (contains font set in emu)
// 0x050-0x0A0 - Used for the built in 4x5 pixel font set (0-F)
// 0x200-0xFFF - Program ROM and work RAM
#define CHIP8_WIDTH 64
#define CHIP8_HEIGHT 32
u8 canvas[CHIP8_WIDTH * CHIP8_HEIGHT];
u8 delayTimer;
u8 soundTimer;

u16 stack[16];
u8 stackpointer;
u8 keyPressed;

u8 keypad[16]; //hex based keypad 1 - F
/*
   Keypad                   Keyboard
   +-+-+-+-+                +-+-+-+-+
   |1|2|3|C|                |1|2|3|4|
   +-+-+-+-+                +-+-+-+-+
   |4|5|6|D|                |Q|W|E|R|
   +-+-+-+-+       =>       +-+-+-+-+
   |7|8|9|E|                |A|S|D|F|
   +-+-+-+-+                +-+-+-+-+
   |A|0|B|F|                |Z|X|C|V|
   +-+-+-+-+                +-+-+-+-+
   */
const int width = CHIP8_WIDTH * 10, height = CHIP8_HEIGHT * 10;
const u16 PC_START_LOC = 0x200;

unsigned char chip8Fontset[] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void
chip8_init() {

    pc = PC_START_LOC;

    for(int i = 0; i < (int)SIZEOF_ARRAY(chip8Fontset); ++i)
        memory[/*0x050 +*/  i] = chip8Fontset[i];
}

#define REQ_VALIDATION(R) do{if(R > 0xF){printf("reqister overflow\n"); exit(1);}} while(0)
#define MEMADDR_VALIDATION(R) do{if(R > 4095){printf("memory overflow\n"); exit(1);}} while(0)
#define KEY_VALIDATION(R) do{if(R > 0xF){printf("keypad overflow\n"); exit(1);}} while(0)
#define FONT_VALIDATION(R) do{if(R >= 0xF){printf("font overflow\n"); exit(1);}} while(0)
#define STACK_VALIDATION(R) do{if(R > 15){printf("stack overflow\n"); exit(1);}} while(0)

void
chip8_load_game(char* game) {

    size_t size;
    u8* data = load_binary_file(game, &size);

    if(!data) {
        printf("%s not found\n", game);
        exit(EXIT_FAILURE);
    }
    if(size >= (SIZEOF_ARRAY(memory) - (int)PC_START_LOC)) {
        printf("Too large file!\n");
        exit(EXIT_FAILURE);
    }
    memcpy(memory + PC_START_LOC, data, size);

    free(data);
}

u8 draw;

void
chip8_cycle() {

    static int updated = 0;
    u16 opcode = memory[pc] << 8 | memory[pc + 1];
    printf ("Executing %04X at %04X , I:%02X SP:%02X UPDATE:%d V0: %d\n",
           opcode, pc, IReqister, stackpointer, updated, (int)VRegisters[0]);

    if(++updated > 100 && 0) exit(1);
    //printf ("next opcode: 0x%X\n", opcode);
    //sleep(1);

    switch(opcode & 0xF000) {
        case 0x0000: // Jumps to address NNN
            {
                switch(opcode & 0x00FF) {
                    case 0x0EE: // return from subroutine
                        {
                            stackpointer -= 1;
                            STACK_VALIDATION(stackpointer);
                            //printf("stackptr %d stack %d \n", stackpointer, stack[stackpointer]);
                            pc = stack[stackpointer];
                            pc += 2;
                        } break;
                    case 0x0E0: // display clear
                        {
                            memset(canvas,0 , sizeof(canvas));
                            draw = 1;
                            pc += 2;
                        } break;
                    default:
                        printf ("Unknown sub opcode: 0x%X\n", opcode);
                        exit(1);
                        break;
                }
                //u16 jumpAddr = opcode & 0x0FFF;
                //c = jumpAddr;

            } break;
        case 0x1000: // Jumps to address NNN
            {
                u16 jumpAddr = opcode & 0x0FFF;
                MEMADDR_VALIDATION(jumpAddr);
                pc = jumpAddr;
            } break;
        case 0x2000: // Calls subroutine at NNN
            {
                u16 addr = opcode & 0x0FFF;
                MEMADDR_VALIDATION(addr);
                stack[stackpointer] = pc;
                stackpointer += 1;
                STACK_VALIDATION(stackpointer);
                pc = addr;
            } break;
        case 0x3000:    // Skips the next instruction if VX equals NN.
            {
                u8 Vreq = (opcode & 0x0F00) >> 8;
                REQ_VALIDATION(Vreq);
                u16 cond = opcode & 0x00FF;
                if(VRegisters[Vreq] == cond) {
                    pc += 2;
                }
                pc += 2;

            } break;
        case 0x4000: // Skips the next instruction if VX does not equals NN.
            {
                u8 Vreq = (opcode & 0x0F00) >> 8;
                REQ_VALIDATION(Vreq);
                u16 cond = opcode & 0x00FF;
                if(VRegisters[Vreq] != cond) {
                    pc += 2;
                }
                pc += 2;

            } break;
        case 0x5000: // Skips the next instruction if VX equals VY.
            {
                u8 VXreq = (opcode & 0x0F00) >> 8;
                u8 VYreq = (opcode & 0x00F0) >> 4;
                REQ_VALIDATION(VXreq);
                REQ_VALIDATION(VYreq);
                if(VRegisters[VXreq] == VRegisters[VYreq]) {
                    pc += 2;
                }
                pc += 2;
            } break;

        case 0x6000: //Sets VX to NN.
            {
                u8 Vreq = (opcode & 0x0F00) >> 8;
                REQ_VALIDATION(Vreq);
                VRegisters[Vreq] = opcode & 0x00FF;
                pc += 2;
            } break;
        case 0x7000: // Adds NN to VX. (Carry flag is not changed)
            {
                u8 Vreq = (opcode & 0x0F00) >> 8;
                u8 val = opcode & 0x00FF;
                REQ_VALIDATION(Vreq);
                VRegisters[Vreq] += val;
                pc += 2;
            } break;
        case 0x8000:
            {
                u8 subOpCode = (opcode & 0x000F);

                u8 VXreq = (opcode & 0x0F00) >> 8;
                u8 VYreq = (opcode & 0x00F0) >> 4;
                REQ_VALIDATION(VXreq);
                REQ_VALIDATION(VYreq);

                switch(subOpCode) {
                    case 0x0: // Sets VX to the value of VY.
                        {
                            VRegisters[VXreq] = VRegisters[VYreq];
                        } break;
                    case 0x1: // Bitwise OR operation
                        {
                            VRegisters[VXreq] = VRegisters[VXreq] | VRegisters[VYreq];
                        } break;
                    case 0x2: // Bitwise AND operation
                        {
                            VRegisters[VXreq] = VRegisters[VXreq] & VRegisters[VYreq];
                        } break;
                    case 0x3: // Sets VX to VX xor VY.
                        {
                            VRegisters[VXreq] ^= VRegisters[VYreq];
                        } break;
                    case 0x4: // Adds VY to VX. VF is set to 1 when there's a carry,
                        // and to 0 when there isn't.
                        {
                            //VRegisters[VXreq] += VRegisters[VYreq];
                            VRegisters[0xF] = 0;
                            if(VRegisters[VYreq] > (0xFF - VRegisters[VXreq])) {
                                // Overflow
                                VRegisters[0xF] = 1;
                            }
                            VRegisters[VXreq] += VRegisters[VYreq];

                        } break;
                    case 5: // VY is subtracted from VX. VF is set to 0 when there's a borrow,
                        // and 1 when there isn't.
                        {
                            VRegisters[0xF] = 1;
                            if(VRegisters[VXreq] < VRegisters[VYreq]) {
                                // Underflow
                                VRegisters[0xF] = 0;
                            }
                            VRegisters[VXreq] -= VRegisters[VYreq];
                        } break;
                    case 6: // Stores the least significant bit of VX in VF
                        // and then shifts VX to the right by 1
                        {
                            u8 lsb = VRegisters[VXreq] & 0x01;
                            VRegisters[0xF] = lsb;
                            VRegisters[VXreq] >>= 1;
                            printf("sdadsadadas\n");
                            //getchar();
                        } break;
                    case 7: // Sets VX to VY minus VX. VF is set to 0 when there's a borrow,
                        // and 1 when there isn't
                        {
                            // v[0xF] = *vxptr < *vyptr ? 1 : 0; // Reverse of the other SUB TODO
                            VRegisters[0xF] = 1;
                            if((int)VRegisters[VYreq] < (int)VRegisters[VXreq]) {
                                // Underflow;
                                VRegisters[0xF] = 0;
                            }
                            VRegisters[VXreq] = VRegisters[VYreq] - VRegisters[VXreq];
                            printf("DJSAKLDJALSKDJLAS\n");
                            //getchar();
                        } break;
                    case 0x0E: // Stores the most significant bit of VX in VF
                        // and then shifts VX to the left by 1
                        {
                            //u8 msb = VRegisters[VXreq] & 0x80;
                            u8 msb = VRegisters[VXreq] >> 7;
                            VRegisters[0xF] = msb;
                            VRegisters[VXreq] <<= 1;
                        } break;
                    default:
                        printf ("Unknown sub opcode: 0x%X\n", opcode);
                        exit(1);
                        break;
                }
                pc += 2;
            } break;
        case 0x9000: //Skips the next instruction if VX doesn't equal VY
            {
                u8 VXreq = (opcode & 0x0F00) >> 8;
                u8 VYreq = (opcode & 0x00F0) >> 4;
                REQ_VALIDATION(VXreq);
                REQ_VALIDATION(VYreq);

                if(VRegisters[VXreq] != VRegisters[VYreq]) {
                    pc += 2;
                }
                pc += 2;
            } break;
        case 0xA000: // Sets I to the address NNN
            {
                u16 addr = (opcode & 0x0FFF);
                MEMADDR_VALIDATION(addr);
                IReqister = addr;
                pc += 2;
            } break;
        case 0xB000: // Jumps to the address NNN plus V0
            {
                u16 addr = (opcode & 0x0FFF) + (u16)VRegisters[0];
                MEMADDR_VALIDATION(addr);
                pc = addr;
            } break;
        case 0xC000: // Sets VX to the result of a bitwise
            // and operation on a random number (Typically: 0 to 255) and NN.
            {
                u8 Vreq = (opcode & 0x0F00) >> 8;
                u8 NN = (opcode & 0x00FF);
                REQ_VALIDATION(Vreq);

                //V[(opcode & 0x0F00) >> 8] = (rand() % (0xFF + 1)) & (opcode & 0x00FF);
                VRegisters[Vreq] =          (rand() % (0xFF + 1)) & NN;
                pc += 2;
            } break;
        case 0xD000:
            // Draws a sprite at coordinate (VX, VY)
            // that has a width of 8 pixels and a height of N pixels.
            // Each row of 8 pixels is read as bit-coded starting from memory location I;
            // I value doesn’t change after the execution of this instruction.
            // As described above,
            // VF is set to 1 if any screen pixels are flipped from set to unset when the sprite is drawn,
            // and to 0 if that doesn’t happen
            {
                draw = 1;
                u8 VXreq = (opcode & 0x0F00) >> 8;
                u8 VYreq = (opcode & 0x00F0) >> 4;
                u8 N = (opcode & 0x000F);
                REQ_VALIDATION(VXreq);
                REQ_VALIDATION(VYreq);

                u8 startX = VRegisters[VXreq];
                u8 startY = VRegisters[VYreq];

                //printf("startx %d, starty %d n %d\n", (int)startX, (int)startY, (int)N);
                VRegisters[0xF] = 0;

                for(u8 y = startY, i = 0; y < (startY + N); y++, i++) {
                    u8 row = memory[IReqister + i];
                    //printf("pixel! %d \n", row);
                    for(u8 x = startX, i2 = 0; i2 < 8; x++, i2++) {

                        if( (row & (0x80 >> i2)) != 0 ) { // check if sprite has pixel set
                            if(canvas[y * CHIP8_WIDTH + x] == 1) { // bit already set
                                VRegisters[0xF] = 1;
                            }
                            canvas[y * CHIP8_WIDTH + x] ^= 0x1;
                            //printf("drawing to %d new value %d\n", y * CHIP8_WIDTH + x, canvas[y * CHIP8_WIDTH + x]);
                        }
                    }
                }
                pc += 2;
            } break;
        case 0xE000:
            {
                u8 Vreq = (opcode & 0x0F00) >> 8;
                REQ_VALIDATION(Vreq);
                u8 key = VRegisters[Vreq];
                KEY_VALIDATION(key);
                u8 keypadKey = keypad[key]; // 1 or 0
                switch(opcode & 0x0FF) {
                    case 0x09E: // Skips the next instruction if the key stored in VX is pressed
                        {
                            if(keypadKey) {
                                pc += 2;
                            }
                        } break;
                    case 0x0A1: // Skips the next instruction if the key stored in VX isn't pressed
                        {
                            if(!keypadKey) {
                                pc += 2;
                            }
                        } break;
                    default:
                        printf ("Unknown sub opcode: 0x%X\n", opcode);
                        exit(1);
                        break;
                }
                pc += 2;
            } break;
        case 0xF000:
            {
                u8 Vreq = (opcode & 0x0F00) >> 8;
                REQ_VALIDATION(Vreq);
                switch(opcode & 0x00FF) {
                    case 0x0007: // Sets VX to the value of the delay timer
                        {
                            VRegisters[Vreq] = delayTimer;
                            pc += 2;
                        } break;
                    case 0x000A: // A key press is awaited, and then stored in VX.
                        // (Blocking Operation. All instruction halted until next key event)
                        {
                            if(keyPressed == 0) {
                                printf("waiting for next key event!\n");
                                break;
                            }
                            pc += 2;
                        } break;
                    case 0x0015: // Sets the delay timer to VX.
                        {
                            delayTimer = VRegisters[Vreq];
                            pc += 2;
                        } break;
                    case 0x0018: // Sets the sound timer to VX
                        {
                            soundTimer = VRegisters[Vreq];
                            pc += 2;
                        } break;
                    case 0x001E: // Adds VX to I. VF is set to 1
                        // when there is a range overflow (I+VX>0xFFF), and to 0 when there isn't.
                        {
                            VRegisters[0xF] = 0;
                            IReqister += VRegisters[Vreq];
                            if(IReqister > 0xFFF) { //12 bit wide in chip8
                                //IReqister -= 0xFFF + 1; //(-1 rolls to 0)
                                VRegisters[0xF] = 1;
                            }
                            pc += 2;
                        } break;
                    case 0x0029:
                        // Sets I to the location of the sprite for the character in VX.
                        // Characters 0-F (in hexadecimal) are represented by a 4x5 font.
                        {
                            u8 font = VRegisters[Vreq];
                            printf("font %d req %d\n",(int)font, Vreq);
                            FONT_VALIDATION(font);
                            IReqister = /*0x050 +*/  font * 5;
                            pc += 2;
                            //getchar();
                        } break;
                    case 0x0033: //  Stores the binary-coded decimal representation of VX,
                        // with the most significant of three digits at the address in I,
                        // the middle digit at I plus 1, and the least significant digit at I plus 2.
                        // (In other words, take the decimal representation of VX,
                        // place the hundreds digit in memory at location in I,
                        // the tens digit at location I+1, and the ones digit at location I+2.)
                        {

                            MEMADDR_VALIDATION(IReqister + 2);
                            //printf("**************  Ireq %d\n", IReqister);
                            memory[IReqister]     = VRegisters[Vreq] / 100;
                            memory[IReqister + 1] = (VRegisters[Vreq] / 10) % 10;
                            memory[IReqister + 2] = VRegisters[Vreq] % 10;
                            pc += 2;
                        } break;

                    case 0x0055: // Stores V0 to VX (including VX) in memory starting at address I.
                        // The offset from I is increased by 1 for each value written,
                        // but I itself is left unmodified
                        {
                            u8 x = (opcode & 0x0F00) >> 8;// VRegisters[Vreq];
                            MEMADDR_VALIDATION(IReqister + x);
                            //REQ_VALIDATION(x);
                            for(u8 i = 0; i <= x; i++) {
                                printf("i %d IR %d vreq %d\n",i, IReqister, Vreq);
                                memory[IReqister + i] = VRegisters[i];
                            }
                            pc += 2;
                        } break;
                    case 0x0065: // Fills V0 to VX (including VX) with values from memory
                        // starting at address I.
                        // The offset from I is increased by 1 for each value written,
                        // but I itself is left unmodified.[d]
                        {
                            u8 x = (opcode & 0x0F00) >> 8;// VRegisters[Vreq];
                            //u8 x = VRegisters[Vreq];
                            //printf("**************  x %d\n", x);
                            //printf("**************  req %d\n", Vreq);
                            //printf("**************  I req %d\n", IReqister);
                            for(u8 i = 0; i <= x; i++) {
                                REQ_VALIDATION(i);
                                MEMADDR_VALIDATION(IReqister + i);
                                VRegisters[i] =  memory[IReqister + i];
                            }
                            pc += 2;
                        } break;
                    default:
                        printf ("Unknown sub opcode: 0x%X\n", opcode);
                        exit(1);
                        break;
                }
            } break;
        default:
            printf ("Unknown opcode: 0x%X\n", opcode);
            exit(1);
            break;
    }

    keyPressed = 0;
}

i32 running = 1;

#define KEYMAP(FN) \
    FN('1', 0x1)\
FN('2', 0x2)\
FN('3', 0x3)\
FN('4', 0x4)\
FN('q', 0x5)\
FN('w', 0x6)\
FN('e', 0x7)\
FN('r', 0x8)\
FN('a', 0x9)\
FN('s', 0xA)\
FN('d', 0xB)\
FN('f', 0xC)\
FN('z', 0xD)\
FN('x', 0xE)\
FN('c', 0xF)\
FN('v', 0x10)

//printf("pressed %c %s\n", KEY, event.type == SDL_KEYDOWN ? "down" : "up");

#define KEY_BIND(KEY, CODE) \
    case KEY: \
keypad[CODE] = event.type == SDL_KEYDOWN; \
keyPressed = event.type == SDL_KEYDOWN; \
break;

void
update_keypad() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if(event.key.repeat == 1) {
            continue;
        }
        switch (event.key.keysym.sym) {
            KEYMAP(KEY_BIND);
            case SDLK_ESCAPE:
            running = 0;
            break;
            default:
            break;
        }
        if (event.type == SDL_QUIT) {
            running = 0;
        }
    }
}

GLenum
glCheckError_(const char *file, int line) {
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        unsigned char* error = NULL;
        switch (errorCode) {
            case GL_INVALID_ENUM:                  error = (unsigned char*)"INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 error = (unsigned char*)"INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             error = (unsigned char*)"INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:                 error = (unsigned char*)"OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = (unsigned char*)"INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        //FATALERRORMESSAGE("GL ERROR %s \n", error);
        printf("GL ERROR %s (file %s, line %d)\n", error, file, line);
        exit(1);
    }
    return errorCode;
}

#define gl_check_error() glCheckError_(__FILE__, __LINE__)
#define GLCHECK(FUN) do{FUN; glCheckError_(__FILE__, __LINE__); } while(0)

u32
shader_compile(GLenum type, const char* source) {
    i32 compiledcheck;

    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        printf("Failed to create shader\n");
    }

    GLCHECK(glShaderSource(shader, 1, &source, NULL));
    GLCHECK(glCompileShader(shader));

    GLCHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &compiledcheck));
    if (!compiledcheck)
    {
        i32 infoLen = 0;
        GLCHECK(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen));
        if (infoLen > 1)
        {
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);
            GLCHECK(glGetShaderInfoLog(shader, infoLen, NULL, infoLog));
            printf("Error compiling shader :\n%s\n", infoLog);
            free(infoLog);
        }
        GLCHECK(glDeleteShader(shader));
        exit(1);
    }
    gl_check_error();
    return shader;
}



u32 shaderProgram;
u32 vao;
i32 transformLoc;
i32 projectionLoc;
mat4 projection;

void
renderer_init() {

    size_t size = 0;
    char* vs = load_file("vert.sha", &size);
    if(!vs) {
        printf("failed to load vert.sha\n");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", vs);

    GLuint vert = shader_compile(GL_VERTEX_SHADER, vs);
    free(vs);

    char* fs = load_file("frag.sha", &size);
    if(!fs) {
        printf("failed to load vert.sha\n");
        exit(EXIT_FAILURE);
    }
    printf("%s\n", fs);

    GLuint frag = shader_compile(GL_FRAGMENT_SHADER, fs);
    free(fs);
    shaderProgram = glCreateProgram();

    GLCHECK(glAttachShader(shaderProgram, vert));
    GLCHECK(glAttachShader(shaderProgram, frag));

    GLCHECK(glBindAttribLocation(shaderProgram, 0, "vertexPosition"));
    GLCHECK(glLinkProgram(shaderProgram));

    i32 linked;
    GLCHECK(glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked));

    if (!linked)
    {
        i32 infoLen = 0;
        GLCHECK(glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &infoLen));

        if (infoLen > 1)
        {
            char* infoLog = (char*)malloc(sizeof(char) * infoLen);
            GLCHECK(glGetProgramInfoLog(shaderProgram, infoLen, NULL, infoLog));
            printf("Error linking program:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteProgram(shaderProgram);
        exit(1);
    }

    transformLoc = glGetUniformLocation(shaderProgram, "transform");
    if(transformLoc == -1) {
        printf("didnt find transform location\n");
        exit(1);
    }

    projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    if(projectionLoc == -1) {
        printf("didnt find projection location\n");
        exit(1);
    }

    GLCHECK(glDeleteShader(vert));
    GLCHECK(glDeleteShader(frag));

    static const float vertData[] = {
        -0.5f,  0.5f,
        0.5f, -0.5f,
        -0.5f, -0.5f,

        -0.5f,  0.5f,
        0.5f, -0.5f,
        0.5f,  0.5f,
    };


    GLCHECK(glGenVertexArrays(1, &vao));

    u32 vertbuff;
    GLCHECK(glGenBuffers(1, &vertbuff));

    GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, vertbuff));
    //u32 ssss = sizeof(vertData);
    GLCHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(vertData), vertData, GL_STATIC_DRAW));

    GLCHECK(glBindVertexArray(vao));

    // position attribute
    GLCHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0));
    GLCHECK(glEnableVertexAttribArray(0));

    GLCHECK(glBindVertexArray(0));

    GLCHECK(glEnable(GL_DEPTH_TEST));
    GLCHECK(glDepthMask(GL_FALSE));

    //orthomat(&projection, 0.5f, (float)CHIP8_WIDTH + 0.5f, (float)CHIP8_HEIGHT + 0.5f, 0.5f, 0.1f, 100.f);
    //orthomat(&projection, 0, (float)CHIP8_WIDTH, 0, (float)CHIP8_HEIGHT, 0.1f, 100.f);
    orthomat(&projection, 0.5f,
            (float)CHIP8_WIDTH + 0.5f - 1.f,
            (float)CHIP8_HEIGHT + 0.5f - 1.f,
            0.5f, 0.1f, 100.f);

#if 0
    mat4 transform;

    create_translation_mat_inside(&transform, (vec3){0, 0, 0});

    vec4 pos = mat4_mult_vec4(&transform, (vec4){ 0.5 , 0.5 ,0 , 1});

    pos = mat4_mult_vec4(&transform, pos);

    printf("rerere\n");

    pos = mat4_mult_vec4(&projection, pos);

    printf("rerere\n");


    //identify_mat4(&projection);

    // from left top corner

    //create_scaling_mat4(&projection, (vec3) { 1.f/ (float)CHIP8_WIDTH, 1.f/ (float)CHIP8_HEIGHT, 0});
    //translate_mat4(&projection, (vec3) {-1, -1, 0});

    vec4 pos = mat4_mult_vec4(&projection, (vec4){0,0,0,1});

    printf("rerere\n");

    pos = mat4_mult_vec4(&projection, (vec4){1,1,0,1});

    printf("rerere\n");

    pos = mat4_mult_vec4(&projection, (vec4){CHIP8_WIDTH + 0.5, CHIP8_HEIGHT + 0.5,0,1});

    printf("rerere\n");

    pos = mat4_mult_vec4(&projection, (vec4){ 0.5f , 0.5f ,0,1});

    printf("rerere\n");

#endif
#if 0
    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::rotate(trans, glm::radians(90.0f), glm::vec3(0.0, 0.0, 1.0));
    trans = glm::scale(trans, glm::vec3(0.5, 0.5, 0.5));
#endif
}



void
chip8_draw(SDL_Window *window) {

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glViewport(0, 0, width, height);
    glClearColor(1.f, 0.f, 1.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    // Draw all
    GLCHECK(glUseProgram(shaderProgram));
    GLCHECK(glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, (float*)&projection));
    mat4 transform;
    for(int y = 0; y < CHIP8_HEIGHT; y++) {
        for(int x = 0; x < CHIP8_WIDTH; x++) {
            //if((x + y) % 2) {
            //    canvas[y * CHIP8_WIDTH + x] = 1;
            //}
            if(canvas[y * CHIP8_WIDTH + x] == 1) { // draw
                create_translation_mat_inside(&transform, (vec3){x,y,0});
                GLCHECK(glUniformMatrix4fv(transformLoc, 1, GL_FALSE, (float*)&transform));

                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }


    SDL_GL_SwapWindow(window);
}

int
main(int argc, char** argv) {

    if(argc != 2) {
        printf("specify game\n");
        //return EXIT_FAILURE;
    }

    SDL_Init( SDL_INIT_VIDEO );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
    SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );

    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );

    if(!SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1")) {
        printf("not set!\n");
    }

    SDL_Window *window = SDL_CreateWindow("Chip8",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    assert(window);
    SDL_GLContext Context = SDL_GL_CreateContext(window);

    renderer_init();

    running = 1;
    // Init rand
    time_t t;
    //srand((unsigned) time(&t));

    chip8_init();

    if(argc != 2) {
        chip8_load_game("c8games/PONG");
    } else {
        chip8_load_game(argv[1]);
    }
    double processorHZ = 1.0 / 100.0;
    double timerHZ = 1.0 / 50.0;
    double processorLastTime = (double)clock() / CLOCKS_PER_SEC;
    double timerLastTime = processorLastTime;
    while (running) {
        double currentTime = (double)clock() / CLOCKS_PER_SEC;
        //printf("%f\n", currentTime - processorLastTime);

        if( (currentTime - processorLastTime) > processorHZ) {

            processorLastTime = currentTime;
            chip8_cycle();

            if(draw) {
                draw = 0;
                //printf("drawing!\n");
                //exit(1);
                chip8_draw(window);
            }

        }

        if( (currentTime - timerLastTime) > timerHZ) {

            timerLastTime =  currentTime;
            //printf("timer update!\n");

            if(delayTimer > 0)
                delayTimer -= 1;

            if(soundTimer > 0)
                soundTimer -= 1;
        }

        update_keypad();
    }



    return EXIT_SUCCESS;
}
