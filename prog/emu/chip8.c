#import os/term
#import rnd
#import time
#import chip8instr.c

enum {
	MEM_BEGIN = 0x200,
	videoWidth = 64,
	videoHeight = 32,
	V0 = 0x0,
	VF = 0xF,
};

typedef {
	// 4 KB of RAM, 0x000...0xFFF.
	// The first 512 bytes, 0x000...0x1FF are reserved.
	uint8_t memory[4096];

	uint16_t PC; // Program counter.
	uint16_t I; // general-purpose address register typically used to point to drawable sprites in memory.

	// Call stack and stack pointer.
	uint16_t callStack[16];
	uint8_t SP;

	// Delay timer. When non-zero, the machine decrements it and skips other work.
	uint8_t DT;

	// Sound timer. When non-zero, the machine decrements it and keeps buzzing until it becomes zero.
	uint8_t ST;

	// Video screen with 64x32 monochrome pixels.
	char video[64 * 32];

	// 16 general purpose registers, usually referred to as V0 to VF.
	// VF is also used for carry and collide flags.
	char registers[16];

	// Key buffer, an array of 16 key states (pressed/not pressed).
	// The keypad looks like this:
	// 1 2 3 C
	// 4 5 6 D
	// 7 8 9 E
	// A 0 B F
	bool keyBuffer[16];
} chip8_t;

int keycode(char c) {
	// 1 2 3 C   ->   1 2 3 4
	// 4 5 6 D   ->   q w e r
	// 7 8 9 E   ->   a s d f
	// A 0 B F   ->   z x c v
	switch (c) {
		case '1': { return 1; }
		case '2': { return 2; }
		case '3': { return 3; }
		case '4': { return 0xC; }
		case 'q': { return 4; }
		case 'w': { return 5; }
		case 'e': { return 6; }
		case 'r': { return 0xD; }
		case 'a': { return 7; }
		case 's': { return 8; }
		case 'd': { return 9; }
		case 'f': { return 0xE; }
		case 'z': { return 0xA; }
		case 'x': { return 0; }
		case 'c': { return 0xB; }
		case 'v': { return 0xF; }
		default: { return -1; }
	}
}

// Digit sprites.
// Every digit is 5 bytes.
// This, for example, is the digit 1 (look at "1" bits):
// 0 0 1 0 0 0 0 0 | 0x20
// 0 1 1 0 0 0 0 0 | 0x60
// 0 0 1 0 0 0 0 0 | 0x20
// 0 0 1 0 0 0 0 0 | 0x20
// 0 1 1 1 0 0 0 0 | 0x70
uint8_t DIGITS[] = {
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
	0xf0, 0x90, 0xf0, 0x90, 0x90, // A
	0xe0, 0x90, 0xe0, 0x90, 0xe0, // B
	0xf0, 0x80, 0x80, 0x80, 0xf0, // C
	0xe0, 0x90, 0x90, 0x90, 0xe0, // D
	0xf0, 0x80, 0xf0, 0x80, 0xf0, // E
	0xf0, 0x80, 0xf0, 0x80, 0x80, // F
};

term.term_t *term = NULL;

pub int run(char *rompath) {
	chip8_t c8 = {};

	// Put the digit sprits into the reserved memory.
	memcpy(c8.memory, DIGITS, sizeof(DIGITS));

	// Load the ROM into the available memory.
	FILE *f = fopen(rompath, "rb");
	if (!f) {
		fprintf(stderr, "failed to open file: %s\n", strerror(errno));
		return 1;
	}
	uint8_t *p = c8.memory + MEM_BEGIN;
	while (true) {
		int c = fgetc(f);
		if (c == EOF) {
			break;
		}
		*p++ = (uint8_t) c;
		p++;
	}
	fclose(f);
	c8.PC = MEM_BEGIN;

	signal(SIGINT, handle_interrupt);
	term = term.term_get_stdin();
	term.term_disable_input_buffering(term);

	while (true) {
		int code = -1;
		if (term.stdin_has_input()) {
			int c = getchar();
			code = keycode(c);
			if (code >= 0) {
				printf("pressed %X\n", code);
			}
			c8.keyBuffer[code] = 1;
		}
		// printf("PC %x\t%02x%02x\n", c8->PC, b1, b2);
		cycle(&c8);
		if (code >= 0) {
			c8.keyBuffer[code] = 0;
		}
		if (0) draw(&c8);
		// 60Hz
		time.sleep(time.SECONDS/60);
	}
}



