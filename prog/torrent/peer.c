#import dbg
#import files.c
#import formats/torrent
#import ioloop.c
#import peerproto.c
#import reader
#import writer
#import enc/hex

torrent.info_t *_tf = NULL;
uint8_t *_peer_id = NULL;
uint8_t tmpbuf1[1000000] = {};
uint8_t tmpbuf2[1000000] = {};
const char *outdir = "download";

pub typedef {
    int foo;
} init_t;

const char *TAG = "peer";

pub bool init(torrent.info_t *tf, uint8_t *peer_id) {
	_tf = tf;
	_peer_id = peer_id;
	return true;
}

enum {
	WAITING_HANDSHAKE,
    READING_MESSAGE_LENGTH,
}

typedef {
    int state;

    uint8_t stash[1000000];
    size_t stashlen;

    bool is_downloader;
    size_t next_req_piece;
} peer_t;

void shift(peer_t *p, size_t n) {
    if (n > p->stashlen) panic("incorrect shift: have %zu, shifting %zu", p->stashlen, n);
    memmove(p->stash, p->stash+n, p->stashlen - n);
    p->stashlen -= n;
}
void append(peer_t *p, char *data, size_t n) {
    if (p->stashlen + n > sizeof(p->stash)) {
        panic("stash too small");
    }
    memcpy(p->stash + p->stashlen, data, n);
    p->stashlen += n;
}

void setname(void *ctx, char *NAME) {
    peer_t *state = ioloop.get_stash(ctx);
    if (state) {
        if (state->is_downloader) {
            sprintf(NAME, "downloader (%d)", state->state);
        } else {
            sprintf(NAME, "seeder (%d)", state->state);
        }
    } else {
        sprintf(NAME, "peer (-)");
    }
}


pub void handle(void *ctx, int event, void *edata) {
    torrent.info_t *tf = _tf;
    uint8_t *peer_id = _peer_id;

    char NAME[20] = {};
    setname(ctx, NAME);

    switch (event) {
        case ioloop.CONNECT_FAILED: {
			printf("connect failed, err=%s\n", strerror(errno));
		}
        case ioloop.CONNECTED: {
            peer_t *p = calloc!(1, sizeof(peer_t));
            ioloop.set_stash(ctx, p);

            init_t *cfg = edata;
            if (cfg) {
                p->is_downloader = true;

                //
                // send a handshake
                //
				uint8_t buf[1000];
				writer.t *w = writer.static_buffer(buf, sizeof(buf));
                if (peerproto.write_handshake(w, (uint8_t*)tf->infohash_bytes, peer_id) < 0) {
                    panic("write_handshake failed");
                }
                peerproto.write_bitfield(w, tf);
                size_t nw = w->nwritten;
				writer.free(w);
				ioloop.write(ctx, (char *)buf, nw);

				request_pieces(ctx);
            } else {
                printf("%s: new connection, init=NULL\n", NAME);
            }
        }
        case ioloop.EXIT: {
            peer_t *p = ioloop.get_stash(ctx);
            free(p);
        }
        case ioloop.DATA_IN: {
            ioloop.buff_t *b = edata;
            peer_t *state = ioloop.get_stash(ctx);

            append(state, b->data, b->len);
            // dbg.print_bytes((uint8_t *)b->data, b->len);

            setname(ctx, NAME);
            while (true) {
                // Read a message. If no message could be read, stop.
                req_t req = {};
                reader.t *r = reader.static_buffer(state->stash, state->stashlen);
                bool ok = read_request(r, state, &req);
                size_t nread = r->pos;
                reader.free(r);
                if (!ok) break;

                // Process the message and write the response.
                // He we're being optimistic about the outbox having enough
                // space for the response.
                writer.t *w = writer.static_buffer(tmpbuf2, sizeof(tmpbuf2));
                write_response(ctx, w, state, &req);
                size_t nwritten = w->nwritten;
                if (state->state == WAITING_HANDSHAKE) {
                    state->state = READING_MESSAGE_LENGTH;
                }
                writer.free(w);
                shift(state, nread);
                if (nwritten > 0) {
                    if (!ioloop.write(ctx, (char *)tmpbuf2, nwritten)) panic("write failed");
                }
            }
        }
        case ioloop.WRITE_FINISHED: {}
        default: {
            panic("peer: event=%d, data=%p\n", event, edata);
        }
    }
}

enum { HANDSHAKE, KEEPALIVE, MSG }

typedef {
    int type;
    char data[10000];
} req_t;

