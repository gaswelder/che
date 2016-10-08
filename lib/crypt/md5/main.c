import "./block"
import "zio"

/*
 * Data type for the MD5 message digest.
 */
typedef uint32_t md5sum[4];

/*
 * Computes digest for the file located at 'path' and puts
 * it in 'md'.
 */
bool md5_file(const char *path, uint32_t md[4])
{
	zio *z = zopen("file", path, "rb");
	if(!z) {
		return false;
	}
	md5(z, md);
	zclose(z);
	return true;
}

/*
 * Computes digest for the string 's' and stores it in 'md'.
 */
bool md5_str(const char *s, uint32_t md[4])
{
	return md5_buf(s, strlen(s), md);
}

/*
 * Computes digest for the data in the 'buf' and stores in 'md'.
 */
bool md5_buf(const char *buf, size_t len, uint32_t md[4])
{
	zio *z = zopen("mem", "", "");
	int n = zwrite(z, buf, len);
	zrewind(z);
	assert((size_t) n == len);
	md5(z, md);
	zclose(z);
	return true;
}

void md5_print(uint32_t buf[4])
{
	for(int i = 0; i < 4; i++) {
		uint32_t w = buf[i];
		for(int j = 0; j < 4; j++) {
			uint8_t b = (w >> (j * 8)) & 0xFF;
			printf("%02x", b);
		}
	}
}
