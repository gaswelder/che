#include <unistd.h>

pub bool fs_unlink(const char *path) {
    return unlink(path) == 0;
}
