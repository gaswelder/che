#define _X_OPEN_SOURCE 700
#include <unistd.h>
#include <sys/stat.h>

#known stat
#known S_ISDIR

typedef struct stat stat_t;

/**
 * Deletes the file with the given path.
 * Returns false on error.
 */
pub bool unlink(const char *path) {
    return OS.unlink(path) == 0;
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