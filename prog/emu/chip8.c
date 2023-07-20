pub int disas(char *rompath) {
	FILE *in = fopen(rompath, "rb");
	if (!in) {
		fprintf(stderr, "Couldn't open input file %s: %s\n", rompath, strerror(errno));
		return 1;
	}
	size_t pos = 0x200;
	while (!feof(in)) {
		fprintf(stdout, "0x%zu\t", pos);

		uint8_t byte1 = 0;
		uint8_t byte2 = 0;
		fread( &byte1, 1, 1, in );
		fread( &byte2, 1, 1, in );
		pos += 2;

		uint8_t a = (byte1 & (0xF << 4)) >> 4;
		uint8_t b = (byte1 & 0xF);
		uint8_t c = (byte2 & (0xF << 4)) >> 4;
		uint8_t d = (byte2 & 0xF);
		uint16_t param2 = (c << 4) + d;
		uint16_t param3 = (b << 8) + (c << 4) + d;

		fprintf( stdout, "%x%x%x%x\t", a, b, c, d );

		switch (a) {
			case 0x0: {
				if (b == 0x0 && c == 0xE) {
					switch (d) {
						case 0x0: { fprintf( stdout, "CLS\n" ); }
						case 0xE: { fprintf( stdout, "RET\n" ); }
						default: { fprintf(stdout, "Unknown\n" ); return 2; }
					}
				} else {
					fprintf( stdout, "SYS\t0x%x\n", param3 );
				}
			}
			case 0x1: { fprintf( stdout, "JP\t0x%x\n", param3 ); }
			case 0x2: { fprintf( stdout, "CALL\t0x%x\n", param3 ); }
			case 0x3: { fprintf( stdout, "SE\tV%x, 0x%x\n", b, param2 ); }
			case 0x4: { fprintf( stdout, "SNE\tV%x, 0x%x\n", b, param2 ); }
			case 0x5: {
				if (d == 0) {
					fprintf(stdout, "SE\tV%x, V%x\n", b, c);
				} else {
					fprintf(stdout, "Unknown\n");
					return 2;
				}
			}
			case 0x6: { fprintf( stdout, "LD\tV%x, 0x%x\n", b, param2 ); }
			case 0x7: { fprintf( stdout, "ADD\tV%x, 0x%x\n", b, param2 ); }
			case 0x8: {
				switch (d) {
					case 0x0: { fprintf( stdout, "LD\tV%x, V%x\n", b, c ); }
					case 0x1: { fprintf( stdout, "OR\tV%x, V%x\n", b, c ); }
					case 0x2: { fprintf( stdout, "AND\tV%x, V%x\n", b, c ); }
					case 0x3: { fprintf( stdout, "XOR\tV%x, V%x\n", b, c ); }
					case 0x4: { fprintf( stdout, "ADD\tV%x, V%x\n", b, c ); }
					case 0x5: { fprintf( stdout, "SUB\tV%x, V%x\n", b, c ); }
					case 0x6: { fprintf( stdout, "SHR\tV%x {, V%x}\n", b, c ); }
					case 0x7: { fprintf( stdout, "SUBN\tV%x, V%x\n", b, c ); }
					case 0xE: { fprintf( stdout, "SHL\tV%x {, V%x}\n", b, c ); }
				}
			}
			case 0x9: {
				assert( d == 0 );
				fprintf( stdout, "SNE\tV%x, V%x\n", b, c );
			}
			case 0xA: { fprintf( stdout, "LD\tI, 0x%x\n", param3 ); }
			case 0xB: { fprintf( stdout, "JP\tV0, 0x%x\n", param3 ); }
			case 0xC: { fprintf( stdout, "RND\tV%x, 0x%x\n", b, param2 ); }
			case 0xD: { fprintf( stdout, "DRW\tV%x, V%x, %d\n", b, c, d ); }
			case 0xE: {
				if( c == 0x9 && d == 0xE ){
					fprintf( stdout, "SKP\tV%x\n", b );
					break;
				}
				if( c == 0xA && d == 0x1 ){
					fprintf( stdout, "SNKP\tV%x\n", b );
					break;
				}
				fprintf( stdout, "Error\n" );
				return 2;
			}
			case 0xF: {
				switch ((c<<4) + d) {
					case 0x07: { fprintf( stdout, "LD\tV%x, DT\n", b ); }
					case 0x0A: { fprintf( stdout, "LD\tV%x, K\n", b ); }
					case 0x15: { fprintf( stdout, "LD\tDT, V%x\n", b ); }
					case 0x18: { fprintf( stdout, "LD\tST, V%x\n", b ); }
					case 0x1E: { fprintf( stdout, "ADD\tI, V%x\n", b ); }
					case 0x29: { fprintf( stdout, "LD\tF, V%x\n", b ); }
					case 0x33: { fprintf( stdout, "LD\tB, V%x\n", b ); }
					case 0x55: { fprintf( stdout, "LD\t[I], V%x\n", b ); }
					case 0x65: { fprintf( stdout, "LD\tV%x, [I]\n", b ); }
				}
			}
			default: {
				fprintf(stderr, "Unknown: %02x %02x\n", byte1, byte2 );
			}
		}
	}
	return 0;
}
