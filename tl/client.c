#import os/exec
#import parsebuf
#import strutil

typedef {
    FILE *out;
    exec_t *child;
} client_t;

client_t tl_exec(char **args) {
    char *argv[20] = {};
    int i = 0;
    argv[i++] = "/usr/bin/ssh";
    argv[i++] = "pi";
    argv[i++] = "transmission-remote";
    argv[i++] = "-n";
    argv[i++] = "transmission:transmission";
    while (*args) {
        argv[i++] = *args;
        args++;
    }
    argv[i++] = NULL;

    char **env = {NULL};
    pipe_t pipe = exec_makepipe();
    exec_t *child = exec("/usr/bin/ssh", argv, env, stdin, pipe.write, stderr);
    fclose(pipe.write);
    client_t r = {
        .out = pipe.read,
        .child = child
    };
    return r;
}

void tl_close(client_t c) {
    int status = 0;
    if (!exec_wait(c.child, &status)) {
        fprintf(stderr, "wait failed: %s\n", strerror(errno));
    }
}

void fpassthrough(FILE *from, *to) {
    char buf[1000] = {};
    while (fgets(buf, sizeof(buf), from)) {
        fputs(buf, to);
    }
}

pub void tl_add(char *url) {
    char *args[] = {"-a", url, NULL};
    client_t c = tl_exec(args);
    fpassthrough(c.out, stdout);
    tl_close(c);
}

pub void tl_rm(char *id) {
    char *args[] = {"-r", id, NULL};
    client_t c = tl_exec(args);
    fpassthrough(c.out, stdout);
    tl_close(c);
}

pub bool tl_getval(char *id, char *param, char *out, size_t n) {
    char *args[] = {"-t", id, "-i", NULL};
    client_t c = tl_exec(args);

    char buf[1000] = {};
    bool ok = false;
    while (fgets(buf, sizeof(buf), c.out)) {
        char *line = trim(buf);
        puts(line);
        if (strstr(line, param) == line) {
            char *from = line + strlen(param) + strlen(": ");
            strncpy(out, from, n);
            ok = true;
            break;
        }
    }
    tl_close(c);
    return ok;
}

pub typedef {
     char id[8];
     char done[8];
     char have[32];
     char eta[32];
     char up[32];
     char down[32];
     char ratio[32];
     char status[32];
     char name[1000];
} torr_t;

pub void torrents(torr_t **items, size_t *size) {
    char *args[] = {"-l", NULL};
    client_t client = tl_exec(args);

    /*
     * Read and parse the output
     */
    size_t len = 0;
    size_t cap = 1;
    torr_t *list = malloc(cap * sizeof(torr_t));
    char buf[1000] = {};
    while (fgets(buf, sizeof(buf), client.out)) {
        char *line = trim(buf);
        torr_t t = parseline(line);
        if (!strcmp(t.id, "ID") || !strcmp(t.id, "Sum:")) {
            continue;
        }
        if (len + 1 > cap) {
            cap *= 2;
            list = realloc(list, cap * sizeof(torr_t));
        }
        list[len] = t;
        len++;
    }

    tl_close(client);

    *items = list;
    *size = len;
}


torr_t parseline(char *line)
{
	// ID     Done       Have  ETA           Up    Down  Ratio  Status       Name
	// 1      100%   10.48 MB  Done         0.0     0.0    2.9  Idle         John Fowles
	// 41     n/a        None  Unknown      0.0     0.0   None  Idle          Books
	// 38*    n/a        None  Unknown      0.0     0.0   None  Stopped       name

    parsebuf_t *b = buf_new(line);

    torr_t t = {};
    buf_skip_set(b, " \t");
    // ID: 1 | 38*
    word(b, t.id, sizeof(t.id));
    // "Done": 20% | n/a
    word(b, t.done, sizeof(t.done));
    // "Have": 10.48 MB | None
    size(b, t.have, sizeof(t.have));
    // "ETA": 10 m | Done | Unknown
    size(b, t.eta, sizeof(t.eta));
    // up, down, ratio: 1.2 | None
    word(b, t.up, sizeof(t.up));
    word(b, t.down, sizeof(t.down));
    word(b, t.ratio, sizeof(t.ratio));
    // "Status": Idle | Stopped | Up & Down
    if (buf_skip_literal(b, "Up & Down")) {
        strcpy(t.status, "Up & Down");
    }
    else word(b, t.status, sizeof(t.status));
    
    // "Name": the remainder
    rest(b, t.name, sizeof(t.name));
    return t;
}

void word(parsebuf_t *b, char *p, size_t n) {
    while (buf_more(b) && buf_peek(b) != ' ') {
        if (n == 0) return;
        n--;
        *p++ = buf_get(b);
    }
    spaces(b);
}

void spaces(parsebuf_t *b) {
    while (buf_more(b) && buf_peek(b) == ' ') {
        buf_get(b);
    }
}

void size(parsebuf_t *b, char *p, size_t n) {
    if (isdigit(buf_peek(b))) {
        while (buf_more(b) && buf_peek(b) != ' ') {
            if (n == 0) return;
            n--;
            *p++ = buf_get(b);
        }
        spaces(b);
        word(b, p, n);
    } else {
        word(b, p, n);
    }
}

void rest(parsebuf_t *b, char *p, size_t n) {
    while (buf_more(b)) {
        if (n == 0) return;
        n--;
        *p++ = buf_get(b);
    }
}
