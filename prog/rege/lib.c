
enum {
    END,
    LITERAL,
    SPACE
}

pub typedef {
    int type;
    char val;
} token_t;

pub typedef {
    token_t pat[100];
    size_t patlen;
    size_t patpos;
    bool error;
} regex_t;

pub regex_t *init(const char *pattern) {
    regex_t *r = calloc(1, sizeof(regex_t));
    if (!r) {
        return NULL;
    }
    const char *p = pattern;
    while (*p != '\0') {
        const char *q = parsetok(p, &r->pat[r->patlen]);
        if (q == p) {
            break;
        }
        p = q;
        r->patlen++;
    }
    r->pat[r->patlen++].type = END;
    return r;
}

const char *parsetok(const char *p, token_t *t) {
    if (*p == '\\') {
        p++;
        switch (*p) {
            case 's': {
                t->type = SPACE;
                p++;
                return p;
            }
            default: {
                panic("unknown escape sequence: \\%c", *p);
            }
        }
    }
    t->type = LITERAL;
    t->val = *p;
    p++;
    return p;
    
}

pub size_t consume(regex_t *r, const char *s) {
    const char *p = s;
    while (*p != '\0') {
        if (!consumechar(r, *p)) {
            break;
        }
        p++;
    }
    return p-s;
}

bool consumechar(regex_t *r, char c) {
    token_t t = r->pat[r->patpos];

    // If we're at the end of the pattern, return false.
    if (t.type == END) {
        return false;
    }

    // If the character matches the current token, accept the character and
    // shift to the next token. shift() might not actually shift if the token
    // is a repetition (a*).
    if (match(t, c)) {
        accept(c);
        shift(r);
        return true;
    }

    // If the character didn't match the current token, try skipping the token,
    // if it's skippable, and matching with the next one. Skippable tokens are,
    // for example, optional ones and repetitions (a?, a*).
    if (skip(r)) {
        return consumechar(r, c);
    }

    // If neither match, nor skip happenned, the pattern didn't match, mark it
    // as failure.
    r->error = true;
    return false;
}

void accept(char c) {
    printf("accept '%c'\n", c);
}

void shift(regex_t *r) {
    token_t t = r->pat[r->patpos];
    switch (t.type) {
        case LITERAL, SPACE: {
            r->patpos++;
            return;
        }
        default: {
            panic("unhandled pattern type");
        }
    }
}

/**
 * Skips current token if it's skippable, and returns true.
 * Returns false otherwise.
 */
bool skip(regex_t *r) {
    token_t t = r->pat[r->patpos];
    switch (t.type) {
        case LITERAL: { return false; }
        default: { panic("unhandled pattern type"); }
    }
}

/**
 * Returns true if the character c matches the token t.
*/
bool match(token_t t, char c) {
    switch (t.type) {
        case LITERAL: { return c == t.val; }
        case SPACE: { return isspace(c); }
        default: { panic("unhandled pattern type"); }
    }
}
