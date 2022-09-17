#import endian

/**
 * The type of PNG image. It determines how the pixels are stored.
 */
pub enum {
    /**
     * 256 shades of gray, 8bit per pixel
     */
    PNG_GRAYSCALE = 0,
    /**
     * 24bit RGB values
     */
    PNG_RGB = 2,
    /**
     * Up to 256 RGBA palette colors, 8bit per pixel
     */
    PNG_PALETTE = 3,
    /**
     * 256 shades of gray plus alpha channel, 16bit per pixel
     */
    PNG_GRAYSCALE_ALPHA = 4,
    /**
     * 24bit RGB values plus 8bit alpha channel
     */
    PNG_RGBA = 6
};

/**
 * This struct holds the internal state of the PNG. The members should never be used directly.
 */
pub typedef {
    int type;      /**< File type */
    size_t capacity;             /**< Reserved memory for raw data */
    uint8_t *data;                  /**< Raw pixel data, format depends on type */
    uint32_t *palette;           /**< Palette for image */
    size_t palette_length;       /**< Entries for palette, 0 if unused */
    size_t width;                /**< Image width */
    size_t height;               /**< Image height */

    char *out;                   /**< Buffer to store final PNG */
    size_t out_pos;              /**< Current size of output buffer */
    size_t out_capacity;         /**< Capacity of output buffer */
    uint32_t crc;                /**< Currecnt CRC32 checksum */
    uint16_t s1;                 /**< Helper variables for Adler checksum */
    uint16_t s2;                 /**< Helper variables for Adler checksum */
    size_t bpp;                  /**< Bytes per pixel */

    size_t stream_x;             /**< Current x coordinate for pixel streaming */
    size_t stream_y;             /**< Current y coordinate for pixel streaming */
} png_t;

const int ADLER_BASE = 65521;

