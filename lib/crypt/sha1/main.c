import "zio"
import "./sha1"

typedef uint32_t sha1sum[5];

/*
 * Computes digest for the file located at 'path' and puts
 * it in 'md'.
 */
pub bool sha1_file(const char *path, sha1sum h)
{
	zio *z = zopen("file", path, "rb");
	if(!z) {
		return false;
	}
	sha1(z, h);
	zclose(z);
	return true;
}

/*
 * Computes digest for the string 's' and stores it in 'md'.
 */
pub bool sha1_str(const char *s, sha1sum h)
{
	return sha1_buf(s, strlen(s), h);
}

/*
 * Computes digest for the data in the 'buf' and stores in 'md'.
 */
pub bool sha1_buf(const char *buf, size_t len, sha1sum h)
{
	zio *z = zopen("mem", "", "");
	int n = zwrite(z, buf, len);
	zrewind(z);
	assert((size_t) n == len);
	sha1(z, h);
	zclose(z);
	return true;
}

pub void sha1_print(sha1sum h)
{
	printf("%08x %08x %08x %08x %08x", h[0], h[1], h[2], h[3], h[4]);
}
