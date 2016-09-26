import "./block"
import "zio"

typedef uint32_t md5sum[4];

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

bool md5_str(const char *s, uint32_t md[4])
{
	return md5_buf(s, strlen(s), md);
}

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
	//static const char hex[] = "0123456789ABCDEF";
	for(int i = 0; i < 4; i++) {
		uint32_t w = buf[i];
		for(int j = 0; j < 4; j++) {
			uint8_t b = (w >> (j * 8)) & 0xFF;
			printf("%02x", b);
		}
	}
}
