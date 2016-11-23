/*
 * Directory mapping module
 */

import "net"
import "os/dir"
import "fileutil"
import "zio"

/*
 * Serve the file corresponding to the given query path
 */
void map(conn_t *c, char *path)
{
	printf("map %s\n", path);
	assert(path[0] == '/');
	char *localpath = newstr(".%s", path);
	defer free(localpath);

	if(is_dir(localpath)) {
		show_dir(c, localpath);
	}
	else {
		map_file(c, localpath);
	}
}

/*
 * Sends a file to a connection
 */
void map_file(conn_t *c, char *localpath)
{
	/*
	 * Get file size.
	 */
	size_t size;
	if(!filesize(localpath, &size)) {
		error_404(c);
		return;
	}

	/*
	 * Send headers
	 */
	net_printf(c,
		"HTTP/1.1 200 OK\r\n"
		"Content-Length: %zu\r\n"
		"\r\n", size, c);

	/*
	 * Pump the file to the client
	 */
	zio *f = zopen("file", localpath, "rb");
	assert(f);
	defer zclose(f);

	size_t total = copy(f, c);
	assert(total == size);
}

/*
 * Copy data from f to c and return the number
 * of bytes copied.
 */
size_t copy(zio *f, conn_t *c)
{
	char buf[4096];
	size_t total = 0;
	size_t len;

	while(1) {
		len = zread(f, buf, 4096);
		if(len == 0) {
			break;
		}
		int r = net_write(c, buf, len);
		if((size_t) r < len) {
			fatal("net_write failed: %d (%s)", errno, strerror(errno));
		}
		total += len;
	}
	return total;
}

void show_dir(conn_t *c, char *path)
{
	/*
	 * To avoid dealing with chunked encoding,
	 * we will write the page in memory and then
	 * serve it with already known content-length.
	 */
	zio *buf = zopen("mem", "", "");
	defer zclose(buf);

	/*
	 * Write the index page to the buffer
	 */
	dir_t *d = diropen(path);
	assert(d);
	write_index(buf, d, path);
	dirclose(d);

	/*
	 * Write the page out
	 */
	size_t size = ztell(buf);
	zrewind(buf);
	net_printf(c,
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Content-Length: %zu\r\n"
		"\r\n", size);
	size_t total = copy(buf, c);
	assert(total == size);
}

void write_index(zio *c, dir_t *d, const char *path)
{
	zputs(
		"<!DOCTYPE html>"
		"<html><head>"
		"<style>.dir {font-weight: bold;}</style>"
		"</head><body>", c);

	zputs("<ul>", c);

	while(1) {
		const char *fn = dirnext(d);
		if(!fn) break;

		if(strcmp(fn, ".") == 0 || strcmp(fn, "..") == 0) {
			continue;
		}

		const char *fmt;

		char *fpath = newstr("%s/%s", path, fn);
		if(is_dir(fpath)) {
			fmt = "<li class=\"dir\"><a href=\"%s\">%s</a></li>";
		}
		else {
			fmt = "<li><a href=\"%s\">%s</a></li>";
		}
		free(fpath);
		zprintf(c, fmt, fn, fn);
	}

	zputs("</ul>", c);
	zputs("</body></html>", c);
}
