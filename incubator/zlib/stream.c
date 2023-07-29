/* Allowed flush values for deflate() and inflate() */
/* If when compressing you periodically use Z_FULL_FLUSH, carefully write all the pending data at those points, and
keep an index of those locations, then you can start decompression at those
points. You have to be careful to not use Z_FULL_FLUSH too often, since it
can significantly degrade compression. Alternatively, you can scan a
deflate stream once to generate an index, and then use that index for
random access. See examples/zran.c .*/
pub enum {
    Z_NO_FLUSH = 0,
    Z_PARTIAL_FLUSH = 1,
    Z_SYNC_FLUSH = 2,
    Z_FULL_FLUSH = 3,
    Z_FINISH = 4,
    Z_BLOCK = 5,
    Z_TREES = 6,
};

/* Return codes for the compression/decompression functions. Negative values
 * are errors, positive values are used for special but normal events. */
pub enum {
    Z_OK            =  0,
    Z_STREAM_END    =  1,
    Z_NEED_DICT     =  2,
    Z_ERRNO         = -1,
    Z_STREAM_ERROR  = -2,
    Z_DATA_ERROR    = -3,
    Z_MEM_ERROR     = -4,
    Z_BUF_ERROR     = -5, // Note that Z_BUF_ERROR is not fatal
    Z_VERSION_ERROR = -6,
};

/* Possible values of the data_type field for deflate() */
pub enum {
  Z_BINARY   = 0,
  Z_TEXT     = 1,
  Z_ASCII    = 1,   /* for compatibility with 1.2.2 and earlier */
  Z_UNKNOWN  = 2,
};

#define MAX_BITS 15
/* All codes must not exceed MAX_BITS bits */

/*
     gzip header information passed to and from zlib routines.  See RFC 1952
  for more details on the meanings of these fields.
*/
pub typedef {
    int     text;       /* true if compressed data believed to be text */
    uint32_t   time;       /* modification time */
    int     xflags;     /* extra flags (not used when writing a gzip file) */
    int     os;         /* operating system */
    Bytef   *extra;     /* pointer to extra field or NULL if none */
    uInt    extra_len;  /* extra field length (valid if extra != NULL) */
    uInt    extra_max;  /* space at extra (only when reading header) */
    Bytef   *name;      /* pointer to zero-terminated file name or NULL */
    uInt    name_max;   /* space at name (only when reading header) */
    Bytef   *comment;   /* pointer to zero-terminated comment or NULL */
    uInt    comm_max;   /* space at comment (only when reading header) */
    int     hcrc;       /* true if there was or will be a header crc */
    int     done;       /* true when done reading gzip header (not used
                           when writing a gzip file) */
} gz_header;

/* Data structure describing a single value and its code string. */
pub typedef {
    union {
        uint16_t  freq;       /* frequency count */
        uint16_t  code;       /* bit string */
    } fc;
    union {
        uint16_t  dad;        /* father node in Huffman tree */
        uint16_t  len;        /* length of bit string */
    } dl;
} ct_data;

pub typedef {
    ct_data *dyn_tree;           /* the dynamic tree */
    int     max_code;            /* largest code with non zero frequency */
    const static_tree_desc *stat_desc;  /* the corresponding static tree */
} tree_desc;

/* number of distance codes */
#define D_CODES   30
#define HEAP_SIZE (2*L_CODES+1)
/* maximum heap size */

#define BL_CODES  19
/* number of codes used to transfer the bit lengths */

