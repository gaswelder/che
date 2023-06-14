#import lkalloc.c
#import lkbuffer.c
#import lknet.c

/*** LKSocketReader - Buffered input for sockets ***/
pub typedef {
    int sock;
    lkbuffer.LKBuffer *buf;
    int sockclosed;
} LKSocketReader;

enum {FD_SOCK, FD_FILE};

pub LKSocketReader *lk_socketreader_new(int sock, size_t buf_size) {
    LKSocketReader *sr = lkalloc.lk_malloc(sizeof(LKSocketReader), "lk_socketreader_new");
    if (buf_size == 0) {
        buf_size = 1024;
    }
    sr->sock = sock;
    sr->buf = lkbuffer.lk_buffer_new(buf_size);
    sr->sockclosed = 0;
    return sr;
}

pub void lk_socketreader_free(LKSocketReader *sr) {
    lkbuffer.lk_buffer_free(sr->buf);
    sr->buf = NULL;
    lkalloc.lk_free(sr);
}

// // Read one line from buffered socket including the \n char if present.
// // Function return values:
// // lknet.Z_OPEN (fd still open)
// // lknet.Z_EOF (end of file)
// // Z_ERR (errno set with error detail)
// // Z_BLOCK (fd blocked, no data)
// pub int lk_socketreader_readline(LKSocketReader *sr, lkstring.LKString *line) {
    
//     lkstring.lk_string_assign(line, "");
//     if (sr->sockclosed) {
//         return lknet.Z_EOF;
//     }
//     lkbuffer.LKBuffer *buf = sr->buf;

//     int z = lknet.Z_OPEN;
//     while (1) { // leave space for null terminator
//         // If no buffer chars available, read from socket.
//         if (buf->bytes_cur >= buf->bytes_len) {
//             memset(buf->bytes, '*', buf->bytes_size); // initialize for debugging purposes.
//             z = recv(sr->sock, buf->bytes, buf->bytes_size, MSG_DONTWAIT | MSG_NOSIGNAL);
//             // socket closed, no more data
//             if (z == 0) {
//                 sr->sockclosed = 1;
//                 z = lknet.Z_EOF;
//                 break;
//             }
//             // any other error
//             if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
//                 return Z_BLOCK;
//             }
//             if (z == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
//                 return Z_ERR;
//             }
//             assert(z > 0);
//             buf->bytes_len = z;
//             buf->bytes_cur = 0;
//             z = lknet.Z_OPEN;
//         }

//         // Copy unread buffer bytes into dst until a '\n' char.
//         while (1) {
//             if (buf->bytes_cur >= buf->bytes_len) {
//                 break;
//             }
//             char ch = buf->bytes[buf->bytes_cur];
//             lkstring.lk_string_append_char(line, ch);
//             buf->bytes_cur++;

//             if (ch == '\n') {
//                 assert(z <= lknet.Z_OPEN);
//                 return z;
//             }
//         }
//     }
// }

pub int lk_socketreader_recv(LKSocketReader *sr, lkbuffer.LKBuffer *buf_dest) {
    lkbuffer.lk_buffer_clear(buf_dest);
    lkbuffer.LKBuffer *buf = sr->buf;

    // Append any unread buffer bytes into buf_dest.
    if (buf->bytes_cur < buf->bytes_len) {
        int ncopy = buf->bytes_len - buf->bytes_cur;
        lkbuffer.lk_buffer_append(buf_dest, buf->bytes + buf->bytes_cur, ncopy);
        buf->bytes_cur += ncopy;
    }

    int z = lknet.lk_read_all(sr->sock, FD_SOCK, buf_dest);
    if (z == lknet.Z_EOF) {
        sr->sockclosed = 1;
    }
    return z;
}

void lk_socketreader_debugprint(LKSocketReader *sr) {
    printf("buf_size: %ld\n", sr->buf->bytes_size);
    printf("buf_len: %ld\n", sr->buf->bytes_len);
    printf("buf cur: %ld\n", sr->buf->bytes_cur);
    printf("sockclosed: %d\n", sr->sockclosed);
    debugprint_buf(sr->buf->bytes, sr->buf->bytes_size);
    printf("\n");
}

void debugprint_buf(char *buf, size_t buf_size) {
    printf("buf: ");
    for (int i=0; i < buf_size; i++) {
        putchar(buf[i]);
    }
    putchar('\n');
    printf("buf_size: %ld\n", buf_size);
}