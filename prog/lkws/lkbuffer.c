/*** LKBuffer - Dynamic bytes buffer ***/
pub typedef {
    char *bytes;        // bytes buffer
    size_t bytes_cur;   // index to current buffer position
    size_t bytes_len;   // length of buffer
    size_t bytes_size;  // capacity of buffer in bytes
} LKBuffer;

#define LK_BUFSIZE_SMALL 512

pub LKBuffer *lk_buffer_new(size_t bytes_size) {
    if (bytes_size == 0) {
        bytes_size = LK_BUFSIZE_SMALL;
    }

    LKBuffer *buf = calloc(1, sizeof(LKBuffer));
    buf->bytes_cur = 0;
    buf->bytes_len = 0;
    buf->bytes_size = bytes_size;
    buf->bytes = calloc(1, buf->bytes_size);
    return buf;
}

pub void lk_buffer_free(LKBuffer *buf) {
    assert(buf->bytes != NULL);
    free(buf->bytes);
    buf->bytes = NULL;
    free(buf);
}

pub void lk_buffer_resize(LKBuffer *buf, size_t bytes_size) {
    buf->bytes_size = bytes_size;
    if (buf->bytes_len > buf->bytes_size) {
        buf->bytes_len = buf->bytes_size;
    }
    if (buf->bytes_cur >= buf->bytes_len) {
        buf->bytes_cur = buf->bytes_len-1;
    }
    buf->bytes = realloc(buf->bytes, buf->bytes_size);
}

pub void lk_buffer_clear(LKBuffer *buf) {
    memset(buf->bytes, 0, buf->bytes_len);
    buf->bytes_len = 0;
    buf->bytes_cur = 0;
}

pub int lk_buffer_append(LKBuffer *buf, char *bytes, size_t len) {
    // If not enough capacity to append bytes, expand the bytes buffer.
    if (len > buf->bytes_size - buf->bytes_len) {
        char *bs = realloc(buf->bytes, buf->bytes_size + len);
        if (bs == NULL) {
            return -1;
        }
        buf->bytes = bs;
        buf->bytes_size += len;
    }
    memcpy(buf->bytes + buf->bytes_len, bytes, len);
    buf->bytes_len += len;

    assert(buf->bytes != NULL);
    return 0;
}

pub int lk_buffer_append_sz(LKBuffer *buf, char *s) {
    return lk_buffer_append(buf, s, strlen(s));
}

#define LK_BUFSIZE_MEDIUM 1024

pub void lk_buffer_append_sprintf(LKBuffer *buf, const char *format, ...) {
    va_list args = {};
	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (len < 0) {
        abort();
    }
	char *tmp = calloc(len + 1, 1);
	if (!tmp) {
        abort();
    }
	va_start(args, format);
	vsnprintf(tmp, len + 1, format, args);
	va_end(args);
    lk_buffer_append(buf, tmp, len);
    free(tmp);
}

// Read CR-terminated line from buffer starting from buf->bytes_cur position.
// buf->bytes_cur is updated to point to the first char of next line.
// If dst doesn't have enough chars for line, partial line is copied.
// Returns number of bytes read.
pub size_t lk_buffer_readline(LKBuffer *buf, char *dst, size_t dst_len) {
    assert(dst_len > 2); // Reserve space for \n and \0.

    size_t nread = 0;
    while (nread < dst_len-1) { // leave space for null terminator
        if (buf->bytes_cur >= buf->bytes_len) {
            break;
        }
        dst[nread] = buf->bytes[buf->bytes_cur];
        nread++;
        buf->bytes_cur++;

        if (dst[nread-1] == '\n') {
            break;
        }
    }

    assert(nread <= dst_len-1);
    dst[nread] = '\0';
    return nread;
}
