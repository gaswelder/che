pub enum {
	NOOP,
	ADDIVx, ADDVxkk, ADDVxVy, ANDVxVy,
	SUBNVxVy, SUBVxVy,
	JPnnn, JPV0nnn,
	CALLnnn, RET,
	DRWVxVyn, CLS,
	LD_I_Vx, LDBVx, LDDTVx, LDFVx, LDInnn,
	LDSTVx, LDVx_I_, LDVxDT, LDVxK, LDVxkk, LDVxVy,
	RNDVxkk,
	SEVxkk, SEVxVy,
	SHLVx_Vy_, SHRVx_Vy_,
	SKNPVx, SKPVx, SNEVxkk, SNEVxVy,
	ORVxVy, XORVxVy,
}

pub typedef {
	int OP;
	int x, y;
	uint16_t nnn;
	int kk;
	int d4;
} instr_t;

// Decodes the instruction from bytes [b1 b2] into i.
pub void decode(instr_t *i, uint8_t b1, b2) {
	memset(i, 0, sizeof(instr_t));

	int d1 = b1 >> 4;
	int d2 = b1 & 0xF;
	int d3 = b2 >> 4;
	int d4 = b2 & 0xF;

	// 00EE - RET
	if (b1 == 0 && b2 == 0xEE) {
		i->OP = RET;
		return;
	}

	// 00E0 - CLS
	if (b1 == 0 && b2 == 0xE0) {
		i->OP = CLS;
		return;
	}

	// "C???" - "C" is a command
	// "Cxy?" - "x" and "y" are numbers of registers
	// "Cnnn" - nnn is an address
	// "C?kk" - "kk" is a byte value
	int c = d1;
	int x = d2;
	int y = d3;
	uint16_t nnn = b2 + (b1 & 0xF) * 256;
	int kk = b2;

	i->x = x;
	i->y = y;
	i->nnn = nnn;
	i->kk = kk;
	i->d4 = d4;

	int OP;
	switch (c) {
		case 1: { OP = JPnnn; } // 1nnn - JP nnn
		case 2: { OP = CALLnnn; } // 2nnn - CALL nnn
		case 3: { OP = SEVxkk; } // 3xkk - SE Vx, kk
		case 4: { OP = SNEVxkk; } // 4xkk - SNE Vx, kk
		case 5: { OP = SEVxVy; } // 5xy0 - SE Vx, Vy
		case 6: { OP = LDVxkk; } // 6xkk - LD Vx, kk
		case 7: { OP = ADDVxkk; } // 7xkk - ADD Vx, kk
		case 8: {
			switch (d4) {
				case 0: { OP = LDVxVy; } // 8xy0 - LD Vx, Vy
				case 1: { OP = ORVxVy; } // 8xy1 - OR Vx, Vy
				case 2: { OP = ANDVxVy; } // 8xy2 - AND Vx, Vy
				case 3: { OP = XORVxVy; } // 8xy3 - XOR Vx, Vy
				case 4: { OP = ADDVxVy; } // 8xy4 - ADD Vx, Vy
				case 5: { OP = SUBVxVy; } // 8xy5 - SUB Vx, Vy
				case 6: { OP = SHRVx_Vy_; } // 8xy6 - SHR Vx {, Vy}
				case 7: { OP = SUBNVxVy; } // 8xy7 - SUBN Vx, Vy
				case 0xE: { OP = SHLVx_Vy_; } // 8xyE - SHL Vx {, Vy}
			}
		}
		case 9: { OP = SNEVxVy; } // 9xy0 - SNE Vx, Vy
		case 0xA: { OP = LDInnn; } // Annn - LD I, nnn
		case 0xB: { OP = JPV0nnn; } // Bnnn - JP V0, nnn
		case 0xC: { OP = RNDVxkk; } // Cxkk - RND Vx, kk
		case 0xD: { OP = DRWVxVyn; } // Dxyn - DRW Vx, Vy, n
		case 0xE: {
			switch (b2) {
				case 0x9E: { OP = SKPVx; } // Ex9E - SKP Vx
				case 0xA1: { OP = SKNPVx; } // ExA1 - SKNP Vx
			}
		}
		case 0xF: {
			switch (b2) {
				case 0x07: { OP = LDVxDT; } // Fx07 - LD Vx, DT
				case 0x0A: { OP = LDVxK; } // Fx0A - LD Vx, K
				case 0x15: { OP = LDDTVx; } // Fx15 - LD DT, Vx
				case 0x18: { OP = LDSTVx; } // Fx18 - LD ST, Vx
				case 0x1E: { OP = ADDIVx; } // Fx1E - ADD I, Vx
				case 0x29: { OP = LDFVx; } // Fx29 - LD F, Vx
				case 0x33: { OP = LDBVx; } // Fx33 - LD B, Vx
				case 0x55: { OP = LD_I_Vx; } // Fx55 - LD [I], Vx
				case 0x65: { OP = LDVx_I_; } // Fx65 - LD Vx, [I]
			}
		}
	}
	i->OP = OP;
}

