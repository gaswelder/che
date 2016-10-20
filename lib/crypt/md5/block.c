import "zio"

/*
 * The internal "machinery" processes a stream of 64-byte blocks.
 * The message itself is put in the base of that stream, followed by
 * a padding:
 *
 * | message | eof | zeros | length |
 *
 * Everything is measured in bits, but here we always deal with bytes.
 * 'message' is the actual data, its length is 'b' bits (b/8 bytes).
 * 'eof' byte is a bit '1' followed by seven zero bits.
 * 'zeros' is 'z' zero bytes.
 * 'length' is an 64-bit encoding of length of the message.
 * The number of zeros 'z' is such that end of stream happens to be at
 * a length mark that is a multiple of 512 bits (64 bytes).
 */
struct __src {
	zio *stream; // data stream

	uint64_t length; // current message length in bits
	bool more_data; // set to false after the end of data
	int zeros; // how many zeros left to put out

	uint8_t lenbuf[8]; // buffer with the message length representation
	int lenpos; // current position in lenbuf

	bool more;
};

/*
 * Process the stream and put the digest in 'digest'.
 */
void md5(zio *stream, uint32_t digest[4])
{
	struct __src s = {
		.stream = stream,
		.length = 0,
		.more_data = true,
		.more = true
	};

	md5_init(digest);

	/*
	 * Process the stream in 16-word blocks.
	 */
	uint32_t block[16];
	while (s.more)
	{
		for (int i = 0; i < 16; i++) {
			block[i] = next_word(&s);
		}
		//print_block(block);
		md5_feed(digest, block);
	}
}

/*
void print_block(uint32_t b[16])
{
	for(int i = 0; i < 16; i++) {
		printf("%08x (%u)", b[i], b[i]);
		if((i+1) % 4 == 0) putchar('\n');
		else putchar(' ');
	}
}
*/
/*
 * Regardless of the machine, the "md5" byte is big-endian (the highest
 * bit comes first), but a "word", which is 4 bytes, is little-endian
 * (the lowest byte comes first):
 *   |->|->|->|->|->|->|->|->|
 *   |<----------|<----------|
 */
uint32_t next_word(struct __src *s)
{
	uint32_t word = 0;
	int pos = 0;
	/*
	 * Compose a word from bytes, least-significant byte first.
	 */
	for(int i = 0; i < 4; i++) {
		uint8_t b = next_byte(s);
		word += (b << pos);
		pos += 8;
	}
	return word;
}

uint8_t next_byte(struct __src *s)
{
	if(s->more_data) {
		int c = zgetc(s->stream);
		if(c != EOF) {
			s->length += 8;
			return (uint8_t) c;
		}
		s->more_data = false;

		/*
		 * Calculate padding parameters now that we know the
		 * actual message length.
		 */
		init_padding(s);

		/*
		 * The spec says to add bit '1' to the end of the actual
		 * data and then put zeros until a specific position is
		 * reached. We can just add the whole byte with first bit
		 * set to '1' because we will not reach that special
		 * position for at least 7 more bits. And this byte is
		 * actually value 128 (because bytes are treated as big-endian).
		 */
		return 128;
	}

	if(s->zeros > 0) {
		s->zeros--;
		return 0;
	}

	assert((size_t) s->lenpos < sizeof(s->lenbuf));
	uint8_t b = s->lenbuf[s->lenpos++];
	/*
	 * Put a flag after the stream is finished.
	 */
	if(s->lenpos == sizeof(s->lenbuf)) {
		s->more = false;
	}
	return b;
}

void init_padding(struct __src *s)
{
	/*
	 * Now we know 'b' (data length in bits), so we can create the
	 * length mark and calculate how many zero bytes need to be added.
	 */

	/*
	 * The length mark is a sequence of two words, low word first.
	 */
	int i;
	uint8_t b;
	uint32_t w;
	uint8_t *pos = (uint8_t *) &(s->lenbuf);

	// low word
	w = s->length & 0xFFFFFFFF;
	for(i = 0; i < 4; i++) {
		b = (w >> i*8) & 0xFF;
		*pos = b;
		pos++;
	}
	// high word
	w = (s->length >> 32) & 0xFFFFFFFF;
	for(int i = 0; i < 4; i++) {
		b = (w >> i*8) & 0xFF;
		*pos = b;
		pos++;
	}

	s->lenpos = 0;

	/*
	 * To get the number of zero bytes we have to solve the following:
	 * (length + 1 + zeros + 8) % 64 == 0
	 * where '1' is for the 'eof' byte and '8' is for the length marker.
	 */
	int l = s->length/8;
	int n = (9 + l) / 64;
	if((9 + l) % 64 != 0) {
		n++;
	}
	s->zeros = 64 * n - 9 - l;
}
