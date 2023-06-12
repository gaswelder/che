

// Predefined buffer sizes.
// Sample use: char buf[LK_BUFSIZE_SMALL]
#define LK_BUFSIZE_SMALL 512
#define LK_BUFSIZE_MEDIUM 1024
#define LK_BUFSIZE_LARGE 2048
#define LK_BUFSIZE_XL 4096
#define LK_BUFSIZE_XXL 8192

#ifdef DEBUGALLOC
#define malloc(size) (lk_malloc((size), "malloc"))
#define realloc(p, size) (lk_realloc((p), (size), "realloc"))
#define free(p) (lk_free((p))) 
#define strdup(s) (lk_strdup((s), "strdup"))
#define strndup(s, n) (lk_strndup((s), (n), "strndup"))
#endif

// Return localtime in server format: 11/Mar/2023 14:05:46
// Usage:
// char timestr[TIME_STRING_SIZE];
// get_localtime_string(timestr, sizeof(timestr))
#define TIME_STRING_SIZE 25

