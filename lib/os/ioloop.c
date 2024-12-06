#import clip/buffer
#import os/net
#import dbg

#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#type fd_set
typedef struct timeval timeval_t;

pub typedef {
    char *data;
    size_t len;
} buff_t;

pub typedef void handler_t(void *, int, void *); // ctx, event, eventdata
pub typedef void voidfunc_t();

typedef {
    bool on;
    time_t t;
    voidfunc_t *f;
} ioloop_timer_t;

ioloop_timer_t timeouts[100] = {};

const char *DBG_TAG = "ioloop";

pub enum {
    // The stream has been created and can now be written to and read from.
    // This event can be used as an init signal to set up local state.
    CONNECTED = -4,

    // The stream has been removed from the loop and its memory is about to be
    // freed. This can be seen as the finalizer event corresponding to the init
    // event.
    EXIT = -100,

    // The handler is called with this event to notify that a scheduled stream
    // creation has failed. This is the event where a program would log an
    // error like "failed to connect: ...".
    CONNECT_FAILED = -10,

    // The handler will be called with this event when new data has been read
    // from the stream. The event data will point to a buffer with the data.    
    DATA_IN = -207,

    // The handler will receive this event whenever the outgoing buffer becomes
    // empty. This event can be used to know when the handler can send more data.
    WRITE_FINISHED = -231,
};

const char *eventname(int event) {
    switch (event) {
        case CONNECTED: { return "CONNECTED"; }
        case EXIT: { return "EXIT"; }
        case CONNECT_FAILED: { return "CONNECT_FAILED"; }
        case DATA_IN: { return "DATA_IN"; }
        case WRITE_FINISHED: { return "WRITE_FINISHED"; }
    }
    return "(unknown event)";
}

// Since write doesn't actually write but only stashes data for later sending,
// a program can't just post an infinite amount of data. The size of the outgoing
// buffer is limited, and if there's no slace to the data, write will return false.
// Later, when the buffer is fully shipped, the handler will be notified with a
// special event.
const size_t MAX_OUT_SIZE = 1 * 1024 * 1024 * 1024;




typedef {
    // This client's slot number, to avoid linear searches.
    int slot;

    net.net_t *conn;

    // True if this client is a wrapper for a listening socket.
    bool is_listener;

    // Handler function to be called for this client's events.
    // For a listener client, the handler to assign to new incoming connections.
	handler_t *handler;

    // Buffer for bytes to be written out.
	buffer.t *outgoing;

    bool close;

    // A pointer to associated state that a handler may keep for each client.
    void *stash;
} client_t;

client_t *clients[100] = {};

client_t *addclient(net.net_t *conn, handler_t *h) {
    //
    // Find a free slot.
    //
    int slot = -1;
    for (int i = 0; i < 100; i++) {
        if (!clients[i]) {
            slot = i;
            break;
        }
    }
	if (conn->is_listener) {
		dbg.m(DBG_TAG, "#%d: new listener, %s", slot, conn->addrstr);
	} else {
		dbg.m(DBG_TAG, "#%d: new connection, %s", slot, conn->addrstr);
	}
    if (slot == -1) return false;

    //
    // Create and setup an instance.
    //
    client_t *c = calloc(1, sizeof(client_t));
    if (!c) return false;
    c->conn = conn;
    c->handler = h;
    c->outgoing = buffer.new();
    if (!c->outgoing) {
        panic("failed to allocate buffers");
    }

    //
    // Store and return the instance.
    //
    c->slot = slot;
    clients[slot] = c;
    return c;
}

void removeclient(client_t *c) {
    callhandler(c, EXIT, NULL);
    buffer.free(c->outgoing);
    int slot = c->slot;
    free(c);
    clients[slot] = NULL;
}

// Adds a listener socket with the given handler to process new connections.
pub void listen(const char *addr, handler_t *handler) {
    net.net_t *conn = net.net_listen("tcp", addr);
    client_t *c = addclient(conn, handler);
    if (!conn || !c) panic("listen failed");
    c->is_listener = true;
}

// Schedules a connect task with the handler processing the new connection's
// events. If the connection attempt fails, the handler will be called with
// a corresponding event.
pub void connect(const char *addr, handler_t *handler, void *initdata) {
    net.net_t *conn = net.connect_nonblock("tcp", addr);
    client_t *c = addclient(conn, handler);
    if (!conn) {
        dbg.m(DBG_TAG, "connection failed: %s\n", strerror(errno));
        handler(c, CONNECT_FAILED, NULL);
        removeclient(c);
        return;
    }
    handler(c, CONNECTED, initdata);
}

pub void set_stash(void *ctx, void *stash) {
    client_t *c = ctx;
    c->stash = stash;
}

pub void *get_stash(void *ctx) {
    client_t *c = ctx;
    return c->stash;
}

// Schedules a write operation on the client's socket. The write will happen
// later when it's guaranteed to succeed without blocking.
// Will return false if there is no space for the data in the outbox.
// When the outbox becomes empty, the handler will be called with a
// WRITE_FINISHED event.
pub bool write(void *ctx, char *data, size_t len) {
    client_t *c = ctx;
    if (buffer.size(c->outgoing) + len > MAX_OUT_SIZE) {
        return false;
    }
    return buffer.write(c->outgoing, data, len);
}

// Returns the free space size in the outbox in bytes.
pub size_t write_space(void *ctx) {
	client_t *c = ctx;
	return MAX_OUT_SIZE - buffer.size(c->outgoing);
}

pub void close(void *ctx) {
    client_t *c = ctx;
    c->close = true;
}

