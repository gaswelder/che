// https://giflib.sourceforge.net/gif89.txt

#import reader
#import enc/endian
#import bits
#import compress/lzw
#import image

typedef {
	reader.t *r;
	bool global_color_table;
	int color_resolution;
	int table_size;
	uint8_t bgcolor_index;
	image.rgba_t color_table[256];
	uint16_t width;
	uint16_t height;
} gif_t;

pub image.image_t *read(reader.t *r) {
	gif_t g = { .r = r };
	readblock_header(r);
	readblock_logical_screen_descriptor(&g);
	if (g.global_color_table) {
		read_colortable(&g);
	}
	readblock_graphics_control_extension(r);
	readblock_image_descriptor(&g);
	image.image_t *img = readblock_image_data(&g);
	readblock_trailer(r);

	uint8_t byte = 0;
	int x = reader.read(r, &byte, 1);
	if (x != EOF) {
		panic("unexpected trailing data");
	}
	info(&g);
	return img;
}

void info(gif_t *g) {
	printf("width = %u, height = %u\n", g->width, g->height);
	printf("ggt = %d\n", g->global_color_table);
	printf("res = %d bits/primary\n", g->color_resolution);
	printf("table size = %d\n", g->table_size);
	printf("bgcolor: %d\n", g->bgcolor_index);
	for (int i = 0; i < g->table_size; i++) {
		image.rgba_t c = g->color_table[i];
		printf("color %d: (%d,%d,%d)\n", i, c.red, c.green, c.blue);
	}
}

void readblock_header(reader.t *r) {
	uint8_t header[6] = {};
	reader.read(r, header, 6);
	if (memcmp(header, "GIF89a", 6) != 0) {
		panic("expected GIF89a");
	}
}

void readblock_logical_screen_descriptor(gif_t *g) {
	reader.t *r = g->r;

	// 4 bytes: logical screen size, obsolete.
	uint16_t width = 0;
	uint16_t height = 0;
	endian.read2le(r, &width);
	endian.read2le(r, &height);

	// 1 byte: packed color table params.
	uint8_t desc = 0;
	reader.read(r, &desc, 1);
	parse_color_table_params(g, desc);

	// 1 byte: bgcolor index
	reader.read(r, &g->bgcolor_index, 1);

	// 1 byte: pixel aspect ratio, not used.
	uint8_t pixel_aspect_ratio = 0;
	reader.read(r, &pixel_aspect_ratio, 1);
	if (pixel_aspect_ratio != 0) {
		panic("unexpected obsolete non-zero pixel aspect ratio");
	}
}

void parse_color_table_params(gif_t *g, uint8_t desc) {
	uint8_t descbits[8] = {};
	bits.getbits_msfirst(desc, descbits);

	g->global_color_table = descbits[0] == 1;

	// Color resolution for the global color table.
	// (bits per primary color) - 1.
	g->color_resolution = 4*descbits[1] + 2*descbits[2] + descbits[3] + 1;

	// Sort flag. If true, then colors are sorted in decreasing frequency use.
	// Not used anymore.
	bool sortflag = descbits[4] == 1;
	if (sortflag) {
		panic("unexpected obsolete sortflag = true");
	}

	// Global color table size.
	g->table_size = 1 << (1 + 4*descbits[5] + 2*descbits[6] + descbits[7]);
}

void read_colortable(gif_t *g) {
	reader.t *r = g->r;
	int ncolors = g->table_size;
	for (int i = 0; i < ncolors; i++) {
		uint8_t rgb[3] = {};
		reader.read(r, rgb, 3);
		g->color_table[i].red = rgb[0];
		g->color_table[i].green = rgb[1];
		g->color_table[i].blue = rgb[2];
	}
}