pub typedef {
    z_stream *strm;      /* pointer back to this zlib stream */
    int   status;

    /* output still pending */
    uint8_t *pending_buf;
    uint32_t   pending_buf_size;

    uint8_t *pending_out;  /* next pending byte to output to the stream */
    uint32_t   pending;       /* nb of bytes in the pending buffer */
    int   wrap;          /* bit 0 true for zlib, bit 1 true for gzip */
    gz_header *gzhead;  /* gzip header information to write */
    uint32_t   gzindex;       /* where in extra, name, or comment */
    uint8_t  method;        /* can only be DEFLATED */
    int   last_flush;    /* value of flush param for previous deflate call */

    uint32_t  w_size;        /* LZ77 window size (32K by default) */
    uint32_t  w_bits;        /* log2(w_size)  (8..16) */
    uint32_t  w_mask;        /* w_size - 1 */

    uint8_t *window;
    /* Sliding window. Input bytes are read into the second half of the window,
     * and move to the first half later to keep a dictionary of at least wSize
     * bytes. With this organization, matches are limited to a distance of
     * wSize-MAX_MATCH bytes, but this ensures that IO is always
     * performed with a length multiple of the block size. Also, it limits
     * the window size to 64K, which is quite useful on MSDOS.
     * To do: use the user input buffer as sliding window.
     */

    uint32_t window_size;
    /* Actual size of window: 2*wSize, except when the user input buffer
     * is directly used as sliding window.
     */

    Pos *prev;
    /* Link to older string with same hash index. To limit the size of this
     * array to 64K, this link is maintained only for the last 32K strings.
     * An index in this array is thus a window index modulo 32K.
     */

    Pos *head; /* Heads of the hash chains or NIL. */

    uint32_t  ins_h;          /* hash index of string to be inserted */
    uint32_t  hash_size;      /* number of elements in hash table */
    uint32_t  hash_bits;      /* log2(hash_size) */
    uint32_t  hash_mask;      /* hash_size-1 */

    uint32_t  hash_shift;
    /* Number of bits by which ins_h must be shifted at each input
     * step. It must be such that after MIN_MATCH steps, the oldest
     * byte no longer takes part in the hash key, that is:
     *   hash_shift * MIN_MATCH >= hash_bits
     */

    long block_start;
    /* Window position at the beginning of the current output block. Gets
     * negative when the window is moved backwards.
     */

    uint32_t match_length;           /* length of best match */
    IPos prev_match;             /* previous match */
    int match_available;         /* set if previous match exists */
    uint32_t strstart;               /* start of string to insert */
    uint32_t match_start;            /* start of matching string */
    uint32_t lookahead;              /* number of valid bytes ahead in window */

    uint32_t prev_length;
    /* Length of the best match at previous step. Matches not greater than this
     * are discarded. This is used in the lazy match evaluation.
     */

    uint32_t max_chain_length;
    /* To speed up deflation, hash chains are never searched beyond this
     * length.  A higher limit improves compression ratio but degrades the
     * speed.
     */

    uint32_t max_lazy_match;
    /* Attempt to find a better match only when the current match is strictly
     * smaller than this value. This mechanism is used only for compression
     * levels >= 4.
     */

    /* Insert new strings in the hash table only if the match length is not
     * greater than this length. This saves time but degrades compression.
     * max_insert_length is used only for compression levels <= 3.
     */

    int level;    /* compression level (1..9) */
    int strategy; /* favor or force Huffman coding*/

    uint32_t good_match;
    /* Use a faster search when the previous match is longer than this */

    int nice_match; /* Stop searching when current match exceeds this */

                /* used by trees.c: */
    /* Didn't use ct_data typedef below to suppress compiler warning */
    ct_data dyn_ltree[HEAP_SIZE];   /* literal and length tree */
    ct_data dyn_dtree[2*D_CODES+1]; /* distance tree */
    ct_data bl_tree[2*BL_CODES+1];  /* Huffman tree for bit lengths */

    tree_desc l_desc;               /* desc. for literal tree */
    tree_desc d_desc;               /* desc. for distance tree */
    tree_desc bl_desc;              /* desc. for bit length tree */

    uint16_t bl_count[MAX_BITS+1];
    /* number of codes at each bit length for an optimal tree */

    int heap[2*L_CODES+1];      /* heap used to build the Huffman trees */
    int heap_len;               /* number of elements in the heap */
    int heap_max;               /* element of largest frequency */
    /* The sons of heap[n] are heap[2*n] and heap[2*n+1]. heap[0] is not used.
     * The same heap array is used to build all trees.
     */

    uint8_t depth[2*L_CODES+1];
    /* Depth of each subtree used as tie breaker for trees of equal frequency
     */

    uint8_t *sym_buf;        /* buffer for distances and literals/lengths */

    uint32_t  lit_bufsize;
    /* Size of match buffer for literals/lengths.  There are 4 reasons for
     * limiting lit_bufsize to 64K:
     *   - frequencies can be kept in 16 bit counters
     *   - if compression is not successful for the first block, all input
     *     data is still in the window so we can still emit a stored block even
     *     when input comes from standard input.  (This can also be done for
     *     all blocks if lit_bufsize is not greater than 32K.)
     *   - if compression is not successful for a file smaller than 64K, we can
     *     even emit a stored file instead of a stored block (saving 5 bytes).
     *     This is applicable only for zip (not gzip or zlib).
     *   - creating new Huffman trees less frequently may not provide fast
     *     adaptation to changes in the input data statistics. (Take for
     *     example a binary file with poorly compressible code followed by
     *     a highly compressible string table.) Smaller buffer sizes give
     *     fast adaptation but have of course the overhead of transmitting
     *     trees more frequently.
     *   - I can't count above 4
     */

    uint32_t sym_next;      /* running index in sym_buf */
    uint32_t sym_end;       /* symbol table full when sym_next reaches this */

    uint32_t opt_len;        /* bit length of current block with optimal trees */
    uint32_t static_len;     /* bit length of current block with static trees */
    uint32_t matches;       /* number of string matches in current block */
    uint32_t insert;        /* bytes at end of window left to insert */

    // #ifdef ZLIB_DEBUG
    uint32_t compressed_len; /* total bit length of compressed file mod 2^32 */
    uint32_t bits_sent;      /* bit length of compressed data sent mod 2^32 */

    uint16_t bi_buf;
    /* Output buffer. bits are inserted starting at the bottom (least
     * significant bits).
     */
    int bi_valid;
    /* Number of valid bits in bi_buf.  All bits above the last valid bit
     * are always zero.
     */

    uint32_t high_water;
    /* High water mark offset in window for initialized bytes -- bytes above
     * this are set to zero in order to avoid memory check warnings when
     * longest match routines access bytes past the input.  This is then
     * updated to the new high water mark.
     */

} deflate_state;

