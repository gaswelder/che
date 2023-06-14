// Return whether line is empty, ignoring whitespace chars ' ', \r, \n
pub bool is_empty_line(const char *s) {
    int slen = strlen(s);
    for (int i=0; i < slen; i++) {
        // Not an empty line if non-whitespace char is present.
        if (s[i] != ' ' && s[i] != '\n' && s[i] != '\r') {
            return false;
        }
    }
    return true;
}
