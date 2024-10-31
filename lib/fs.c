// #define _X_OPEN_SOURCE 700
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#type DIR

typedef struct dirent dirent_t;
typedef struct stat stat_t;

pub typedef {
	DIR *d;
} dir_t;

/*
 * Opens directory at given path and returns its handle.
 * Returns NULL on failure.
 */
pub dir_t *dir_open(const char *path) {
	dir_t *d = calloc(1, sizeof(dir_t));
	if (!d) {
		return NULL;
	}
	d->d = OS.opendir(path);
	if (!d->d) {
		free(d);
		return NULL;
	}
	return d;
}

/*
 * Moves to next file in the directory and returns
 * its name.
 */
pub const char *dir_next(dir_t *d) {
	dirent_t *e = OS.readdir(d->d);
	if (!e) {
		return NULL;
	}
	return e->d_name;
}

/*
 * Closes the directory handle/
 */
pub void dir_close(dir_t *d) {
	OS.closedir(d->d);
	free(d);
}


pub bool realpath(const char *path, char *buf, size_t n) {
    char tmp[OS.PATH_MAX] = {};
    if (!OS.realpath(path, tmp)) {
        return false;
    }
    if (strlen(tmp) > n-1) {
        // buf is too small.
        return false;
    }
    strcpy(buf, tmp);
    return true;
}

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
pub bool filesize(const char *path, size_t *size) {
	stat_t s = {};
	if (OS.stat(path, &s) < 0) {
		return false;
	}
	*size = (size_t) s.st_size;
	return true;
}

/*
 * Returns true if given path exists and points to a directory.
 */
pub bool is_dir(const char *path) {
	stat_t s = {};
	if (OS.stat(path, &s) < 0) {
		return false;
	}
	return OS.S_ISDIR(s.st_mode);
}

/*
 * Reads the file at 'path' and returns a pointer to the file's
 * contents in memory. The contents size is put into 'size'.
 */
pub uint8_t *readfile(const char *path, size_t *size) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}

	size_t len = 0;
	if (!fsize(f, &len)) {
		fclose(f);
		return NULL;
	}

	char *data = calloc(len, 1);
	if (!data) {
		fclose(f);
		return NULL;
	}
	if (!readf(f, data, len)) {
		fclose(f);
		free( data );
		return NULL;
	}
	fclose(f);
	if (size) *size = len;
	return (uint8_t *) data;
}

/*
 * Reads file 'path' and returns its contents as string.
 */
pub char *readfile_str(const char *path) {
	size_t len = 0;
	char *data = (char *) readfile(path, &len);
	if (!data) return NULL;

	char *new = realloc(data, len+1);
	if (!new) {
		free(data);
		return NULL;
	}
	new[len] = '\0';
	return new;
}

int fsize(FILE *f, size_t *s)
{
	if(fseek(f, 0, SEEK_END) != 0) {
		return 0;
	}

	long size = ftell(f);
	if(size < 0) {
		return 0;
	}

	if(fseek(f, 0, SEEK_SET) != 0) {
		return 0;
	}

	*s = (size_t) size;
	return 1;
}

int readf( FILE *f, char *data, size_t size)
{
	int c = 0;
	while( size > 0 ) {
		c = fgetc(f);
		if( feof(f) ) {
			return 0;
		}
		*data = c;
		data++;
		size--;
	}
	return 1;
}

pub bool writefile(const char *path, const char *data, size_t n) {
	FILE *f = fopen(path, "wb");
	if (!f) {
		return false;
	}
	if (fwrite(data, 1, n, f) != n) {
		fclose(f);
		return false;
	}
	if (fclose(f)) {
		return false;
	}
	return true;
}


pub int append_file( const char *path_to, const char *path_from )
{
	FILE *from = fopen( path_from, "rb" );
	if( !from ) {
		return 0;
	}

	FILE *to = fopen( path_to, "ab" );
	if( !to ) {
		fclose( from );
		return 0;
	}

	char buf[BUFSIZ] = {};
	int ok = 1;
	while( 1 ) {
		size_t len = fread( buf, 1, sizeof(buf), from );
		if( !len ) {
			break;
		}
		if( fwrite( buf, 1, len, to ) < len ) {
			ok = 0;
			break;
		}
	}
	fclose( from );
	fclose( to );
	return ok;
}

pub bool file_exists(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		return false;
	}
	fclose(f);
	return true;
}

// Return ptr to start of file extension within filepath.
// Ex. "path/to/index.html" returns "html"
pub const char *fileext(const char *filepath) {
    int filepath_len = strlen(filepath);
    // filepath of "" returns ext of "".
    if (filepath_len == 0) {
        return filepath;
    }

    const char *p = filepath + strlen(filepath) - 1;
    while (p >= filepath) {
        if (*p == '.') {
            return p+1;
        }
        p--;
    }
    return filepath;
}

pub const char *basename(const char *path) {
	const char *last = path;
	const char *p = path;
	while (*p) {
		if (*p == '/') last = p+1;
		p++;
	}
	return last;
}

// Puts the directory part the path into buf and return true.
// Returns false if buf is too small.
pub bool dirname(const char *path, char *buf, size_t bufsize) {
    if (strlen(path) >= bufsize) {
        return false;
    }
    strcpy(buf, path);
    char *p = buf;
    char *last_slash = NULL;
    while (*p) {
        if (*p == '/') last_slash = p;
        p++;
    }
    if (last_slash) {
        *last_slash = '\0';
    } else {
        buf[0] = '\0';
    }
    return true;
}
