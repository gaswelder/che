#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#import lkbuffer.c

#define LK_BUFSIZE_LARGE 2048

enum {FD_SOCK, FD_FILE};

pub enum {
    Z_OPEN = 1, // fd still open
    Z_EOF = 0, // end of file
    Z_ERR = -1, // error, check errno
    Z_BLOCK = -2 // fd blocked, no data
};


// Read nonblocking fd count bytes to buf.
// Returns one of the following:
//    1 (Z_OPEN) for socket OS.open
//    0 (Z_EOF) for EOF
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
// On return, nbytes contains the number of bytes read.
pub int lk_read(int fd, int fd_type, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    int z;
    char readbuf[LK_BUFSIZE_LARGE];

    size_t nread = 0;
    while (nread < count) {
        // read minimum of sizeof(readbuf) and count-nread
        int nblock = count-nread;
        if ((size_t)nblock > sizeof(readbuf)) nblock = sizeof(readbuf);

        if (fd_type == FD_SOCK) {
            z = OS.recv(fd, readbuf, nblock, OS.MSG_DONTWAIT);
        } else {
            z = OS.read(fd, readbuf, nblock);
        }
        // EOF
        if (z == 0) {
            z = Z_EOF;
            break;
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == OS.EINTR) {
            continue;
        }
        if (z == -1 && (errno == OS.EAGAIN || errno == OS.EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z > 0);
        lkbuffer.lk_buffer_append(buf, readbuf, z);
        nread += z;
    }
    if (z > 0) {
        z = Z_OPEN;
    }
    *nbytes = nread;
    return z;
}

// Read all available nonblocking fd bytes to buffer.
// Returns one of the following:
//    0 (Z_EOF) for EOF
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
//
// Note: This keeps track of last buf position read.
// Used to cumulatively read data into buf.
pub int lk_read_all(int fd, int fd_type, lkbuffer.LKBuffer *buf) {
    int z;
    char readbuf[LK_BUFSIZE_LARGE];
    while (1) {
        if (fd_type == FD_SOCK) {
            z = OS.recv(fd, readbuf, sizeof(readbuf), OS.MSG_DONTWAIT);
        } else {
            z = OS.read(fd, readbuf, sizeof(readbuf));
        }
        // EOF
        if (z == 0) {
            z = Z_EOF;
            break;
        }
        if (z == -1 && errno == OS.EINTR) {
            continue;
        }
        if (z == -1 && (errno == OS.EAGAIN || errno == OS.EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z > 0);
        lkbuffer.lk_buffer_append(buf, readbuf, z);
    }
    assert(z <= 0);
    return z;
}

pub int lk_read_all_file(int fd, lkbuffer.LKBuffer *buf) {
    return lk_read_all(fd, FD_FILE, buf);
}


// Write count buf bytes to nonblocking fd.
// Returns one of the following:
//    1 (Z_OPEN) for socket OS.open
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
// On return, nbytes contains the number of bytes written.
//
// Note: use OS.open(O_NONBLOCK) to OS.open nonblocking file.
pub int lk_write(int fd, int fd_type, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    int z;
    if (count > buf->bytes_len) {
        count = buf->bytes_len;
    }
    size_t nwrite = 0;
    while (nwrite < count) {
        if (fd_type == FD_SOCK) {
            z = OS.send(fd, buf->bytes+nwrite, count-nwrite, OS.MSG_DONTWAIT | OS.MSG_NOSIGNAL);
        } else {
            z = OS.write(fd, buf->bytes+nwrite, count-nwrite);
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == OS.EINTR) {
            continue;
        }
        if (z == -1 && (errno == OS.EAGAIN || errno == OS.EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z >= 0);
        nwrite += z;
    }
    if (z >= 0) {
        z = Z_OPEN;
    }
    *nbytes = nwrite;
    return z;
}

// Write buf bytes to nonblocking fd.
// Returns one of the Z_* statuses.
//
// Note: This keeps track of last buf position written.
// Used to cumulatively write data into buf.
pub int lk_write_all(int fd, int fd_type, lkbuffer.LKBuffer *buf) {
    int z;
    while (1) {
        int nwrite = buf->bytes_len - buf->bytes_cur;
        if (nwrite <= 0) {
            z = Z_EOF;
            break;
        }
        if (fd_type == FD_SOCK) {
            z = OS.send(fd,
                     buf->bytes + buf->bytes_cur, 
                     buf->bytes_len - buf->bytes_cur,
                     OS.MSG_DONTWAIT | OS.MSG_NOSIGNAL);
        } else {
            z = OS.write(fd,
                      buf->bytes + buf->bytes_cur, 
                      buf->bytes_len - buf->bytes_cur);
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == OS.EINTR) {
            continue;
        }
        if (z == -1 && (errno == OS.EAGAIN || errno == OS.EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z >= 0);
        buf->bytes_cur += z;
    }
    if (z > 0) {
        z = Z_OPEN;
    }
    return z;
}
pub int lk_write_all_sock(int fd, lkbuffer.LKBuffer *buf) {
    return lk_write_all(fd, FD_SOCK, buf);
}
pub int lk_write_all_file(int fd, lkbuffer.LKBuffer *buf) {
    return lk_write_all(fd, FD_FILE, buf);
}
