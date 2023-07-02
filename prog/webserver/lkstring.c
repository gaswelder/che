#import strings

pub typedef {
    char *s;
    size_t s_len;
    size_t s_size;
} LKString;

// Zero out entire size
void zero_s(LKString *lks) {
    memset(lks->s, 0, lks->s_size+1);
}

// Zero out the free unused space
void zero_unused_s(LKString *lks) {
    memset(lks->s + lks->s_len, '\0', lks->s_size+1 - lks->s_len);
}

pub LKString *lk_string_new(const char *s) {
    if (s == NULL) {
        s = "";
    }
    size_t s_len = strlen(s);

    LKString *lks = calloc(1, sizeof(LKString));
    lks->s_len = s_len;
    lks->s_size = s_len;
    lks->s = calloc(1, lks->s_size+1);
    zero_s(lks);
    strncpy(lks->s, s, s_len);

    return lks;
}

pub void lk_string_free(LKString *lks) {
    assert(lks->s != NULL);

    zero_s(lks);
    free(lks->s);
    lks->s = NULL;
    free(lks);
}

pub void lk_string_assign(LKString *lks, const char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_size) {
        lks->s_size = s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
    }
    zero_s(lks);
    strncpy(lks->s, s, s_len);
    lks->s_len = s_len;
}

pub void lk_string_assign_sprintf(LKString *lks, const char *format, ...) {
    va_list args = {};
	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args);
	va_end(args);

	if (len < 0) {
        abort();
    }
	
	char *buf = calloc(len + 1, 1);
	if (!buf) {
        abort();
    }

	va_start(args, format);
	vsnprintf(buf, len + 1, format, args);
	va_end(args);
    lk_string_assign(lks, buf);
    free(buf);
}

pub void lk_string_append(LKString *lks, const char *s) {
    size_t s_len = strlen(s);
    if (lks->s_len + s_len > lks->s_size) {
        lks->s_size = lks->s_len + s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
        zero_unused_s(lks);
    }

    strncpy(lks->s + lks->s_len, s, s_len);
    lks->s_len = lks->s_len + s_len;
}

pub void lk_string_append_char(LKString *lks, char c) {
    if (lks->s_len + 1 > lks->s_size) {
        // Grow string by ^2
        lks->s_size = lks->s_len + ((lks->s_len+1) * 2);
        lks->s = realloc(lks->s, lks->s_size+1);
        zero_unused_s(lks);
    }

    lks->s[lks->s_len] = c;
    lks->s[lks->s_len+1] = '\0';
    lks->s_len++;
}

pub void lk_string_prepend(LKString *lks, const char *s) {
    size_t s_len = strlen(s);
    if (lks->s_len + s_len > lks->s_size) {
        lks->s_size = lks->s_len + s_len;
        lks->s = realloc(lks->s, lks->s_size+1);
        zero_unused_s(lks);
    }

    memmove(lks->s + s_len, lks->s, lks->s_len); // shift string to right
    strncpy(lks->s, s, s_len);                   // prepend s to string
    lks->s_len = lks->s_len + s_len;
}


pub int lk_string_sz_equal(LKString *lks1, const char *s2) {
    if (!strcmp(lks1->s, s2)) {
        return 1;
    }
    return 0;
}

// Return if string starts with s.
pub int lk_string_starts_with(LKString *lks, const char *s) {
    return strings.starts_with(lks->s, s);
}

// Remove leading and trailing white from string.
pub void lk_string_trim(LKString *lks) {
    char *copy = strings.newstr("%s", lks->s);
    char *r = strings.trim(copy);
    lk_string_assign(lks, r);
    free(copy);
}
