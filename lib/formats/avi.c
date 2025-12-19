#import enc/endian
#import image
#import writer

pub typedef {
	writer.t *out;
    int64_t riff_size_pos;
    int64_t hdrl_size_pos;
    int64_t strl_size_pos;
    int64_t movi_size_pos;
    int64_t movi_start;
    int width, height, fps;
    int frame_count;
} writer_t;

// Starts an AVI stream writing into f.
pub writer_t *start(FILE *f, int width, height, fps) {
	writer.t *w = writer.file(f);
    writer_t *avi = calloc!(1, sizeof(writer_t));
	avi->out = w;
    avi->width = width;
    avi->height = height;
    avi->fps = fps;

	// Begin RIFF
	writer.write(w, (uint8_t *)"RIFF", 4);
    avi->riff_size_pos = w->nwritten;
	
	endian.write4le(w, 999999); // placeholder

	writer.write(w, (uint8_t *) "AVI ", 4);
	writer.write(w, (uint8_t *) "LIST", 4);
    avi->hdrl_size_pos = w->nwritten;
	endian.write4le(w, 999999); // placeholder
	writer.write(w, (uint8_t *) "hdrl", 4);

	// avi header
	writer.write(w, (uint8_t *) "avih", 4);
	endian.write4le(w, 56);
    endian.write4le(w, 1000000 / fps);         // dwMicroSecPerFrame
    endian.write4le(w, width * height * 3 * fps);
    endian.write4le(w, 0);                     // padding
    endian.write4le(w, 0x10);                  // flags
    endian.write4le(w, 0);                     // total frames (patch later)
    endian.write4le(w, 0);                     // initial frames
    endian.write4le(w, 1);                     // streams
    endian.write4le(w, width * height * 3);    // buffer size
    endian.write4le(w, width);
    endian.write4le(w, height);
    for (int i = 0; i < 4; i++) {
		endian.write4le(w, 0);
	}

	writer.write(w, (uint8_t *) "LIST", 4);
    avi->strl_size_pos = w->nwritten;
	endian.write4le(w, 0); // placeholder
	writer.write(w, (uint8_t *) "strl", 4);

	writer.write(w, (uint8_t *) "strh", 4);
	endian.write4le(w, 56);
	writer.write(w, (uint8_t *) "vids", 4);
	writer.write(w, (uint8_t *) "DIB ", 4);
    endian.write4le(w, 0);
	writer.write(w, (uint8_t *) "\0\0\0\0", 4); // priority+language
    endian.write4le(w, 0);
    endian.write4le(w, 1);
    endian.write4le(w, fps);
    endian.write4le(w, 0);
    endian.write4le(w, 0); // length (patch later)
    endian.write4le(w, width * height * 3);
    endian.write4le(w, 0xFFFFFFFF);
    endian.write4le(w, 0);
	writer.write(w, (uint8_t *) "\0\0\0\0\0\0\0\0", 8); // rcFrame

	//
    writer.write(w, (uint8_t *) "strf", 4);
	endian.write4le(w, 40);
    endian.write4le(w, 40);
    endian.write4le(w, width);
    endian.write4le(w, height);
    writer.write(w, (uint8_t *) "\1\0", 2); // planes
    writer.write(w, (uint8_t *) "\30\0", 2); // 24-bit
    endian.write4le(w, 0);
    endian.write4le(w, width * height * 3);
    endian.write4le(w, 0);
    endian.write4le(w, 0);
    endian.write4le(w, 0);
    endian.write4le(w, 0);

	//
    writer.write(w, (uint8_t *) "LIST", 4);
    avi->movi_size_pos = w->nwritten;
	endian.write4le(w, 9999999); // placeholder
    writer.write(w, (uint8_t *) "movi", 4);
    avi->movi_start = w->nwritten;

    return avi;
}

pub void addframe(writer_t *avi, image.image_t *img) {
	if (img->width != avi->width || img->height != avi->height) {
		panic("frame image size mismatch");
	}
	int width = avi->width;
	int height = avi->height;

	uint8_t *frame = calloc!(1, width * height * 3);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int pos = y * (width*3) + (x*3);
			image.rgba_t c = image.get(img, x, y);
			frame[pos++] = c.red;
			frame[pos++] = c.green;
			frame[pos++] = c.blue;
		}
	}

	writer.t *w = avi->out;
	writer.write(w, (uint8_t *) "00db", 4);
    uint32_t frame_size = avi->width * avi->height * 3;
    endian.write4le(w, frame_size);
	writer.write(w, frame, frame_size);
    if (frame_size & 1) {
		writer.writebyte(w, 0);
	}
    avi->frame_count++;
	free(frame);
}

pub void stop(writer_t *avi) {
	writer.t *w = avi->out;

    // int64_t movi_end = ftell(f);
    // fseek(f, avi->movi_size_pos, SEEK_SET);
    // endian.write4le(w, movi_end - (avi->movi_size_pos + 4));
    // fseek(f, movi_end, SEEK_SET);

	writer.write(w, (uint8_t *) "idx1", 4);
    endian.write4le(w, avi->frame_count * 16);

    int64_t offset = avi->movi_start - 8;
    for (int i = 0; i < avi->frame_count; i++) {
        writer.write(w, (uint8_t *) "00db", 4);
        endian.write4le(w, 0x10);
        endian.write4le(w, offset);
        endian.write4le(w, avi->width * avi->height * 3);
        offset += 8 + avi->width * avi->height * 3;
        if ((avi->width * avi->height * 3) & 1) offset++;
    }

    // // Patch RIFF size
    // int64_t file_end = ftell(f);
    // fseek(f, 4, SEEK_SET);
    // endian.write4le(w, file_end - 8);

    // // Patch frame count in headers
    // fseek(f, 32, SEEK_SET); // dwTotalFrames in avih
    // endian.write4le(w, avi->frame_count);

    // fseek(f, 140, SEEK_SET); // dwLength in strh
    // endian.write4le(w, avi->frame_count);

	writer.free(w);
    free(avi);
}