typedef void *alloc_func(void *, uInt, uInt);
typedef void  free_func(void *, void *);

pub typedef {

/* strm.total_in and strm_total_out counters may be limited to 4 GB.  These
    counters are provided as a convenience and are not used internally by
    inflate() or deflate() */

    const uint8_t *next_in;     /* next input byte */
    uInt     avail_in;  /* number of bytes available at next_in */
    uint32_t    total_in;  /* total number of input bytes read so far */

    uint8_t    *next_out; /* next output byte will go here */
    uInt     avail_out; /* remaining free space at next_out */
    uint32_t    total_out; /* total number of bytes output so far */

    const char *msg;  /* last error message, NULL if no error */
    deflate_state *state; /* not visible by applications */

    alloc_func zalloc;  /* used to allocate the internal state */
    free_func  zfree;   /* used to free the internal state */
    void*     opaque;  /* private data object passed to zalloc and zfree */

    int     data_type;  /* best guess about the data type: binary or text
                           for deflate, or the decoding state for inflate */
    uint32_t   adler;      /* Adler-32 or CRC-32 value of the uncompressed data */
    uint32_t   reserved;   /* reserved for future use */
} z_stream;

pub void *ZALLOC(z_stream *strm, size_t items, size_t size) {
    return (strm->zalloc)(strm->opaque, items, size);
}

pub void ZFREE(z_stream *strm, void *addr) {
    (strm->zfree)(strm->opaque, addr);
}