void cycle(chip8_t *c8) {
	bool skip = false;
	if (c8->DT) {
		c8->DT--;
		skip = true;
	}
	if (c8->ST) {
		c8->ST--;
		skip = true;
		buzz();
	}
	if (skip) {
		return;
	}

	uint8_t b1 = c8->memory[c8->PC++];
	uint8_t b2 = c8->memory[c8->PC++];
	chip8instr.instr_t instr = {};
	chip8instr.decode(&instr, b1, b2);

	int OP = instr.OP;
	int x = instr.x;
	int y = instr.y;
	uint16_t nnn = instr.nnn;
	int kk = instr.kk;
	int d4 = instr.d4;


	switch (OP) {
		case chip8instr.RET: {
			c8->SP--;
			c8->PC = c8->callStack[c8->SP];
			c8->callStack[c8->SP] = 0;
		}
		case chip8instr.CLS: {
			memset(c8->video, 0, videoWidth * videoHeight);
		}
		// Jump to location nnn.
		case chip8instr.JPnnn: {
			c8->PC = nnn;
		}
		// Call subroutine at nnn.
		case chip8instr.CALLnnn: {
			c8->callStack[c8->SP] = c8->PC;
			c8->SP++;
			c8->PC = nnn;
		}
		// Skip next instruction if Vx = kk.
		case chip8instr.SEVxkk: {
			if (c8->registers[x] == kk) {
				c8->PC += 2;
			}
		}
		case chip8instr.SNEVxkk: {
			// Skip next instruction if Vx != kk.
			if (c8->registers[x] != kk) {
				c8->PC += 2;
			}
		}
		case chip8instr.SEVxVy: {
			// Skip next instruction if Vx = Vy.
			if (c8->registers[x] == c8->registers[y]) {
				c8->PC += 2;
			}
		}
		case chip8instr.LDVxkk: {
			// Set Vx = kk.
			c8->registers[x] = kk;
		}
		case chip8instr.ADDVxkk: {
			// Set Vx = Vx + kk.
			c8->registers[x] += kk;
		}
		case chip8instr.LDVxVy: {
			// Set Vx = Vy.
			c8->registers[x] = c8->registers[y];
		}
		case chip8instr.ORVxVy: {
			// Set Vx = Vx OR Vy.
			c8->registers[x] = c8->registers[x] | c8->registers[y];
		}
		case chip8instr.ANDVxVy: {
			// Set Vx = Vx AND Vy.
			c8->registers[x] = c8->registers[x] & c8->registers[y];
		}
		case chip8instr.XORVxVy: {
			// Set Vx = Vx XOR Vy.
			c8->registers[x] = c8->registers[x] ^ c8->registers[y];
		}
		case chip8instr.ADDVxVy: {
			// Set Vx = Vx + Vy, set VF = carry.
			// The values of Vx and Vy are added together.
			// If the result is greater than 8 bits (i.e., > 255,) VF is set to 1, otherwise 0.
			// Only the lowest 8 bits of the result are kept and stored in Vx.
			uint16_t sum = c8->registers[x] + c8->registers[y];
			c8->registers[x] = sum & 0xFF;
			if (sum > 255) {
				c8->registers[VF] = 1;
			} else {
				c8->registers[VF] = 0;
			}
		}
		case chip8instr.SUBVxVy: {
			// Set Vx = Vx - Vy, set VF = NOT borrow.
			// If Vx > Vy, then VF is set to 1, otherwise 0. Then Vy is subtracted from Vx, and the results stored in Vx.
			if (c8->registers[x] < c8->registers[y]) {
				c8->registers[VF] = 1;
				c8->registers[x] = 255 + c8->registers[x] - c8->registers[y];
			} else {
				c8->registers[VF] = 0;
				c8->registers[x] -= c8->registers[y];
			}
		}
		// Shift Vx to the right, set VF to the popped bit.
		case chip8instr.SHRVx_Vy_: {
			c8->registers[VF] = c8->registers[x] & 1;
			c8->registers[x] = c8->registers[x] >> 1;
		}
		// Set Vx = Vy - Vx, set VF = NOT borrow.
		// If Vy > Vx, then VF is set to 1, otherwise 0.
		// Then Vx is subtracted from Vy, and the results stored in Vx.
		case chip8instr.SUBNVxVy: {
			if (c8->registers[y] < c8->registers[x]) {
				c8->registers[x] = 255 + c8->registers[y] - c8->registers[x];
				c8->registers[VF] = 0;
			} else {
				c8->registers[x] = c8->registers[y] - c8->registers[x];
				c8->registers[VF] = 1;
			}
		}
		case chip8instr.SHLVx_Vy_: {
			// Set Vx = Vx SHL 1.
			// If the most-significant bit of Vx is 1, then VF is set to 1, otherwise to 0.
			// Then Vx is multiplied by 2.
			uint16_t r = c8->registers[x] << 1;
			c8->registers[VF] = r > 255;
			c8->registers[x] = r & 0xFF;
		}
		case chip8instr.SNEVxVy: {
			// Skip next instruction if Vx != Vy.
			if (c8->registers[x] != c8->registers[y]) {
				c8->PC += 2;
			}
		}
		case chip8instr.LDInnn: {
			c8->I = nnn;
		}
		case chip8instr.JPV0nnn: {
			// Jump to location V0 + nnn.
			c8->PC = c8->registers[V0] + nnn;
		}
		case chip8instr.RNDVxkk: {
			// Set Vx = random byte AND kk.
			c8->registers[x] = rnd.intn(256) & kk;
		}
		case chip8instr.DRWVxVyn: {
			// Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
			// The interpreter reads n bytes from memory, starting at the address stored in I. These bytes are then displayed as sprites on screen at coordinates (Vx, Vy). Sprites are XORed onto the existing screen. If this causes any pixels to be erased, VF is set to 1, otherwise it is set to 0. If the sprite is positioned so part of it is outside the coordinates of the display, it wraps around to the opposite side of the screen. See instruction 8xy3 for more information on XOR, and section 2.4, Display, for more information on the Chip-8 screen and sprites.
			uint8_t *p = c8->memory + c8->I;
			bool collision = false;
			for (int i = 0; i < d4; i++) {
				collision = xor_sprite(c8, x, y + i, *p) || collision;
				p++;
			}
			if (collision) {
				c8->registers[VF] = 1;
			} else {
				c8->registers[VF] = 0;
			}
		}
		case chip8instr.SKPVx: {
			// Skip next instruction if key with the value of Vx is pressed.
			int key = c8->registers[x];
			if (c8->keyBuffer[key]) {
				c8->PC += 2;
			}
		}
		case chip8instr.SKNPVx: {
			// Skip next instruction if key with the value of Vx is not pressed.
			int key = c8->registers[x];
			if (!c8->keyBuffer[key]) {
				c8->PC += 2;
			}
		}
		case chip8instr.LDVxDT: {
			// Set Vx = delay timer value.
			c8->registers[x] = c8->DT;
		}
		case chip8instr.LDVxK: {
			// Wait for a key press, store the value of the key in Vx.
			c8->registers[x] = getchar();
		}
		case chip8instr.LDDTVx: {
			// Set delay timer = Vx.
			c8->DT = c8->registers[x];
		}
		case chip8instr.LDSTVx: {
			// Set sound timer = Vx.
			c8->ST = c8->registers[x];
		}
		case chip8instr.ADDIVx: {
			// Set I = I + Vx.
			c8->I += c8->registers[x];
		}
		case chip8instr.LDFVx: {
			// Set I = location of sprite for digit Vx.
			// The value of I is set to the location for the hexadecimal sprite corresponding to the value of Vx. See section 2.4, Display, for more information on the Chip-8 hexadecimal font.
			c8->I = c8->registers[x] * 5;
		}
		case chip8instr.LDBVx: {
			// Store BCD representation of Vx in memory locations I, I+1, and I+2.
			// The interpreter takes the decimal value of Vx, and places the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.
			uint8_t val = c8->registers[x];
			uint8_t ones = val % 10;
			val /= 10;
			uint8_t tens = val % 10;
			val /= 10;
			uint8_t hundreds = val;

			uint8_t *p = &c8->memory[c8->I];
			*p++ = hundreds;
			*p++ = tens;
			*p++ = ones;
		}
		case chip8instr.LD_I_Vx: {
			// Store registers V0 through Vx in memory starting at location I.
			uint8_t *p = &c8->memory[c8->I];
			for (int i = 0; i <= x; i++) {
				*p++ = c8->registers[i];
			}
		}
		case chip8instr.LDVx_I_: {
			// Read registers V0 through Vx from memory starting at location I.
			uint8_t *p = &c8->memory[c8->I];
			for (int i = 0; i <= x; i++) {
				c8->registers[i] = *p++;
			}
		}
	}
}