int read_request(reader.t *r, peer_t *state, req_t *req) {
    if (state->state == WAITING_HANDSHAKE) {
        if (state->stashlen < 68) {
            return 0;
        }
        req->type = HANDSHAKE;
        peerproto.handshake_t *hs = (void *) req->data;
        int c = peerproto.read_handshake(r, hs);
        printf("Got a handshake, proto = %s\n", (char *)hs->proto);
        printf("infohash: ");
        hex.write(writer.stdout(), (char *) hs->infohash, 20);
        printf("\npeer id: ");
        hex.write(writer.stdout(), (char *) hs->peer_id, 20);
        printf("\n");
        return c;
    }
    if (state->state == READING_MESSAGE_LENGTH) {
        if (state->stashlen < 4) {
            return 0;
        }

        // hack: make sure there's a complete message in stash.
        reader.t *tmpr = reader.static_buffer(state->stash, state->stashlen);
        uint32_t len = peerproto.read_length(tmpr);
        bool ok = (state->stashlen >= 4 + len);
        reader.free(tmpr);
        if (!ok) {
            return 0;
        }
        // ----

        peerproto.read_length(r);
        if (len == 0) {
            req->type = KEEPALIVE;
        } else {
            peerproto.msg_t msg = {};
            peerproto.read_message(r, len, &msg);
            req->type = MSG;
            memcpy(req->data, &msg, sizeof(msg));
        }
        return 123;
    }
    panic("!");
}

void write_response(void *ctx, writer.t *w, peer_t *state, req_t *req) {
    torrent.info_t *tf = _tf;

    if (req->type == HANDSHAKE) {
        uint8_t *peer_id = _peer_id;
        // The downloader started the handshake, the seeder responds.
        if (!state->is_downloader) {
            dbg.m(TAG, "responding with a handshake");
            if (peerproto.write_handshake(w, (uint8_t *)tf->infohash_bytes, peer_id) < 0) {
                panic("write handshake failed");
            }
        }
        return;
    }
    if (req->type == KEEPALIVE) {
        dbg.m(TAG, "got heartbeat, responding");
        peerproto.write_keepalive(w);
        return;
    }
    if (req->type != MSG) {
        panic("unknown req type: %d", req->type);
    }
    peerproto.msg_t *msg = (void *) req->data;
    switch (msg->id) {
        case peerproto.MSG_BITFIELD: {
            bool *flags = (void*) msg->data;
            size_t npieces = torrent.npieces(tf);
            for (size_t i = 0; i < npieces; i++) {
                printf("peer has %zu: %d\n", i, flags[i]);
            }
        }
        case peerproto.MSG_REQUEST: {
            peerproto.msg_request_t *req = (void *) msg->data;
            dbg.m(TAG, "got read request: (%u, %u, %u)", req->index, req->begin, req->length);
            files.range_t freq = {
                .index = req->index,
                .begin = req->begin,
                .length = req->length
            };
            files.readslice(".", tf, freq, tmpbuf1);
            if (peerproto.write_piece(w, freq, tmpbuf1) < 0) {
                panic("oops, writer pos is %zu", w->nwritten);
            }
        }
        case peerproto.MSG_PIECE: {
            peerproto.msg_piece_t *piece = (void*) msg->data;
            dbg.m(TAG, "got slice (%u, %u, %u)", piece->index, piece->begin, piece->length);
            files.range_t freq = {
                .index = piece->index,
                .begin = piece->begin,
                .length = piece->length
            };
            files.writeslice(outdir, tf, freq, piece->data);
            if (files.check_piece(outdir, tf, freq.index)) {
                printf("piece %u OK\n", freq.index);
                request_pieces(ctx);
            }
        }
        default: {
            panic("!");
        }
    }
}

// void dumpdata(uint8_t *data, size_t n) {
//     char path[100];
//     sprintf(path, "peer-%ld.bin", time(NULL));
//     fs.writefile(path, (char *)data, n);
// }

void request_pieces(void *ctx) {
    peer_t *state = ioloop.get_stash(ctx);
    torrent.info_t *tf = _tf;

    printf("next piece to request: %zu\n", state->next_req_piece);
    size_t slice_size = 16 * 1024;
    size_t i = state->next_req_piece;
    if (i >= torrent.npieces(tf)) {
        printf("no more pieces to request\n");
        return;
    }
    size_t piecelen = files.get_piece_length(tf, i);
    size_t c = 0;
    writer.t *w = writer.static_buffer(tmpbuf1, sizeof(tmpbuf1));
    for (size_t pos = 0; pos < piecelen; pos += slice_size) {
        if (pos + slice_size > piecelen) {
            slice_size = piecelen - pos;
        }
        files.range_t req = {
            .index = i,
            .begin = pos,
            .length = slice_size
        };
        int r = peerproto.write_request(w, req);
        if (r < 0) {
            panic("write_request failed");
        }
        c += r;
    }
    state->next_req_piece++;
    writer.free(w);
    ioloop.write(ctx, (char *)tmpbuf1, c);
}