void readblock_graphics_control_extension(reader.t *r) {
	// Extension introducer: 0x21
	uint8_t introducer = 0;
	reader.read(r, &introducer, 1);
	if (introducer != 0x21) {
		panic("expected 0x21, got 0x%x", introducer);
	}

	// Graphic Control Label: 0xF9
	uint8_t label = 0;
	reader.read(r, &label, 1);
	if (label != 0xF9) {
		panic("expected 0xF9, got 0x%x", label);
	}

	// Block size: 0x04
	uint8_t block_size = 0;
	reader.read(r, &block_size, 1);
	if (block_size != 4) {
		panic("expected block size 4, got %d", block_size);
	}

	// Packed field
	uint8_t packed = 0;
	reader.read(r, &packed, 1);
	// int disposal_method = (packed >> 2) & 0x07;
	// bool user_input = (packed >> 1) & 1;
	// bool transparent_flag = packed & 1;
	// printf("disposal = %d, user_input = %d, transparent = %d\n", disposal_method, user_input, transparent_flag);

	// Delay time (little-endian, in centiseconds)
	uint16_t delay = 0;
	endian.read2le(r, &delay);
	// printf("delay = %d cs\n", delay);

	// Transparent color index
	uint8_t transparent_index = 0;
	reader.read(r, &transparent_index, 1);
	// printf("transparent index = %d\n", transparent_index);

	// Block terminator: 0x00
	uint8_t terminator = 0;
	reader.read(r, &terminator, 1);
	if (terminator != 0x00) {
		panic("expected block terminator 0x00, got 0x%x", terminator);
	}
}

void readblock_image_descriptor(gif_t *g) {
	reader.t *r = g->r;
	// Image descriptor begins with 2C.
	uint8_t byte = 0;
	reader.read(r, &byte, 1);
	if (byte != 0x2c) {
		panic("expected 0x2c, got 0x%x", byte);
	}

	// Image position on the canvas.
	uint16_t left = 0;
	uint16_t top = 0;
	endian.read2le(r, &left);
	endian.read2le(r, &top);
	// printf("left = %u, top = %u\n", left, top);
	if (left != 0 || top != 0) {
		panic("expected left and top to be zero");
	}

	endian.read2le(r, &g->width);
	endian.read2le(r, &g->height);

	reader.read(r, &byte, 1);
	uint8_t colorinfo[8] = {};
	bits.getbits_msfirst(byte, colorinfo);
	bool colortable = colorinfo[0] == 1;
	bool interlace = colorinfo[1] == 1;
	bool sort = colorinfo[2] == 1;
	uint8_t colortable_size = 4*colorinfo[5] + 2*colorinfo[6] + colorinfo[7];
	printf("colortable = %d, interlace = %d, sort = %d, size = %d\n", colortable, interlace, sort, colortable_size);
}

image.image_t *readblock_image_data(gif_t *g) {
	reader.t *r = g->r;

	// This value tells how many color indexes to allocate in the
	// decoder table. It's a power of two.
	uint8_t sizearg = 0;
	reader.read(r, &sizearg, 1);
	size_t alphabet_size = 1 << sizearg;

	// Read all compressed sub-blocks.
	// A sub-block is at most 255 bytes.
	size_t total = 0;
	uint8_t buf[4096] = {};
	while (true) {
		uint8_t blocklen = 0;
		reader.read(r, &blocklen, 1);
		if (blocklen == 0) {
			break;
		}
		if (total + blocklen > 4096) {
			panic("static buffer too small for data blocks");
		}
		reader.read(r, buf + total, blocklen);
		total += blocklen;
	}

	// Start reading the compressed data.
	reader.t *in = reader.static_buffer(buf, total);
	lzw.dec_t *dec = lzw.newdecoder(in, alphabet_size);

	// Image data is color index scans, left to right, top to bottom.
	image.image_t *img = image.new(g->width, g->height);
	int totaldec = 0;
	while (true) {
		uint8_t decbuf[4096] = {};
		int n = lzw.decode(dec, decbuf);
		if (n == 0) {
			break;
		}
		for (int i = 0; i < n; i++) {
			uint8_t color_index = decbuf[i];
			int x = totaldec / g->width;
			int y = totaldec % g->width;
			image.rgba_t c = g->color_table[color_index];
			image.set(img, x, y, c);
			totaldec++;
		}
	}
	lzw.freedecoder(dec);
	reader.free(in);
	return img;
}

void readblock_trailer(reader.t *r) {
	uint8_t byte = 0;
	reader.read(r, &byte, 1);
	if (byte != 0x3b) {
		panic("expected 0x3b");
	}
}
