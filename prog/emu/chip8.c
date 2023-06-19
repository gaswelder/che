pub int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
		return 1;
	}

	FILE *in = fopen( argv[1], "rb" );
	if (!in) {
		fprintf(stderr, "Couldn't open input file %s: %s\n", argv[1], strerror(errno));
		return 1;
	}

	int r = disas(in, stdout, stderr);
	fclose(in);
	return r;
}

int disas(FILE *in, *out, *err) {
	size_t pos = 0x200;
	while (!feof(in)) {
		fprintf(out, "0x%zu\t", pos);

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

		fprintf( out, "%x%x%x%x\t", a, b, c, d );

		switch( a )
		{
			case 0x0:
				if( b == 0x0 && c == 0xE )
				{
					if( d == 0x0 ){
						fprintf( out, "CLS\n" );
					} else if( d == 0xE ){
						fprintf( out, "RET\n" );
					} else {
						fprintf(out, "Unknown\n" );
						return 2;
					}
				} else {
					fprintf( out, "SYS\t0x%x\n", param3 );
				}
			break;
			case 0x1: fprintf( out, "JP\t0x%x\n", param3 ); break;
			case 0x2: fprintf( out, "CALL\t0x%x\n", param3 ); break;
			case 0x3: fprintf( out, "SE\tV%x, 0x%x\n", b, param2 ); break;
			case 0x4: fprintf( out, "SNE\tV%x, 0x%x\n", b, param2 ); break;
			case 0x5:
				if( d == 0 ){
					fprintf( out, "SE\tV%x, V%x\n", b, c );
				} else {
					fprintf( out, "Unknown\n" );
					return 2;
				}
			break;
			case 0x6: fprintf( out, "LD\tV%x, 0x%x\n", b, param2 ); break;
			case 0x7: fprintf( out, "ADD\tV%x, 0x%x\n", b, param2 ); break;
			case 0x8:
				switch( d )
				{
					case 0x0: fprintf( out, "LD\tV%x, V%x\n", b, c ); break;
					case 0x1: fprintf( out, "OR\tV%x, V%x\n", b, c ); break;
					case 0x2: fprintf( out, "AND\tV%x, V%x\n", b, c ); break;
					case 0x3: fprintf( out, "XOR\tV%x, V%x\n", b, c ); break;
					case 0x4: fprintf( out, "ADD\tV%x, V%x\n", b, c ); break;
					case 0x5: fprintf( out, "SUB\tV%x, V%x\n", b, c ); break;
					case 0x6: fprintf( out, "SHR\tV%x {, V%x}\n", b, c ); break;
					case 0x7: fprintf( out, "SUBN\tV%x, V%x\n", b, c ); break;
					case 0xE: fprintf( out, "SHL\tV%x {, V%x}\n", b, c ); break;
				}
			break;
			case 0x9:
				assert( d == 0 );
				fprintf( out, "SNE\tV%x, V%x\n", b, c );
				break;
			case 0xA: fprintf( out, "LD\tI, 0x%x\n", param3 ); break;
			case 0xB: fprintf( out, "JP\tV0, 0x%x\n", param3 ); break;
			case 0xC: fprintf( out, "RND\tV%x, 0x%x\n", b, param2 ); break;
			case 0xD: fprintf( out, "DRW\tV%x, V%x, %d\n", b, c, d ); break;
			case 0xE:
				if( c == 0x9 && d == 0xE ){
					fprintf( out, "SKP\tV%x\n", b );
					break;
				}
				if( c == 0xA && d == 0x1 ){
					fprintf( out, "SNKP\tV%x\n", b );
					break;
				}
				fprintf( out, "Error\n" );
				return 2;
			case 0xF:
				switch( (c<<4) + d )
				{
					case 0x07: fprintf( out, "LD\tV%x, DT\n", b ); break;
					case 0x0A: fprintf( out, "LD\tV%x, K\n", b ); break;
					case 0x15: fprintf( out, "LD\tDT, V%x\n", b ); break;
					case 0x18: fprintf( out, "LD\tST, V%x\n", b ); break;
					case 0x1E: fprintf( out, "ADD\tI, V%x\n", b ); break;
					case 0x29: fprintf( out, "LD\tF, V%x\n", b ); break;
					case 0x33: fprintf( out, "LD\tB, V%x\n", b ); break;
					case 0x55: fprintf( out, "LD\t[I], V%x\n", b ); break;
					case 0x65: fprintf( out, "LD\tV%x, [I]\n", b ); break;
				}
				break;
			default:
				fprintf(err, "Unknown: %02x %02x\n", byte1, byte2 );
		}
	}
	return 0;
}