pub void set_timer(voidfunc_t *f, int seconds) {
    // Find a free slot.
    int slot = -1;
    for (int i = 0; i < 100; i++) {
        if (!timeouts[i].on) {
            slot = i;
            break;
        }
    }
    if (slot == -1) panic("failed to find a slot");

    timeouts[slot].on = true;
    timeouts[slot].f = f;
    timeouts[slot].t = time(NULL) + seconds;
}


size_t _tick = 0;

// Does a single poll of all streams and calls handlers when new events come.
// Returns false when there are no more streams or timeouts to process.
pub bool process() {
    dbg.m(DBG_TAG, "------------- tick %zu --------------- \n", _tick);
    _tick++;

    timeval_t timeout = { .tv_sec = 200, .tv_usec = 0 };

	fd_set rset = {0};
    fd_set wset = {0};    
    int maxfd = create_selection(&rset, &wset);

    ioloop_timer_t *closest_timer = closest_timeout();
    if (closest_timer) {
        timeout.tv_sec = closest_timer->t - time(NULL);
        dbg.m(DBG_TAG, "closest timeout in %ld s", timeout.tv_sec);
    }

    if (maxfd == 0 && !closest_timer) {
        return false;
    }

    int nfds = OS.select(maxfd + 1, &rset, &wset, NULL, &timeout);
    dbg.m(DBG_TAG, "got %d events", nfds);

    dispatch_timers();
    dispatch_updates(&rset, &wset);
    return true;
}

int create_selection(fd_set *rset, *wset) {
    int maxfd = 0;
    for (int i = 0; i < 100; i++) {
        if (!clients[i]) continue;
        client_t *c = clients[i];

        // socket, connecting -> wait for writable.
        // if (c->connecting) {
        //     dbg.m(DBG_TAG, "#%d: poll for w (connecting state)", i);
        //     OS.FD_SET(c->conn->fd, wset);
        //     if (c->conn->fd > maxfd) maxfd = c->conn->fd;
        //     continue;
        // }

        // socket, have outgoing data -> wait for writable.
        size_t nout = buffer.size(c->outgoing);
        if (nout > 0) {
            dbg.m(DBG_TAG, "#%d: poll for rw (out: %zu)", i, nout);
            OS.FD_SET(c->conn->fd, wset);
            OS.FD_SET(c->conn->fd, rset);
        } else {
            dbg.m(DBG_TAG, "#%d: poll for r", i);
            OS.FD_SET(c->conn->fd, rset);
        }
        if (c->conn->fd > maxfd) maxfd = c->conn->fd;
    }
    return maxfd;
}

void dispatch_updates(fd_set *rset, *wset) {
    char buf[4096];
    for (int i = 0; i < 100; i++) {
        client_t *c = clients[i];
        if (!c) continue;

        bool readable = OS.FD_ISSET(c->conn->fd, rset);
        bool writable = OS.FD_ISSET(c->conn->fd, wset);

        if (readable) {
            if (c->is_listener) {
                // We'll accept the connection and store it as a new client.
                // The next loop run will start processing the new client.
                net.net_t *conn2 = net.net_accept(c->conn);
                client_t *c2 = addclient(conn2, c->handler);
                callhandler(c2, CONNECTED, NULL);
            } else {
                int datalen = OS.read(c->conn->fd, buf, sizeof(buf));
                dbg.m(DBG_TAG, "#%d: read %d, %s", i, datalen, strerror(errno));
                if (datalen < 0) {
					// EAGAIN (11) means the non-blocking socket is not ready
					// yet, skip it.
					if (errno == OS.EAGAIN) {
						continue;
					}
                    panic("got r=%d, err=%d (%s)", datalen, errno, strerror(errno));
                }
                if (datalen == 0) {
                    c->close = true;
                } else {
                    buff_t b = { .data = buf, .len = datalen };
                    callhandler(c, DATA_IN, &b);
                }
            }
        }
        if (writable) {
            if (buffer.size(c->outgoing) == 0) {
                panic("selected as writable but outbuffer is empty");
            }
            // Here we're happily assuming that all bytes from buf will be
            // written out and panic in case of partial writes.
            size_t n = buffer.read(c->outgoing, buf, sizeof(buf));
            int r = OS.write(c->conn->fd, buf, n);
            if ((size_t) r != n) {
                panic("partial write");
            }
            if (buffer.size(c->outgoing) == 0) {
                callhandler(c, WRITE_FINISHED, NULL);
            }
        }

        if (c->close) {
            dbg.m(DBG_TAG, "#%d: closed or we closed", i);
            net.net_close(c->conn);
            removeclient(c);
        }
	}
}

void callhandler(client_t *c, int event, void *data) {
    if (event == DATA_IN) {
        buff_t *b = data;
        dbg.m(DBG_TAG, "#%d: event=%s, data=(%zu bytes)\n", c->slot, eventname(event), b->len);
    } else {
        dbg.m(DBG_TAG, "#%d: event=%s, data=%p\n", c->slot, eventname(event), data);
    }
    c->handler(c, event, data);
}

ioloop_timer_t *closest_timeout() {
    ioloop_timer_t *closest = NULL;
    for (int i = 0; i < 100; i++) {
        if (!timeouts[i].on) continue;
        if (!closest || timeouts[i].t < closest->t) {
            closest = &timeouts[i];
        }
    }
    return closest;
}

void dispatch_timers() {
    time_t now = time(NULL);
    for (int i = 0; i < 100; i++) {
        if (!timeouts[i].on) continue;
        if (timeouts[i].t > now) continue;
        timeouts[i].on = false;
        dbg.m(DBG_TAG, "calling timeout #%d\n", i);
        timeouts[i].f();
    }
}
