#import os/term
#import rnd
#import time
#import chip8instr.c

enum {
	MEM_BEGIN = 0x200,
	VID_WIDTH = 64,
	VID_HEIGHT = 32,
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

const bool DEBUG = false;

void loadrom(chip8_t *c8, const char *rompath) {
	FILE *f = fopen(rompath, "rb");
	if (!f) {
		fprintf(stderr, "failed to open file: %s\n", strerror(errno));
		exit(1);
	}
	int pos = MEM_BEGIN;
	while (true) {
		int c = fgetc(f);
		if (c == EOF) break;
		c8->memory[pos++] = (uint8_t) c;
	}
	fclose(f);
}

pub int run(char *rompath) {
	chip8_t c8 = {};

	// Load digit sprites into memory at zero.
	// Technically, we don't need this as we can load sprites directly,
	// but there might be some crazy code that assumes the sprites are here.
	memcpy(c8.memory, DIGITS, sizeof(DIGITS));

	c8.PC = MEM_BEGIN;

	loadrom(&c8, rompath);

	signal(SIGINT, handle_interrupt);
	term = term.term_get_stdin();
	term.term_disable_input_buffering(term);

	while (true) {
		bool skip = false;
		if (c8.DT) {
			c8.DT--;
			skip = true;
		}
		if (c8.ST) {
			c8.ST--;
			skip = true;
			buzz();
		}
		if (skip) continue;


		int code = pollkeyboard();
		if (code >= 0) {
			if (DEBUG) {
				printf("pressed %X\n", code);
			}
			c8.keyBuffer[code] = 1;
		}

		uint8_t b1 = c8.memory[c8.PC];
		uint8_t b2 = c8.memory[c8.PC + 1];
		chip8instr.instr_t instr;
		chip8instr.decode(&instr, b1, b2);

		if (DEBUG) {
			printf("0x%x ", c8.PC);
			chip8instr.print_instr(instr);
			putchar('\n');
		}

		c8.PC += 2;

		step(&c8, instr);
		if (code >= 0) {
			c8.keyBuffer[code] = 0;
		}

		if (DEBUG) {
			printf("\t");
			for (int i = 0; i < 16; i++) {
				printf(" %x", c8.registers[i]);
			}
			printf(" I:%x\n\n", c8.I);
		}


		draw(c8.video, VID_WIDTH, VID_HEIGHT);

		// 60Hz
		time.sleep(time.SECONDS / 60);
	}
}

int pollkeyboard() {
	if (!term.stdin_has_input()) {
		return -1;
	}
	return keycode(getchar());
}

void step(chip8_t *c8, chip8instr.instr_t instr) {
	int OP = instr.OP;
	int x = instr.x;
	int y = instr.y;
	uint16_t nnn = instr.nnn;


	switch (OP) {
		//
		// Drawing
		//

		// Clear screen
		case chip8instr.CLS: {
			memset(c8->video, 0, VID_WIDTH * VID_HEIGHT);
		}

		// I = digit_sprite_addr(Vx)
		case chip8instr.LDFVx: {
			int num = c8->registers[instr.x];
			if (num < 0 || num > 15) {
				panic("loading sprite for number %d - out of bounds\n", num);
			}
			c8->I = c8->registers[instr.x] * 5;
		}

		// Put decimal digits for Vx into I for drawing.
		case chip8instr.LDBVx: {
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

		// Draw first n bytes of selected sprite at (Vx, Vy), set VF to collision.
		case chip8instr.DRWVxVyn: {
			uint8_t *p = c8->memory + c8->I;

			bool collision = false;
			for (int i = 0; i < instr.d4; i++) {
				collision = xor_sprite(c8, c8->registers[instr.x], c8->registers[instr.y] + i, *p) || collision;
				p++;
			}
			if (collision) {
				c8->registers[VF] = 1;
			} else {
				c8->registers[VF] = 0;
			}
		}

		//
		// Keyboard
		//
		// Wait for a key press, Vx = key code.
		case chip8instr.LDVxK: {
			c8->registers[x] = getchar();
		}

		//
		// Jumping
		//

		// Goto nnn.
		case chip8instr.JPnnn: {
			c8->PC = instr.nnn;
		}
		// Goto V0 + nnn.
		case chip8instr.JPV0nnn: {
			c8->PC = c8->registers[V0] + nnn;
		}
		// Call subroutine at nnn.
		case chip8instr.CALLnnn: {
			c8->callStack[c8->SP] = c8->PC;
			c8->SP++;
			c8->PC = instr.nnn;
		}
		// Return from a call.
		case chip8instr.RET: {
			c8->SP--;
			c8->PC = c8->callStack[c8->SP];
			c8->callStack[c8->SP] = 0;
		}
		// Skip next instruction if Vx = kk.
		case chip8instr.SEVxkk: {
			if (c8->registers[x] == instr.kk) {
				c8->PC += 2;
			}
		}
		// Skip next instruction if Vx != kk.
		case chip8instr.SNEVxkk: {
			if (c8->registers[instr.x] != instr.kk) {
				c8->PC += 2;
			}
		}
		// Skip next instruction if Vx = Vy.
		case chip8instr.SEVxVy: {
			if (c8->registers[instr.x] == c8->registers[instr.y]) {
				c8->PC += 2;
			}
		}
		// Skip next instruction if Vx != Vy.
		case chip8instr.SNEVxVy: {
			if (c8->registers[instr.x] != c8->registers[instr.y]) {
				c8->PC += 2;
			}
		}
		// Skip next instruction if key with the value of Vx is pressed.
		case chip8instr.SKPVx: {
			int key = c8->registers[x];
			if (c8->keyBuffer[key]) {
				c8->PC += 2;
			}
		}
		// Skip next instruction if key with the value of Vx is not pressed.
		case chip8instr.SKNPVx: {
			int key = c8->registers[x];
			if (!c8->keyBuffer[key]) {
				c8->PC += 2;
			}
		}

		//
		// Moves
		//

		// Vx = kk
		case chip8instr.LDVxkk: {
			c8->registers[instr.x] = instr.kk;
		}
		// Vx += kk
		case chip8instr.ADDVxkk: {
			c8->registers[instr.x] += instr.kk;
		}
		// Vx = Vy
		case chip8instr.LDVxVy: {
			c8->registers[instr.x] = c8->registers[instr.y];
		}
		// I = nnn
		case chip8instr.LDInnn: {
			c8->I = nnn;
		}
		// I += Vx.
		case chip8instr.ADDIVx: {
			c8->I += c8->registers[x];
		}
		// Dump V0..Vx to memory at I
		case chip8instr.LD_I_Vx: {
			uint8_t *p = &c8->memory[c8->I];
			for (int i = 0; i <= x; i++) {
				*p++ = c8->registers[i];
			}
		}
		// Read V0..Vx from memory at I
		case chip8instr.LDVx_I_: {
			uint8_t *p = &c8->memory[c8->I];
			for (int i = 0; i <= x; i++) {
				c8->registers[i] = *p++;
			}
		}
		// ST = Vx.
		case chip8instr.LDSTVx: {
			c8->ST = c8->registers[x];
		}
		// Vx = DT.
		case chip8instr.LDVxDT: {
			c8->registers[instr.x] = c8->DT;
		}
		// DT = Vx.
		case chip8instr.LDDTVx: {
			c8->DT = c8->registers[instr.x];
		}

		//
		// Arithmetic
		//

		// Vx |= Vy
		case chip8instr.ORVxVy: {
			c8->registers[instr.x] = c8->registers[instr.x] | c8->registers[instr.y];
		}
		// Vx &= Vy
		case chip8instr.ANDVxVy: {
			c8->registers[instr.x] = c8->registers[instr.x] & c8->registers[instr.y];
		}
		// Vx ^= Vy
		case chip8instr.XORVxVy: {
			c8->registers[instr.x] = c8->registers[instr.x] ^ c8->registers[instr.y];
		}
		// Vx += Vy, VF = carry
		case chip8instr.ADDVxVy: {
			uint16_t sum = c8->registers[instr.x] + c8->registers[instr.y];
			c8->registers[x] = sum & 0xFF;
			if (sum > 255) {
				c8->registers[VF] = 1;
			} else {
				c8->registers[VF] = 0;
			}
		}
		// Vx -= Vy, VF = not borrow
		case chip8instr.SUBVxVy: {
			if (c8->registers[instr.x] < c8->registers[instr.y]) {
				c8->registers[VF] = 1;
				c8->registers[instr.x] = 255 + c8->registers[instr.x] - c8->registers[instr.y];
			} else {
				c8->registers[VF] = 0;
				c8->registers[instr.x] -= c8->registers[instr.y];
			}
		}
		// Shift Vx to the right, VF = popped bit.
		case chip8instr.SHRVx_Vy_: {
			c8->registers[VF] = c8->registers[x] & 1;
			c8->registers[x] = c8->registers[x] >> 1;
		}
		// Shift Vx to the left, VF = popped bit.
		case chip8instr.SHLVx_Vy_: {
			uint16_t r = c8->registers[x] << 1;
			c8->registers[VF] = r > 255;
			c8->registers[x] = r & 0xFF;
		}
		// Vx = Vy - Vx, VF = not borrow
		case chip8instr.SUBNVxVy: {
			if (c8->registers[y] < c8->registers[x]) {
				c8->registers[x] = 255 + c8->registers[y] - c8->registers[x];
				c8->registers[VF] = 0;
			} else {
				c8->registers[x] = c8->registers[y] - c8->registers[x];
				c8->registers[VF] = 1;
			}
		}
		
		// Vx = random & kk.
		case chip8instr.RNDVxkk: {
			c8->registers[x] = rnd.intn(256) & instr.kk;
		}
	}
}

// Unpacks bits from a byte into a given array.
void getbits(uint8_t byte, uint8_t *bits) {
	for (int i = 7; i >= 0; i--) {
		bits[i] = byte % 2;
		byte /= 2;
	}
}

bool xor_sprite(chip8_t *c8, int x, y, uint8_t sprite) {
	uint8_t bits[8];
	getbits(sprite, bits);

	bool collision = false;
	for (int i = 0; i < 8; i++) {
		collision = xor_pixel(c8, x+i, y, bits[i]) || collision;
	}
	return collision;
}

bool xor_pixel(chip8_t *c8, int x, y, value) {
	int pos = y * VID_WIDTH + x;
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

int _prevsum = -1;
void draw(char *buf, int width, height) {
	int n = width * height;

	int sum = 0;
	for (int i = 0; i < n; i++) {
		sum += buf[i];
	}
	if (sum == _prevsum) {
		return;
	}
	_prevsum = sum;
	for (int i = 0; i < n; i++) {
		if (buf[i]) {
			putchar('x');
		} else {
			putchar('.');
		}
		if ((i+1) % width == 0) {
			putchar('\n');
		}
	}
	puts("");
}
