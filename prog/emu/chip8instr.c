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
};

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
	switch (i.OP) {
		case JPnnn: { fprintf(stdout, "JP 0x%x", i.nnn); }
		case CALLnnn: { fprintf(stdout, "CALL 0x%x", i.nnn); }
		case SEVxkk: { fprintf(stdout, "SE V%x 0x%x \t// skip if V[%x] == %d", i.x, i.kk, i.x, i.kk); }
		case SNEVxkk: {
			fprintf(stdout, "SNE V%x 0x%x \t// skip if V[%x] != %d", i.x, i.kk, i.x, i.kk);
		}
		case SEVxVy: {
			fprintf(stdout, "SE V%x V%x \t// skip if equal", i.x, i.y);
		}
		case LDVxkk: {
			fprintf(stdout, "LD V%x 0x%x \t// V[%x] = %d", i.x, i.kk, i.x, i.kk);
		}
		case ADDVxkk: {
			fprintf(stdout, "ADD V%x 0x%x \t// V[%x] += %d", i.x, i.kk, i.x, i.kk);
		}
		case LDVxVy: {
			fprintf(stdout, "LD V%x V%x \t// V[%x] = V[%x]", i.x, i.y, i.x, i.y);
		}
		case ORVxVy: {
			fprintf(stdout, "OR V%x V%x \t// V[%x] |= V[%x]", i.x, i.y, i.x, i.y);
		}
		case ANDVxVy: {
			fprintf(stdout, "AND V%x V%x \t// V[%x] &= V[%x]", i.x, i.y, i.x, i.y);
		}
		case XORVxVy: {
			fprintf(stdout, "XOR V%x V%x \t// V[%x] ^= V[%x]", i.x, i.y, i.x, i.y);
		}
		case ADDVxVy: {
			fprintf(stdout, "ADD V%x V%x \t// V[%x] += V[%x]", i.x, i.y, i.x, i.y);
		}
		case SUBVxVy: {
			fprintf(stdout, "SUB V%x V%x \t// V[%x] -= V[%x]", i.x, i.y, i.x, i.y);
		}
		case SHRVx_Vy_: {
			fprintf(stdout, "SHR Vx _Vy_");
		}
		case SUBNVxVy: {
			fprintf(stdout, "SUBN Vx Vy");
		}
		case SHLVx_Vy_: {
			fprintf(stdout, "SHL Vx _Vy_");
		}
		case SNEVxVy: {
			fprintf(stdout, "SNE V%x V%x \t// skip if V[%x] != V[%x]", i.x, i.y, i.x, i.y);
		}
		case LDInnn: {
			fprintf(stdout, "LD I 0x%x \t// I = %x", i.nnn, i.nnn);
		}
		case JPV0nnn: {
			fprintf(stdout, "JP V0 nnn");
		}
		case RNDVxkk: {
			fprintf(stdout, "RND V%x 0x%x \t// V[%x] = random & 0x%x", i.x, i.kk, i.x, i.kk);
		}
		case DRWVxVyn: {
			fprintf(stdout, "DRW V%x V%x %x \t// draw %d bytes from I at (V[%x], V[%x])", i.x, i.y, i.d4, i.d4, i.x, i.y);
		}
		case SKPVx: {
			fprintf(stdout, "SKP Vx");
		}
		case SKNPVx: {
			fprintf(stdout, "SKNP Vx");
		}
		case LDVxDT: {
			fprintf(stdout, "LD Vx DT");
		}
		case LDVxK: {
			fprintf(stdout, "LD Vx K");
		}
		case LDDTVx: {
			fprintf(stdout, "LD DT Vx");
		}
		case LDSTVx: {
			fprintf(stdout, "LD ST Vx");
		}
		case ADDIVx: {
			fprintf(stdout, "ADD I Vx");
		}
		case LDFVx: {
			fprintf(stdout, "LD F Vx");
		}
		case LDBVx: {
			fprintf(stdout, "LDBVx");
		}
		case LD_I_Vx: {
			fprintf(stdout, "LD_I_Vx");
		}
		case LDVx_I_: {
			fprintf(stdout, "LD V%x [I] \t// init V[0]..V[%x] from memory at I", i.x, i.x);
		}
		case RET: { fprintf(stdout, "RET\n"); }
		case CLS: { fprintf(stdout, "CLS"); }
		default: {
			fprintf(stdout, "unknown op %x", i.OP);
		}
	}
}