const uint32_t crc32[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832,
        0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a,
        0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
        0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
        0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4,
        0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074,
        0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525,
        0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
        0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76,
        0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6,
        0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
        0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7,
        0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
        0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330,
        0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

/**
 * Creates a new, empty PNG image of given size in pixels.
 * Returns NULL on error.
 *
 * @param type The type of image. Possible values are
 *                  - PNG_GRAYSCALE (8bit grayscale),
 *                  - PNG_GRAYSCALE_ALPHA (8bit grayscale with 8bit alpha),
 *                  - PNG_PALETTE (palette with up to 256 entries, each 32bit RGBA)
 *                  - PNG_RGB (24bit RGB values)
 *                  - PNG_RGBA (32bit RGB values with alpha)
 */
pub png_t *png_new(size_t width, size_t height, int type) {
    if (SIZE_MAX / 4 / width < height) {
        /* ensure no type leads to an integer overflow */
        return NULL;
    }

    png_t *png = (png_t *) calloc(sizeof(png_t), 1);
    png->width = width;
    png->height = height;
    png->capacity = width * height;
    png->palette_length = 0;
    png->palette = NULL;
    png->out = NULL;
    png->out_capacity = 0;
    png->out_pos = 0;
    png->type = type;
    png->stream_x = 0;
    png->stream_y = 0;

    if (type == PNG_PALETTE) {
        png->palette = (uint32_t *) calloc(256, sizeof(uint32_t));
        if (!png->palette) {
            free(png);
            return NULL;
        }
        png->bpp = 1;
    } else if (type == PNG_GRAYSCALE) {
        png->bpp = 1;
    } else if (type == PNG_GRAYSCALE_ALPHA) {
        png->capacity *= 2;
        png->bpp = 2;
    } else if (type == PNG_RGB) {
        png->capacity *= 4;
        png->bpp = 3;
    } else if (type == PNG_RGBA) {
        png->capacity *= 4;
        png->bpp = 4;
    }

    png->data = calloc(png->capacity, 1);
    if (!png->data) {
        free(png->palette);
        free(png);
        return NULL;
    }
    return png;
}

/**
 * @function png_set_palette
 *
 * @brief Sets the image's palette if the image type is \ref PNG_PALETTE.
 *
 * @param png      Reference to the image
 * @param palette  Color palette, each entry contains a 32bit RGBA value
 * @param length   Number of palette entries
 * @return 0 on success, 1 if the palette contained more than 256 entries
 */
pub int png_set_palette(png_t *png, uint32_t *palette, size_t length) {
    if (length > 256) {
        return 1;
    }
    memmove(png->palette, palette, length * sizeof(uint32_t));
    png->palette_length = length;
    return 0;
}

/**
 * Sets the pixel's color at coordinate (x, y).

 * @param color The pixel value, depending on the type this is
 *              - the 8bit palette index (\ref PNG_PALETTE)
 *              - the 8bit gray value (\ref PNG_GRAYSCALE)
 *              - a 16bit value where the lower 8bit are the gray value and
 *                the upper 8bit are the opacity (\ref PNG_GRAYSCALE_ALPHA)
 *              - a 24bit RGB value (\ref PNG_RGB)
 *              - a 32bit RGBA value (\ref PNG_RGBA)
 * @note If the coordinates are not within the bounds of the image,
 *       the functions does nothing.
 */
pub void png_set_pixel(png_t *png, size_t x, size_t y, uint32_t color) {
    if (x >= png->width || y >= png->height) {
        return;
    }
    if (png->type == PNG_PALETTE || png->type == PNG_GRAYSCALE) {
        png->data[x + y * png->width] = (uint8_t) color;
    } else if (png->type == PNG_GRAYSCALE_ALPHA) {
        set_u16le(png->data, 2 * (x + y * png->width), (uint16_t) color);
    } else {
        set_u32le(png->data, 4 * (x + y * png->width), color);
    }
}

/**
 * Returns the pixel color of image `png` at position (`x`, `y`).
 *
 * Depending on the type this is
 * - the 8bit palette index (PNG_PALETTE)
 * - the 8bit gray value (PNG_GRAYSCALE)
 * - a 16bit value where the lower 8bit are the gray value and the upper 8bit
 *   are the opacity (PNG_GRAYSCALE_ALPHA)
 * - a 24bit RGB value (PNG_RGB)
 * - a 32bit RGBA value (PNG_RGBA)
 * - 0 if the coordinates are out of bounds
 */
pub uint32_t png_get_pixel(png_t* png, size_t x, size_t y) {
    if (x >= png->width || y >= png->height) {
        return 0;
    }
    if (png->type == PNG_PALETTE || png->type == PNG_GRAYSCALE) {
        return (uint32_t) png->data[x + y * png->width];
    }
    if (png->type == PNG_GRAYSCALE_ALPHA) {
        return (uint32_t) get_u16le(png->data, 2 * (x + y * png->width));
    }
    return get_u32le(png->data, 4 * (x + y * png->width));
}

/**
 * @function png_start_stream
 *
 * @brief Set the start position for a batch of pixels
 *
 * @param png  Reference to the image
 * @param x    X coordinate
 * @param y    Y coordinate
 *
 * @see png_put_pixel
 */
pub void png_start_stream(png_t* png, size_t x, size_t y) {
    if(!png || x >= png->width || y >= png->height) {
        return;
    }
    png->stream_x = x;
    png->stream_y = y;
}

/**
 * @function png_put_pixel
 *
 * @brief Sets the pixel of the current pixel within a stream and advances to the next pixel
 *
 * @param png   Reference to the image
 * @param color The pixel value, depending on the type this is
 *              - the 8bit palette index (\ref PNG_PALETTE)
 *              - the 8bit gray value (\ref PNG_GRAYSCALE)
 *              - a 16bit value where the lower 8bit are the gray value and
 *                the upper 8bit are the opacity (\ref PNG_GRAYSCALE_ALPHA)
 *              - a 24bit RGB value (\ref PNG_RGB)
 *              - a 32bit RGBA value (\ref PNG_RGBA)
 */
pub void png_put_pixel(png_t* png, uint32_t color) {
    size_t x = png->stream_x;
    size_t y = png->stream_y;
    if (png->type == PNG_PALETTE || png->type == PNG_GRAYSCALE) {
        png->data[x + y * png->width] = (char) (color & 0xff);
    } else if (png->type == PNG_GRAYSCALE_ALPHA) {
        set_u16le(png->data, 2 * (x + y * png->width), (uint16_t) color);
    } else {
        set_u32le(png->data, 4 * (x + y * png->width), color);
    }
    x++;
    if(x >= png->width) {
        x = 0;
        y++;
        if(y >= png->height) {
            y = 0;
        }
    }
    png->stream_x = x;
    png->stream_y = y;
}


uint32_t png_swap32(uint32_t num) {
    return ((num >> 24) & 0xff) |
           ((num << 8) & 0xff0000) |
           ((num >> 8) & 0xff00) |
           ((num << 24) & 0xff000000);
}

uint32_t png_crc(uint8_t *data, size_t len, uint32_t crc) {
    size_t i;
    for (i = 0; i < len; i++) {
        crc = crc32[(crc ^ data[i]) & 255] ^ (crc >> 8);
    }
    return crc;
}


void png_out_raw_write(png_t *png, const char *data, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        png->out[png->out_pos++] = data[i];
    }
}


