#import dbg
#import os/io

#define MAX_ROUTINES 1000

const char *DEBTAG = "ioro";

pub typedef int func_t(void *, int);

pub typedef {
    int id;
    io.handle_t *waithandle, *readyhandle;
    int waitfilter, readyfilter;

    int waitroutine; // id of a routine this routine is waiting for.
    func_t *f;
    void *ctx;
    int current_line;
} routine_t;

io.pool_t *p = NULL;
routine_t *routines[MAX_ROUTINES] = {};
int CURRENT_ROUTINE = -1;

pub void init() {
    p = io.newpool();
    if (!p) {
        panic("failed to create a pool");
    }
}

pub int spawn(func_t f, void *ctx) {
    routine_t *r = calloc(1, sizeof(routine_t));
    if (!r) {
        panic("calloc failed");
    }
    r->f = f;
    r->ctx = ctx;
    r->waitroutine = -1;
    for (int i = 0; i < MAX_ROUTINES; i++) {
        if (!routines[i]) {
            routines[i] = r;
            r->id = i;
            dbg.m(DEBTAG, "spawned routine %d", i);
            return i;
        }
    }
    panic("failed to find a free slot");
}

bool runnable(routine_t *r) {
    if (r->current_line != -1 && !r->waithandle && r->waitroutine == -1) {
        return true;
    }
    return false;
}

/**
 * Runs a single loop of routines.
 * Returns false is there are no routines to run.
 */
pub int step() {
    // Run non-blocked routines.
    while (true) {
        int ran = 0;
        for (int i = 0; i < MAX_ROUTINES; i++) {
            routine_t *r = routines[i];
            if (!r) {
                continue;
            }
            while (runnable(r)) {
                ran++;
                step_routine(r);
            }
        }
        if (ran == 0) {
            break;
        }
    }

    // By now all routines must be blocked, collect and poll their waithandles.
    io.resetpool(p);
    int handles = 0;
    for (int i = 0; i < MAX_ROUTINES; i++) {
        routine_t *r = routines[i];
        if (!r) {
            continue;
        }
        if (r->waithandle) {
            if (r->waitfilter == io.READ) {
                dbg.m(DEBTAG, "%d blocked reading from %d", r->id, r->waithandle->fd);
            } else if (r->waitfilter == io.WRITE) {
                dbg.m(DEBTAG, "%d blocked writing to %d", r->id, r->waithandle->fd);
            } else {
                dbg.m(DEBTAG, "%d waits for %d (%d)", r->id, r->waithandle->fd, r->waitfilter);
            }
            io.add(p, r->waithandle, r->waitfilter);
            handles++;
        }
    }
    if (handles == 0) {
        dbg.m(DEBTAG, "no handles to poll, returning false");
        return false;
    }
    dbg.m(DEBTAG, "polling %d handles", handles);
    io.event_t *ev = io.poll(p);

    // Unblock relevant routines.
    while (ev->handle) {
        dbg.m(DEBTAG, "poll result: %d, r=%d, w=%d", ev->handle->fd, ev->readable, ev->writable);
        // Find the routine blocked in this IO event.
        routine_t *r = find_routine(ev);
        if (!r) {
            panic("failed to find the routine");
        }
        int filter = 0;
        if (ev->readable) {
            filter |= io.READ;
        }
        if (ev->writable) {
            filter |= io.WRITE;
        }
        r->readyhandle = ev->handle;
        r->readyfilter = filter;
        r->waithandle = NULL;
        while (runnable(r)) {
            step_routine(r);
        }
        ev++;
    }
    return true;
}

routine_t *find_routine(io.event_t *ev) {
    int filter = 0;
    if (ev->readable) {
        filter |= io.READ;
    }
    if (ev->writable) {
        filter |= io.WRITE;
    }
    for (int i = 0; i < MAX_ROUTINES; i++) {
        routine_t *r = routines[i];
        if (!r) {
            continue;
        }
        if (ev->handle == r->waithandle && filter == r->waitfilter) {
            return r;
        }
    }
    return NULL;
}

void step_routine(routine_t *r) {
    CURRENT_ROUTINE = r->id;
    int nextline = r->f(r->ctx, r->current_line);
    if (nextline != r->current_line) {
        r->current_line = nextline;
    }
    if (nextline == -1) {
        dbg.m(DEBTAG, "routine %d finished", CURRENT_ROUTINE);
        for (int i = 0; i < MAX_ROUTINES; i++) {
            routine_t *r = routines[i];
            if (!r) {
                continue;
            }
            if (r->waitroutine == CURRENT_ROUTINE) {
                dbg.m(DEBTAG, "unblocking routine %d", r->id);
                r->waitroutine = -1;
            }
        }
        return;
    }
    // if (r->waitroutine != -1) {
    //     printf("routine %d is waiting for routine %d\n", r->id, r->waitroutine);
    // }
    // if (r->waithandle) {
    //     if (r->waitfilter == io.READ) {
    //         printf("routine %d is waiting to read from %d\n", r->id, r->waithandle->fd);
    //     }
    //     else if (r->waitfilter == io.WRITE) {
    //         printf("routine %d is waiting to write to %d\n", r->id, r->waithandle->fd);
    //     }
    //     else {
    //         printf("routine is waiting for fd %d (%d)\n", r->waithandle->fd, r->waitfilter);
    //     }
    // }
}

pub bool ioready(io.handle_t *h, int filter) {
    routine_t *r = routines[CURRENT_ROUTINE];
    if (r->readyhandle == h && r->readyfilter & filter) {
        r->readyhandle = NULL;
        return true;
    }
    r->waithandle = h;
    r->waitfilter = filter;
    return false;
}

pub bool done(int id) {
    routine_t *r = routines[CURRENT_ROUTINE];
    if (!r) {
        panic("no such routine: %d", id);
    }
    int line = routines[id]->current_line;
    if (line != -1) {
        r->waitroutine = id;
        return false;
    }
    free(routines[id]);
    routines[id] = NULL;
    r->waitroutine = -1;
    return true;
}
