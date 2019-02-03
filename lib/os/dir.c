#include <dirent.h>
#type DIR

struct __dir {
	DIR *d;
};

typedef struct __dir dir_t;
typedef struct dirent dirent_t;

/*
 * Opens directory at given path and returns its handle.
 * Returns NULL on failure.
 */
pub dir_t *diropen(const char *path)
{
	dir_t *d = calloc(1, sizeof(*d));
	if(!d) return NULL;

	d->d = opendir(path);
	if(!d->d) {
		free(d);
		return NULL;
	}

	return d;
}

/*
 * Moves to next file in the directory and returns
 * its name.
 */
pub const char *dirnext(dir_t *d)
{
	dirent_t *e = readdir(d->d);
	if(!e) return NULL;
	return e->d_name;
}

/*
 * Closes the directory handle/
 */
pub void dirclose(dir_t *d)
{
	closedir(d->d);
	free(d);
}