void png_out_raw_uint(png_t *png, uint32_t val) {
    *(uint32_t *) (png->out + png->out_pos) = val;
    png->out_pos += 4;
}


void png_out_raw_uint16(png_t *png, uint16_t val) {
    *(uint16_t *) (png->out + png->out_pos) = val;
    png->out_pos += 2;
}


void png_out_raw_uint8(png_t *png, uint8_t val) {
    *(uint8_t *) (png->out + png->out_pos) = val;
    png->out_pos++;
}


void png_new_chunk(png_t *png, const char *name, size_t len) {
    png->crc = 0xffffffff;
    png_out_raw_uint(png, png_swap32((uint32_t) len));
    png->crc = png_crc((uint8_t *) name, 4, png->crc);
    png_out_raw_write(png, name, 4);
}


void png_end_chunk(png_t *png) {
    png_out_raw_uint(png, png_swap32(~png->crc));
}


void png_out_uint32(png_t *png, uint32_t val) {
    png->crc = png_crc((uint8_t *) &val, 4, png->crc);
    png_out_raw_uint(png, val);
}


void png_out_uint16(png_t *png, uint16_t val) {
    png->crc = png_crc((uint8_t *) &val, 2, png->crc);
    png_out_raw_uint16(png, val);
}


void png_out_uint8(png_t *png, uint8_t val) {
    png->crc = png_crc((uint8_t *) &val, 1, png->crc);
    png_out_raw_uint8(png, val);
}


void png_out_write(png_t *png, const char *data, size_t len) {
    png->crc = png_crc((uint8_t *) data, len, png->crc);
    png_out_raw_write(png, data, len);
}


void png_out_write_adler(png_t *png, uint8_t data) {
    png_out_write(png, (char *) &data, 1);
    png->s1 = (uint16_t) ((png->s1 + data) % ADLER_BASE);
    png->s2 = (uint16_t) ((png->s2 + png->s1) % ADLER_BASE);
}


void png_pixel_header(png_t *png, size_t offset, size_t bpl) {
    if (offset > bpl) {
        /* not the last line */
        png_out_write(png, "\0", 1);
        png_out_uint16(png, (uint16_t) bpl);
        png_out_uint16(png, (uint16_t) ~bpl);
    } else {
        /* last line */
        png_out_write(png, "\1", 1);
        png_out_uint16(png, (uint16_t) offset);
        png_out_uint16(png, (uint16_t) ~offset);
    }
}

/**
 * @function png_get_data
 *
 * @brief Returns the image as PNG data stream
 *
 * @param png  Reference to the image
 * @param len  The length of the data stream is written to this output parameter
 * @return A reference to the PNG output stream
 * @note The data stream is free'd when calling \ref png_destroy and
 *       must not be free'd be the caller
 */
