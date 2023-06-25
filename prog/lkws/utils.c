#import time

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

// localtime in server format: 11/Mar/2023 14:05:46
pub void get_localtime_string(char *buf, size_t size) {
    if (!time.time_format(buf, size, "%d/%b/%Y %H:%M:%S")) {
        snprintf(buf, size, "failed to get time: %s", strerror(errno));
    }
}