pub void print_instr(instr_t i) {
	FILE *f = stdout;
	switch (i.OP) {
		case JPnnn: {
			fprintf(f, "JP 0x%x\t# goto 0x%x", i.nnn, i.nnn);
		}
		case CALLnnn: {
			fprintf(f, "CALL 0x%x", i.nnn);
		}
		case SEVxkk: {
			fprintf(f, "SE V%x 0x%x\t# skip if V[%x] == %d", i.x, i.kk, i.x, i.kk);
		}
		case SNEVxkk: {
			fprintf(f, "SNE V%x 0x%x\t# skip if V[%x] != %d", i.x, i.kk, i.x, i.kk);
		}
		case SEVxVy: {
			fprintf(f, "SE V%x V%x\t# skip if equal", i.x, i.y);
		}
		case LDVxkk: {
			fprintf(f, "LD V%x 0x%x\t# V[%x] = %d", i.x, i.kk, i.x, i.kk);
		}
		case ADDVxkk: {
			fprintf(f, "ADD V%x 0x%x\t# V[%x] += %d", i.x, i.kk, i.x, i.kk);
		}
		case LDVxVy: {
			fprintf(f, "LD V%x V%x\t# V[%x] = V[%x]", i.x, i.y, i.x, i.y);
		}
		case ORVxVy: {
			fprintf(f, "OR V%x V%x\t# V[%x] |= V[%x]", i.x, i.y, i.x, i.y);
		}
		case ANDVxVy: {
			fprintf(f, "AND V%x V%x\t# V[%x] &= V[%x]", i.x, i.y, i.x, i.y);
		}
		case XORVxVy: {
			fprintf(f, "XOR V%x V%x\t# V[%x] ^= V[%x]", i.x, i.y, i.x, i.y);
		}
		case ADDVxVy: {
			fprintf(f, "ADD V%x V%x\t# V[%x] += V[%x]", i.x, i.y, i.x, i.y);
		}
		case SUBVxVy: {
			fprintf(f, "SUB V%x V%x\t# V[%x] -= V[%x]", i.x, i.y, i.x, i.y);
		}
		case SHRVx_Vy_: {
			fprintf(f, "SHR Vx _Vy_");
		}
		case SUBNVxVy: {
			fprintf(f, "SUBN Vx Vy");
		}
		case SHLVx_Vy_: {
			fprintf(f, "SHL Vx _Vy_");
		}
		case SNEVxVy: {
			fprintf(f, "SNE V%x V%x\t# skip if V[%x] != V[%x]", i.x, i.y, i.x, i.y);
		}
		case LDInnn: {
			fprintf(f, "LD I 0x%x\t# I = %x", i.nnn, i.nnn);
		}
		case JPV0nnn: {
			fprintf(f, "JP V%x 0x%x\t# jump to V[%x] + %d", i.x, i.nnn, i.x, i.nnn);
		}
		case RNDVxkk: {
			fprintf(f, "RND V%x 0x%x\t# V[%x] = random & 0x%x", i.x, i.kk, i.x, i.kk);
		}
		case DRWVxVyn: {
			fprintf(f, "DRW V%x V%x %x\t# draw %d bytes from I at (V[%x], V[%x])", i.x, i.y, i.d4, i.d4, i.x, i.y);
		}
		case SKPVx: {
			fprintf(f, "SKP V%x  \t# skip next instruction if key V[%x] is pressed", i.x, i.x);
		}
		case SKNPVx: {
			fprintf(f, "SKNP V%x  \t# skip next instruction if key V[%x] is not pressed", i.x, i.x);
		}
		case LDVxDT: {
			fprintf(f, "LD V%x DT\t# V[%x] = DT", i.x, i.x);
		}
		case LDDTVx: {
			fprintf(f, "LD DT V%d\t# DT = V[%x]", i.x, i.x);
		}
		case LDVxK: {
			fprintf(f, "LD V%x K\t# V%x = wait_key_press()", i.x, i.x);
		}
		case LDSTVx: {
			fprintf(f, "LD ST V%x\t# ST = V%x", i.x, i.x);
		}
		case ADDIVx: {
			fprintf(f, "ADD I V%x\t# I = V%x", i.x, i.x);
		}
		case LDFVx: {
			fprintf(f, "LD F V%x \t# I = sprite_addr(V[%x])", i.x, i.x);
		}
		case LDBVx: {
			fprintf(f, "LD B V%x \t# build_sprites_for(V[%x])", i.x, i.x);
		}
		case LD_I_Vx: {
			fprintf(f, "LD [I] V%x\t# dump registers 0..%x to I", i.x, i.x);
		}
		case LDVx_I_: {
			fprintf(f, "LD V%x [I] \t# load registers 0..%x from I", i.x, i.x);
		}
		case RET: { fprintf(f, "RET\n"); }
		case CLS: { fprintf(f, "CLS"); }
		default: {
			fprintf(f, "unknown op %x", i.OP);
		}
	}
}