char *png_get_data(png_t *png, size_t *len) {
    size_t index, bpl, raw_size, size, p, pos, corr;
    uint8_t *pixel;
    if (!png) {
        return NULL;
    }
    if (png->out) {
        /* delete old output if any */
        free(png->out);
    }
    png->out_capacity = png->capacity + 4096 * 8 + png->width * png->height;
    png->out = (char *) calloc(png->out_capacity, 1);
    png->out_pos = 0;
    if (!png->out) {
        return NULL;
    }

    png_out_raw_write(png, "\211PNG\r\n\032\n", 8);

    /* IHDR */
    png_new_chunk(png, "IHDR", 13);
    png_out_uint32(png, png_swap32((uint32_t) (png->width)));
    png_out_uint32(png, png_swap32((uint32_t) (png->height)));
    png_out_uint8(png, 8); /* bit depth */
    png_out_uint8(png, (uint8_t) png->type);
    png_out_uint8(png, 0); /* compression */
    png_out_uint8(png, 0); /* filter */
    png_out_uint8(png, 0); /* interlace method */
    png_end_chunk(png);

    /* palette */
    if (png->type == PNG_PALETTE) {
        char entry[3];
        size_t s = png->palette_length;
        if (s < 16) {
            s = 16; /* minimum palette length */
        }
        png_new_chunk(png, "PLTE", 3 * s);
        for (index = 0; index < s; index++) {
            entry[0] = (char) (png->palette[index] & 255);
            entry[1] = (char) ((png->palette[index] >> 8) & 255);
            entry[2] = (char) ((png->palette[index] >> 16) & 255);
            png_out_write(png, entry, 3);
        }
        png_end_chunk(png);

        /* transparency */
        png_new_chunk(png, "tRNS", s);
        for (index = 0; index < s; index++) {
            entry[0] = (char) ((png->palette[index] >> 24) & 255);
            png_out_write(png, entry, 1);
        }
        png_end_chunk(png);
    }

    /* data */
    bpl = 1 + png->bpp * png->width;
    if(bpl >= 65536) {
        fprintf(stderr, "[png] ERROR: maximum supported width for this type of PNG is %d pixel\n", (int)(65535 / png->bpp));
        return NULL;
    }
    raw_size = png->height * bpl;
    size = 2 + png->height * (5 + bpl) + 4;
    png_new_chunk(png, "IDAT", size);
    png_out_write(png, "\170\332", 2);

    pixel = (uint8_t *) png->data;
    png->s1 = 1;
    png->s2 = 0;
    index = 0;
    if (png->type == PNG_RGB) {
        corr = 1;
    } else {
        corr = 0;
    }
    for (pos = 0; pos < png->width * png->height; pos++) {
        if (index == 0) {
            /* line header */
            png_pixel_header(png, raw_size, bpl);
            png_out_write_adler(png, 0); /* no filter */
            raw_size--;
        }

        /* pixel */
        for (p = 0; p < png->bpp; p++) {
            png_out_write_adler(png, *pixel);
            pixel++;
        }
        pixel += corr;

        raw_size -= png->bpp;
        index = (index + 1) % png->width;
    }
    /* checksum */
    png->s1 %= ADLER_BASE;
    png->s2 %= ADLER_BASE;
    png_out_uint32(png, png_swap32((uint32_t) ((png->s2 << 16) | png->s1)));
    png_end_chunk(png);

    /* end of image */
    png_new_chunk(png, "IEND", 0);
    png_end_chunk(png);

    if (len) {
        *len = png->out_pos;
    }
    return png->out;
}

/**
 * @function png_save
 *
 * @brief Saves the image as a PNG file
 *
 * @param png      Reference to the image
 * @param filename Name of the file
 * @return 0 on success, 1 on error
 */
pub int png_save(png_t *png, const char *filename) {
    size_t len;
    FILE* f;
    char *data = png_get_data(png, &len);
    if (!data) {
        return 1;
    }
    f = fopen(filename, "wb");
    if (!f) {
        return 1;
    }
    if (fwrite(data, len, 1, f) != 1) {
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

/**
 * @function png_destroy
 *
 * @brief Destroys the reference to a PNG image and free all associated memory.
 *
 * @param png Reference to the image
 *
 */
pub void png_destroy(png_t *png) {
    if (!png) {
        return;
    }
    free(png->palette);
    png->palette = NULL;
    free(png->out);
    png->out = NULL;
    free(png->data);
    png->data = NULL;
    free(png);
}

/**
 * @brief A minimal C library to write uncompressed PNG files.
 *
 * png is a minimal C library to create uncompressed PNG images.
 * The library supports palette, grayscale as well as raw RGB images all with and without transparency.
 *
 * @author Michael Schwarz
 * @date 29 Jan 2017
 */

// https://github.com/misc0110/libattopng

// MIT License

// Copyright (c) 2018 Michael Schwarz

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.