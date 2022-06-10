/*
 * LC-3 emulator from https://justinmeiners.github.io/lc3-vm/
 */
#include <signal.h>
#import os/term
#import os/misc

/**
 * 10 registers with their keys.
 */
uint16_t reg[10] = {0};
enum {
    /*
     * General purpose registers
     */
    R_R0 = 0, R_R1, R_R2, R_R3, R_R4, R_R5, R_R6, R_R7,
    /*
     * Program counter
     */
    R_PC,
    /*
     * Condition flags
     */
    R_COND
};

/**
 * Possible Condition flags for the condition register.
*/
enum {
    FL_POS = 1,
    FL_ZRO = 2,
    FL_NEG = 4
};

/**
 * Memory is a contiguous block of 65536 16-bit values.
 */
#define MEMORY_MAX 1 << 16
uint16_t memory[MEMORY_MAX] = {0}; /* 65536 locations */

/**
 * The memory has padding of 3000 cells reserved for platform code by the spec.
 */
const size_t PC_START = 0x3000;


enum {
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};
enum
{
    OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};

term_t *term = NULL;
bool running = true;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("lc3 [image-file1] ...\n");
        return 1;
    }

    // Read the image.
    FILE* file = fopen(argv[1], "rb");
    if (!file) { 
        fprintf(stderr, "failed to load image\n");
        return 1;
    }
    /* the origin tells us where in memory to place the image */
    uint16_t origin = read_16(file);
    for (uint32_t i = origin; i < MEMORY_MAX; i++) {
        memory[i] = read_16(file);
        if (feof(file) || ferror(file)) {
            break;
        }
    }
    if (!feof(file) && !ferror(file)) {
        fprintf(stderr, "input file too long\n");
        return 1;
    }
    fclose(file);

    signal(SIGINT, handle_interrupt);

    term = term_get_stdin();
    term_disable_input_buffering(term);

    reg[R_COND] = FL_ZRO;
    reg[R_PC] = PC_START;

    while (running) {
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;
        switch (op) {
            case OP_ADD: op_add(instr); break;
            case OP_AND: op_and(instr); break;
            case OP_NOT: op_not(instr); break;
            case OP_BR: op_br(instr); break;
            case OP_JMP: op_jmp(instr); break;
            case OP_JSR: op_jsr(instr); break;
            case OP_LD: op_ld(instr); break;
            case OP_LDI: op_ldi(instr); break;
            case OP_LDR: op_ldr(instr); break;
            case OP_LEA: op_lea(instr); break;
            case OP_ST: op_st(instr); break;
            case OP_STI: op_sti(instr); break;
            case OP_STR: op_str(instr); break;
            case OP_TRAP: op_trap(instr); break;
            case OP_RES:
            case OP_RTI:
            default:
                abort();
                break;
        }
    }
    
    term_restore(term);
}


/**
 * Reads a memory block from the given image file. The file has blocks laid
 * out in a big-endian way.
 */
uint16_t read_16(FILE *file) {
    uint8_t a = 0, b = 0;
    fread(&a, sizeof(uint8_t), 1, file);
    fread(&b, sizeof(uint8_t), 1, file);
    return a * 256 + b;
}

enum
{
    MR_KBSR = 0xFE00, /* keyboard status */
    MR_KBDR = 0xFE02  /* keyboard data */
};

void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
}

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if (stdin_has_input())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        }
        else
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}


void handle_interrupt(int signal) {
    term_restore(term);
    printf("exit on signal %d\n", signal);
    exit(-2);
}

void op_add(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;
    if (imm_flag) {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] + imm5;
    } else {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] + reg[r2];
    }
    update_flags(r0);
}

void op_and(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;
    if (imm_flag) {
        uint16_t imm5 = sign_extend(instr & 0x1F, 5);
        reg[r0] = reg[r1] & imm5;
    } else {
        uint16_t r2 = instr & 0x7;
        reg[r0] = reg[r1] & reg[r2];
    }
    update_flags(r0);
}

void op_not(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    reg[r0] = ~reg[r1];
    update_flags(r0);
}

void op_br(uint16_t instr) {
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    uint16_t cond_flag = (instr >> 9) & 0x7;
    if (cond_flag & reg[R_COND]) {
        reg[R_PC] += pc_offset;
    }
}

void op_jmp(uint16_t instr) {
    /* Also handles RET */
    uint16_t r1 = (instr >> 6) & 0x7;
    reg[R_PC] = reg[r1];
}

void op_jsr(uint16_t instr) {
    uint16_t long_flag = (instr >> 11) & 1;
    reg[R_R7] = reg[R_PC];
    if (long_flag) {
        uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
        reg[R_PC] += long_pc_offset;  /* JSR */
    } else {
        uint16_t r1 = (instr >> 6) & 0x7;
        reg[R_PC] = reg[r1]; /* JSRR */
    }
}

void op_ld(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = mem_read(reg[R_PC] + pc_offset);
    update_flags(r0);
}

void op_ldi(uint16_t instr) {
    /* destination register (DR) */
    uint16_t r0 = (instr >> 9) & 0x7;
    /* PCoffset 9*/
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    /* add pc_offset to the current PC, look at that memory location to get the final address */
    reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
    update_flags(r0);
}

void op_ldr(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    reg[r0] = mem_read(reg[r1] + offset);
    update_flags(r0);
}

void op_lea(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    reg[r0] = reg[R_PC] + pc_offset;
    update_flags(r0);
}

void op_st(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(reg[R_PC] + pc_offset, reg[r0]);
}

void op_sti(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
}

void op_str(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    mem_write(reg[r1] + offset, reg[r0]);
}

void op_trap(uint16_t instr) {
    reg[R_R7] = reg[R_PC];

    switch (instr & 0xFF) {
        case TRAP_GETC:
            /* read a single ASCII char */
            reg[R_R0] = (uint16_t)getchar();
            update_flags(R_R0);
            break;
        case TRAP_OUT:
            putc((char)reg[R_R0], stdout);
            fflush(stdout);
            break;
        case TRAP_PUTS:
            /* one char per word */
            uint16_t *c = memory + reg[R_R0];
            while (*c) {
                putc((char)*c, stdout);
                ++c;
            }
            fflush(stdout);
            break;
        case TRAP_IN:
            printf("Enter a character: ");
            char c = getchar();
            putc(c, stdout);
            fflush(stdout);
            reg[R_R0] = (uint16_t)c;
            update_flags(R_R0);
            break;
        case TRAP_PUTSP:
            /* one char per byte (two bytes per word)
            here we need to swap back to
            big endian format */
            uint16_t* c = memory + reg[R_R0];
            while (*c)
            {
                char char1 = (*c) & 0xFF;
                putc(char1, stdout);
                char char2 = (*c) >> 8;
                if (char2) putc(char2, stdout);
                ++c;
            }
            fflush(stdout);
            break;

        case TRAP_HALT:
            puts("HALT");
            fflush(stdout);
            running = 0;
            break;
    }
}

uint16_t sign_extend(uint16_t x, int bit_count)
{
    int n = bit_count - 1;
    if ((x >> n) & 1) {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

void update_flags(uint16_t r)
{
    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}