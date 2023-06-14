#import lkalloc.c
#import strings

pub typedef {
    char *s;
    size_t s_len;
    size_t s_size;
} LKString;

pub typedef {
    LKString **items;
    size_t items_len;
    size_t items_size;
} LKStringList;

// Zero out entire size
pub void zero_s(LKString *lks) {
    memset(lks->s, 0, lks->s_size+1);
}

// Zero out the free unused space
pub void zero_unused_s(LKString *lks) {
    memset(lks->s + lks->s_len, '\0', lks->s_size+1 - lks->s_len);
}

pub LKString *lk_string_new(const char *s) {
    if (s == NULL) {
        s = "";
    }
    size_t s_len = strlen(s);

    LKString *lks = lkalloc.lk_malloc(sizeof(LKString), "lk_string_new");
    lks->s_len = s_len;
    lks->s_size = s_len;
    lks->s = lkalloc.lk_malloc(lks->s_size+1, "lk_string_new_s");
    zero_s(lks);
    strncpy(lks->s, s, s_len);

    return lks;
}

pub void lk_string_free(LKString *lks) {
    assert(lks->s != NULL);

    zero_s(lks);
    lkalloc.lk_free(lks->s);
    lks->s = NULL;
    lkalloc.lk_free(lks);
}

pub void lk_string_assign(LKString *lks, const char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_size) {
        lks->s_size = s_len;
        lks->s = lkalloc.lk_realloc(lks->s, lks->s_size+1, "lk_string_assign");
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
	
	char *buf = malloc(len + 1);
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
        lks->s = lkalloc.lk_realloc(lks->s, lks->s_size+1, "lk_string_append");
        zero_unused_s(lks);
    }

    strncpy(lks->s + lks->s_len, s, s_len);
    lks->s_len = lks->s_len + s_len;
}

