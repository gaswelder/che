void write_u32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }

typedef {
    FILE *f;
    int64_t riff_size_pos;
    int64_t hdrl_size_pos;
    int64_t strl_size_pos;
    int64_t movi_size_pos;
    int64_t movi_start;
    int width, height, fps;
    int frame_count;
} AviWriter;

AviWriter *avi_open(FILE *f, int width, int height, int fps) {
    AviWriter *avi = calloc!(1, sizeof(AviWriter));
    avi->f = f;
    avi->width = width;
    avi->height = height;
    avi->fps = fps;

    // === RIFF header ===
    fwrite("RIFF", 1, 4, avi->f);
    avi->riff_size_pos = ftell(avi->f);
	write_u32(avi->f, 999999); // placeholder
    fwrite("AVI ", 1, 4, avi->f);

    // === LIST hdrl ===
    fwrite("LIST", 1, 4, avi->f);
    avi->hdrl_size_pos = ftell(avi->f);
	write_u32(avi->f, 999999); // placeholder
    fwrite("hdrl", 1, 4, avi->f);

    // ---- avih ----
    fwrite("avih", 1, 4, avi->f); write_u32(avi->f, 56);
    write_u32(avi->f, 1000000 / fps);         // dwMicroSecPerFrame
    write_u32(avi->f, width * height * 3 * fps);
    write_u32(avi->f, 0);                     // padding
    write_u32(avi->f, 0x10);                  // flags
    write_u32(avi->f, 0);                     // total frames (patch later)
    write_u32(avi->f, 0);                     // initial frames
    write_u32(avi->f, 1);                     // streams
    write_u32(avi->f, width * height * 3);    // buffer size
    write_u32(avi->f, width);
    write_u32(avi->f, height);
    for (int i = 0; i < 4; i++) write_u32(avi->f, 0);

    // ---- LIST strl ----
    fwrite("LIST", 1, 4, avi->f);
    avi->strl_size_pos = ftell(avi->f); write_u32(avi->f, 0); // placeholder
    fwrite("strl", 1, 4, avi->f);

    // ------ strh ------
    fwrite("strh", 1, 4, avi->f); write_u32(avi->f, 56);
    fwrite("vids", 1, 4, avi->f);
    fwrite("DIB ", 1, 4, avi->f);
    write_u32(avi->f, 0);
    fwrite("\0\0\0\0", 1, 4, avi->f); // priority+language
    write_u32(avi->f, 0);
    write_u32(avi->f, 1);
    write_u32(avi->f, fps);
    write_u32(avi->f, 0);
    write_u32(avi->f, 0); // length (patch later)
    write_u32(avi->f, width * height * 3);
    write_u32(avi->f, 0xFFFFFFFF);
    write_u32(avi->f, 0);
    fwrite("\0\0\0\0\0\0\0\0", 1, 8, avi->f); // rcFrame

    // ------ strf ------
    fwrite("strf", 1, 4, avi->f); write_u32(avi->f, 40);
    write_u32(avi->f, 40);
    write_u32(avi->f, width);
    write_u32(avi->f, height);
    fwrite("\1\0", 1, 2, avi->f); // planes
    fwrite("\30\0", 1, 2, avi->f); // 24-bit
    write_u32(avi->f, 0);
    write_u32(avi->f, width * height * 3);
    write_u32(avi->f, 0);
    write_u32(avi->f, 0);
    write_u32(avi->f, 0);
    write_u32(avi->f, 0);

    // // Patch sizes
    // int64_t after_strl = ftell(avi->f);
    // fseek(avi->f, avi->strl_size_pos, SEEK_SET);
    // write_u32(avi->f, after_strl - (avi->strl_size_pos + 4));
    // fseek(avi->f, after_strl, SEEK_SET);

    // int64_t after_hdrl = ftell(avi->f);
    // fseek(avi->f, avi->hdrl_size_pos, SEEK_SET);
    // write_u32(avi->f, after_hdrl - (avi->hdrl_size_pos + 4));
    // fseek(avi->f, after_hdrl, SEEK_SET);

    // === LIST movi ===
    fwrite("LIST", 1, 4, avi->f);
    avi->movi_size_pos = ftell(avi->f);
	write_u32(avi->f, 9999999); // placeholder
    fwrite("movi", 1, 4, avi->f);
    avi->movi_start = ftell(avi->f);

    return avi;
}

void avi_add_frame(AviWriter *avi, const uint8_t *data) {
    FILE *f = avi->f;
    fwrite("00db", 1, 4, f);
    uint32_t frame_size = avi->width * avi->height * 3;
    write_u32(f, frame_size);
    fwrite(data, 1, frame_size, f);
    if (frame_size & 1) fputc(0, f);
    avi->frame_count++;
}

// void avi_close(AviWriter *avi) {
//     FILE *f = avi->f;

//     int64_t movi_end = ftell(f);
//     fseek(f, avi->movi_size_pos, SEEK_SET);
//     write_u32(f, movi_end - (avi->movi_size_pos + 4));
//     fseek(f, movi_end, SEEK_SET);

//     // Write index
//     fwrite("idx1", 1, 4, f);
//     write_u32(f, avi->frame_count * 16);

//     int64_t offset = avi->movi_start - 8;
//     for (int i = 0; i < avi->frame_count; i++) {
//         fwrite("00db", 1, 4, f);
//         write_u32(f, 0x10);
//         write_u32(f, offset);
//         write_u32(f, avi->width * avi->height * 3);
//         offset += 8 + avi->width * avi->height * 3;
//         if ((avi->width * avi->height * 3) & 1) offset++;
//     }

//     // Patch RIFF size
//     int64_t file_end = ftell(f);
//     fseek(f, 4, SEEK_SET);
//     write_u32(f, file_end - 8);

//     // Patch frame count in headers
//     fseek(f, 32, SEEK_SET); // dwTotalFrames in avih
//     write_u32(f, avi->frame_count);

//     fseek(f, 140, SEEK_SET); // dwLength in strh
//     write_u32(f, avi->frame_count);

//     fclose(f);
//     free(avi);
// }

int main() {
	// FILE *f = fopen("streamable.avi", "wb");
    AviWriter *avi = avi_open(stdout, 320, 240, 10);

    uint8_t *frame = calloc!(1, 320 * 240 * 3);
    for (int i = 0; i < 100; i++) {
        memset(frame, i * 25, 320 * 240 * 3);
        avi_add_frame(avi, frame);
    }

    // avi_close(avi);
    free(frame);
    return 0;
}
