#import writer
#import reader
#import enc/endian
#import files.c
#import formats/torrent
#import dbg

// choke <len=0001><id=0>
// unchoke <len=0001><id=1>
// interested: <len=0001><id=2>
// not interested: <len=0001><id=3>
// have: <len=0005><id=4><piece index>
// cancel: <len=0013><id=8><index><begin><length>

const char *TAG = "msg";

pub typedef {
    uint8_t proto[20];
    uint8_t infohash[20];
    uint8_t peer_id[20];
} handshake_t;

pub int read_handshake(reader.t *r, handshake_t *hs) {
	uint8_t pstrlen = 0;
    int c = 0;
    c += reader.read(r, &pstrlen, 1);
    if (pstrlen != 19) {
        panic("expected protocol string length 19, got %u", pstrlen);
    }
    c += reader.read(r, hs->proto, pstrlen);
    c += reader.read(r, NULL, 8);
    c += reader.read(r, hs->infohash, 20);
    c += reader.read(r, hs->peer_id, 20);
    if (c < 1 + pstrlen + 8 + 20 + 20) {
        return EOF;
    }
	return c;
}

pub int write_handshake(writer.t *w, uint8_t *infohash, *peer_id) {
    dbg.m(TAG, "write_handshake");
	int r = 0;
	r += writer.writebyte(w, 19);
	r += writer.write(w, (uint8_t *)"BitTorrent protocol\0\0\0\0\0\0\0\0", 19 + 8);
	r += writer.write(w, infohash, 20);
	r += writer.write(w, peer_id, 20);
	if (r != 68) {
		return -1;
	}
	return r;
}

pub enum {
    MSG_BITFIELD = 5,
    MSG_REQUEST = 6,
    MSG_PIECE = 7
}

pub typedef {
    uint8_t id;
    uint8_t data[1000]; // untyped
} msg_t;

pub typedef {
    uint32_t index, begin, length;
} msg_request_t;

pub typedef {
    uint32_t index, begin, length;
    uint8_t *data; // the consumer frees it.
} msg_piece_t;

// Reads a message length, which is a uint32.
// Messages are preceded with their lengths in bytes, not counting the length
// itself. So the parsing is: 1. read length. 2. msg = read <length> bytes.
// 3. parse msg.
pub uint32_t read_length(reader.t *r) {
    uint32_t len;
    endian.read4be(r, &len);
    return len;
}

// Reads a message after the length has been read.
pub void read_message(reader.t *r, uint32_t msglen, msg_t *m) {
    endian.read1(r, &m->id);
    switch (m->id) {
        // index: uint32be, piece index
        // begin: uint32be, byte offset within the piece
        // length: uint32be, data length, typically 16 KB
        case MSG_REQUEST: {
            msg_request_t *req = (void *)m->data;
            endian.read4be(r, &req->index);
            endian.read4be(r, &req->begin);
            endian.read4be(r, &req->length);
        }
        // 4: index
        // 4: offset
        // msglen - (4+4+1): data
        case MSG_PIECE: {
            msg_piece_t *piece = (void *)m->data;
            endian.read4be(r, &piece->index);
            endian.read4be(r, &piece->begin);
            piece->length = msglen-9;
            piece->data = calloc(piece->length, 1);
            reader.read(r, piece->data, piece->length);
        }
        // msglen-1: bytes
        case MSG_BITFIELD: {
            bool *flags = (void *)m->data;
            int n = 0;
            for (uint32_t i = 0; i < msglen-1; i++) {
                uint8_t byte;
                reader.read(r, &byte, 1);
                for (int j = 0; j < 8; j++) {
                    bool set = byte & (1<<(7-j));
                    flags[n++] = set;
                    printf("piece %d: %d\n", i * 8 + j, set);
                }
            }
        }
        default: {
            panic("unknown message: %d", m->id);
        }
    }
}

pub int write_keepalive(writer.t *w) {
    dbg.m(TAG, "write_keepalive");
    return endian.write4be(w, 0);
}

pub int write_request(writer.t *w, files.range_t req) {
    dbg.m(TAG, "write_request");
    int c = 0;
    c += endian.write4be(w, 13); // length
    c += endian.write1(w, MSG_REQUEST);
    c += endian.write4be(w, req.index);
    c += endian.write4be(w, req.begin);
    c += endian.write4be(w, req.length);
    if (c != 4 + 13) return -1;
    return c;
}

pub int write_piece(writer.t *w, files.range_t req, uint8_t *data) {
    dbg.m(TAG, "write_piece");
    int c = 0;
	c += endian.write4be(w, 9 + req.length);
	c += endian.write1(w, MSG_PIECE);
	c += endian.write4be(w, req.index);
	c += endian.write4be(w, req.begin);
    c += writer.write(w, data, req.length);
    if (c != 4 + 9 + (int) req.length) return EOF;
    return c;
}

pub int write_bitfield(writer.t *w, torrent.info_t *tf) {
    dbg.m(TAG, "write_bitfield");
    size_t npieces = torrent.npieces(tf);
    size_t nbytes = npieces / 8;
    if (nbytes * 8 < npieces) nbytes++;
    int c = 0;
    size_t chunksize = 8;
    c += endian.write4be(w, 1 + nbytes);
	c += endian.write1(w, MSG_BITFIELD);
    for (size_t piece = 0; piece < npieces; piece += 8) {
        if (piece + 8 > npieces) chunksize = npieces - piece;
        uint8_t byte = 0;
        for (size_t i = 0; i < chunksize; i++) {
            byte |= 1<<(7-i);
        }
        c += writer.write(w, &byte, 1);
    }
    return c;
}