bool xor_sprite(chip8_t *c8, int x, y, uint8_t sprite) {
	bool collision = false;
	for (int i = 7; i >= 0; i--) {
		int bit = sprite % 2;
		sprite /= 2;
		collision = xor_pixel(c8, x+i, y, bit) || collision;
	}
	return collision;
}

bool xor_pixel(chip8_t *c8, int x, y, value) {
	printf("pixel at %d,%d -> %d\n", x, y, value);
	int pos = (x % videoWidth) + videoWidth * (y % videoHeight);
	char oldval = c8->video[pos];
	char newval = oldval ^ value;
	c8->video[pos] = newval;
	return oldval && !newval;
}

void handle_interrupt(int signal) {
	term.term_restore(term);
	printf("exit on signal %d\n", signal);
	exit(-2);
}

void buzz() {
	putchar('\b');
}

void draw(chip8_t *c8) {
	int n = videoWidth * videoHeight;
	for (int i = 0; i < n; i++) {
		int val = c8->video[i];
		if (val) {
			putchar('x');
		} else {
			putchar('.');
		}
		if ((i+1) % videoWidth == 0) {
			putchar('\n');
		}
	}
}

pub int disas(char *rompath) {
	// Read and parse the whole thing.
	chip8instr.instr_t img[4000];
	size_t n = 0;
	FILE *in = fopen(rompath, "rb");
	if (!in) {
		fprintf(stderr, "Couldn't open input file %s: %s\n", rompath, strerror(errno));
		return 1;
	}
	while (!feof(in)) {
		int a = fgetc(in);
		int b = fgetc(in);
		if (a == EOF && b == EOF) break;
		if (b == EOF) {
			fprintf(stderr, "unexpected end of file\n");
			return 1;
		}
		uint8_t byte1 = (uint8_t) a;
		uint8_t byte2 = (uint8_t) b;
		chip8instr.decode(&img[n], byte1, byte2);
		n++;
	}
	fclose(in);

	size_t call_targets[100] = {};
	size_t call_targets_len = 0;
	for (size_t j = 0; j < n; j++) {
		chip8instr.instr_t i = img[j];
		if (i.OP == chip8instr.CALLnnn) {
			call_targets[call_targets_len++] = i.nnn;
		}
	}

	// Print
	size_t pos = MEM_BEGIN;
	for (size_t j = 0; j < n; j++) {
		fprintf(stdout, "0x%x\t", (int) pos);
		chip8instr.instr_t i = img[j];
		chip8instr.print_instr(i);
		fprintf(stdout, "\n");
		pos += 2;
		if (contains(call_targets, call_targets_len, pos)) {
			fprintf(stdout, "\n ----- proc ----- \n");
		}
	}
	return 0;
}

bool contains(size_t *xs, size_t n, size_t x) {
	for (size_t i = 0; i < n; i++) {
		if (xs[i] == x) {
			return true;
		}
	}
	return false;
}
