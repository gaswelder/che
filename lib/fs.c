#include <limits.h>
#include <stdlib.h>

pub bool realpath(const char *path, char *buf, size_t n) {
    char tmp[PATH_MAX] = {};
    if (!OS.realpath(path, buf)) {
        return false;
    }
    if (strlen(tmp) > n-1) {
        // buf is too small.
        return false;
    }
    strcpy(buf, tmp);
    return true;
}