pub void lk_string_append_char(LKString *lks, char c) {
    if (lks->s_len + 1 > lks->s_size) {
        // Grow string by ^2
        lks->s_size = lks->s_len + ((lks->s_len+1) * 2);
        lks->s = lkalloc.lk_realloc(lks->s, lks->s_size+1, "lk_string_append_char");
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
        lks->s = lkalloc.lk_realloc(lks->s, lks->s_size+1, "lk_string_prepend");
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

// Return if string ends with s.
pub int lk_string_ends_with(LKString *lks, const char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_len) {
        return 0;
    }
    size_t i = lks->s_len - s_len;
    if (!strncmp(lks->s + i, s, s_len)) {
        return 1;
    }
    return 0;
}

// Remove leading and trailing white from string.
pub void lk_string_trim(LKString *lks) {
    if (lks->s_len == 0) {
        return;
    }

    // starti: index to first non-whitespace char
    // endi: index to last non-whitespace char
    int starti = 0;
    int endi = lks->s_len-1;
    assert(endi >= starti);

    for (size_t i = 0; i < lks->s_len; i++) {
        if (!isspace(lks->s[i])) {
            break;
        }
        starti++;
    }
    // All chars are whitespace.
    if ((size_t)starti >= lks->s_len) {
        lk_string_assign(lks, "");
        return;
    }
    for (int i = (int)lks->s_len-1; i >= 0; i--) {
        if (!isspace(lks->s[i])) {
            break;
        }
        endi--;
    }
    assert(endi >= starti);

    size_t new_len = endi-starti+1;
    memmove(lks->s, lks->s + starti, new_len);
    memset(lks->s + new_len, 0, lks->s_len - new_len);
    lks->s_len = new_len;
}

pub void lk_string_chop_end(LKString *lks, const char *s) {
    size_t s_len = strlen(s);
    if (s_len > lks->s_len) {
        return;
    }
    if (strncmp(lks->s + lks->s_len - s_len, s, s_len)) {
        return;
    }

    size_t new_len = lks->s_len - s_len;
    memset(lks->s + new_len, 0, s_len);
    lks->s_len = new_len;
}

// Return whether delim matches the first delim_len chars of s.
pub int match_delim(char *s, const char *delim, size_t delim_len) {
    return !strncmp(s, delim, delim_len);
}

pub LKStringList *lk_string_split(LKString *lks, const char *delim) {
    LKStringList *sl = lk_stringlist_new();
    size_t delim_len = strlen(delim);

    LKString *segment = lk_string_new("");
    for (size_t i = 0; i < lks->s_len; i++) {
        if (match_delim(lks->s + i, delim, delim_len)) {
            lk_stringlist_append_lkstring(sl, segment);
            segment = lk_string_new("");
            i += delim_len-1; // need to -1 to account for for loop incrementor i++
            continue;
        }

        lk_string_append_char(segment, lks->s[i]);
    }
    lk_stringlist_append_lkstring(sl, segment);

    return sl;
}

// Given a "k<delim>v" string, assign k and v.
pub void lk_string_split_assign(LKString *s, const char *delim, LKString *k, LKString *v) {
    LKStringList *ss = lk_string_split(s, delim);
    if (k != NULL) {
        lk_string_assign(k, ss->items[0]->s);
    }
    if (v != NULL) {
        if (ss->items_len >= 2) {
            lk_string_assign(v, ss->items[1]->s);
        } else {
            lk_string_assign(v, "");
        }
    }
    lk_stringlist_free(ss);
}

#define N_GROW_STRINGLIST 10

pub LKStringList *lk_stringlist_new() {
    LKStringList *sl = lkalloc.lk_malloc(sizeof(LKStringList), "lk_stringlist_new");
    sl->items_size = N_GROW_STRINGLIST;
    sl->items_len = 0;

    sl->items = lkalloc.lk_malloc(sl->items_size * sizeof(LKString), "lk_stringlist_new_items");
    memset(sl->items, 0, sl->items_size * sizeof(LKString));
    return sl;
}

pub void lk_stringlist_free(LKStringList *sl) {
    assert(sl->items != NULL);

    for (size_t i = 0; i < sl->items_len; i++) {
        lk_string_free(sl->items[i]);
    }
    memset(sl->items, 0, sl->items_size * sizeof(LKString));

    lkalloc.lk_free(sl->items);
    sl->items = NULL;
    lkalloc.lk_free(sl);
}

pub void lk_stringlist_append_lkstring(LKStringList *sl, LKString *lks) {
    assert(sl->items_len <= sl->items_size);

    if (sl->items_len == sl->items_size) {
        LKString **pitems = lkalloc.lk_realloc(sl->items, (sl->items_size+N_GROW_STRINGLIST) * sizeof(LKString), "lk_stringlist_append_lkstring");
        if (pitems == NULL) {
            return;
        }
        sl->items = pitems;
        sl->items_size += N_GROW_STRINGLIST;
    }
    sl->items[sl->items_len] = lks;
    sl->items_len++;

    assert(sl->items_len <= sl->items_size);
}

pub void lk_stringlist_append(LKStringList *sl, const char *s) {
    lk_stringlist_append_lkstring(sl, lk_string_new(s));
}

pub void lk_stringlist_append_sprintf(LKStringList *sl, const char *format, ...) {
    va_list args = {};
	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (len < 0) abort();
	char *buf = malloc(len + 1);
	if (!buf) abort();
	va_start(args, format);
	vsnprintf(buf, len + 1, format, args);
	va_end(args);

    lk_stringlist_append(sl, buf);
    free(buf);
}

pub LKString *lk_stringlist_get(LKStringList *sl, uint32_t i) {
    if (sl->items_len == 0) return NULL;
    if (i >= sl->items_len) return NULL;
    return sl->items[i];
}

pub void lk_stringlist_remove(LKStringList *sl, uint32_t i) {
    if (sl->items_len == 0) return;
    if (i >= sl->items_len) return;

    lk_string_free(sl->items[i]);

    int num_items_after = sl->items_len-i-1;
    memmove(sl->items+i, sl->items+i+1, num_items_after * sizeof(LKString));
    memset(sl->items+sl->items_len-1, 0, sizeof(LKString));
    sl->items_len--;
}
