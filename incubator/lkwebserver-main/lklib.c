
// Print the last error message corresponding to errno.
pub void lk_print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}

pub void lk_exit_err(char *s) {
    lk_print_err(s);
    exit(1);
}

// Return whether line is empty, ignoring whitespace chars ' ', \r, \n
pub int is_empty_line(char *s) {
    int slen = strlen(s);
    for (int i=0; i < slen; i++) {
        // Not an empty line if non-whitespace char is present.
        if (s[i] != ' ' && s[i] != '\n' && s[i] != '\r') {
            return 0;
        }
    }
    return 1;
}

// Return whether string ends with \n char.
pub int ends_with_newline(char *s) {
    int slen = strlen(s);
    if (slen == 0) {
        return 0;
    }
    if (s[slen-1] == '\n') {
        return 1;
    }
    return 0;
}


