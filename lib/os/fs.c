#define _X_OPEN_SOURCE 700
#include <unistd.h>
#include <sys/stat.h>

typedef struct stat stat_t;

pub bool fs_unlink(const char *path) {
    return unlink(path) == 0;
}

/*
 * Puts size of the file at 'path' and returns true.
 * Returns false on failure.
 */
pub bool filesize(const char *path, size_t *size)
{
	stat_t s = {};
	if(stat(path, &s) < 0) {
		return false;
	}
	*size = (size_t) s.st_size;
	return true;
}

/*
 * Returns true if given path exists and points to a directory.
 */
pub bool is_dir(const char *path)
{
	stat_t s = {};
	if(stat(path, &s) < 0) {
		return false;
	}
	return S_ISDIR(s.st_mode);
}