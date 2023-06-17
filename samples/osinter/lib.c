#include <libgen.h>

pub char *basename(char *path) {
    return OS.basename(path);
}
