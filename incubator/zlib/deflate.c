/* deflate.c -- compress data using the deflation algorithm
 * Copyright (C) 1995-2022 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#import adler32
#import assert.c
#import crc32
#import trace.c

 /*
Maximum value for memLevel in deflateInit2
The memory requirements for deflate are (in bytes):
            (1 << (windowBits+2)) +  (1 << (memLevel+9))
 that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
 plus a few kilobytes for small objects. For example, if you want to reduce
 the default memory requirements from 256K to 128K, compile with
     make CFLAGS="-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7"
 Of course this will generally degrade compression (there's no free lunch).
*/
#define MAX_MEM_LEVEL 9

/* number of codes used to transfer the bit lengths */
#define BL_CODES  19

/* Maximum value for windowBits */
#define MAX_WBITS   15 /* 32K LZ77 window */

#define ZLIB_VERSION "foo.bar"

#define LITERALS  256 /* number of literal bytes 0..255 */
#define LENGTH_CODES 29 /* number of length codes, not counting the special END_BLOCK code */
#define L_CODES (LITERALS+1+LENGTH_CODES)
/* number of Literal or Length codes, including the END_BLOCK code */

/* Max Huffman code length */
#define MAX_BITS 15

// In Windows16, OS_CODE is 0, as in MSDOS [Truta]
// In Cygwin, OS_CODE is 3 (Unix), not 11 (Windows32) [Wilson]
#define OS_CODE  3     /* assume Unix */

/* number of distance codes */
#define D_CODES   30

/* maximum heap size */
#define HEAP_SIZE (2*L_CODES+1)

#define PRESET_DICT 0x20 /* preset dictionary flag in zlib header */

/* The minimum and maximum match lengths */
#define MIN_MATCH  3
#define MAX_MATCH  258

/* Maximum stored block length in deflate format (not including header). */
#define MAX_STORED 65535

#define Buf_size 16
/* size of bit buffer in bi_buf */

#define INIT_STATE    42    /* zlib header -> BUSY_STATE */
#define GZIP_STATE  57    /* gzip header -> BUSY_STATE | EXTRA_STATE */
#define EXTRA_STATE   69    /* gzip extra block -> NAME_STATE */
#define NAME_STATE    73    /* gzip file name -> COMMENT_STATE */
#define COMMENT_STATE 91    /* gzip comment -> HCRC_STATE */
#define HCRC_STATE   103    /* gzip header CRC -> BUSY_STATE */
#define BUSY_STATE   113    /* deflate -> FINISH_STATE */
#define FINISH_STATE 666    /* stream complete */
/* Stream status */

#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
/* Minimum amount of lookahead, except at the end of the input file.
 * See deflate.c for comments about the MIN_MATCH+1.
 */

#define WIN_INIT MAX_MATCH
/* Number of bytes after end of data in window to initialize in order to avoid
   memory checker errors from longest match routines */


/*
Before calling deflate or inflate, make sure that avail_in and avail_out are not zero.
    When setting the parameter flush equal to Z_FINISH, also make sure that
    avail_out is big enough to allow processing all pending input.  Note that a
    Z_BUF_ERROR is not fatal--another call to deflate() or inflate() can be
    made with more input or output space.  A Z_BUF_ERROR may in fact be
    unavoidable depending on how the functions are used, since it is not
    possible to tell whether or not there is more output pending when
    strm.avail_out returns with zero.  See http://zlib.net/zlib_how.html for a
    heavily annotated example.



The "deflation" depends on identifying portions of the input text which are
identical to earlier input (within a sliding window).

The most straightforward technique turns out to be the fastest for most input
files: try all possible matches and select the longest.

A previous version used a more sophisticated algorithm by Fiala and Greene
which is guaranteed to run in linear amortized time, but has a larger average
cost, uses more memory and is patented. F&G algorithm may be faster for some
highly redundant files if the parameter max_chain_length (described below) is too large.



The key feature of this algorithm is that insertions into the string dictionary
are very simple and fast, and deletions are avoided completely.
Insertions are performed at each input character, whereas string matches are
performed only when the previous match ends.
So it is preferable to spend more time in matches to allow very fast string
 insertions and avoid deletions. The matching algorithm for small
 strings is inspired from that of Rabin & Karp. A brute force approach
 is used to find longer strings when a small match has been found.
 A similar algorithm is used in comic (by Jan-Mark Wams) and freeze
 (by Leonid Broukhis).
  */

const char my_version[] = ZLIB_VERSION;

/* The three kinds of block type */
enum {
    STORED_BLOCK = 0,
    STATIC_TREES = 1,
    DYN_TREES    = 2,
};

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



/*
     gzip header information passed to and from zlib routines.  See RFC 1952
  for more details on the meanings of these fields.
*/
pub typedef {
    int     text;       /* true if compressed data believed to be text */
    uint32_t   time;       /* modification time */
    int     xflags;     /* extra flags (not used when writing a gzip file) */
    int     os;         /* operating system */
    uint8_t   *extra;     /* pointer to extra field or NULL if none */
    uint32_t    extra_len;  /* extra field length (valid if extra != NULL) */
    uint32_t    extra_max;  /* space at extra (only when reading header) */
    uint8_t   *name;      /* pointer to zero-terminated file name or NULL */
    uint32_t    name_max;   /* space at name (only when reading header) */
    uint8_t   *comment;   /* pointer to zero-terminated comment or NULL */
    uint32_t    comm_max;   /* space at comment (only when reading header) */
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
    const ct_data *static_tree;  /* static tree or NULL */
    int *extra_bits;      /* extra bits for each code or NULL */
    int     extra_base;          /* base index for extra_bits */
    int     elems;               /* max number of elements in the tree */
    int     max_length;          /* max bit length for the codes */
} static_tree_desc;

pub typedef {
    ct_data *dyn_tree;           /* the dynamic tree */
    int     max_code;            /* largest code with non zero frequency */
    const static_tree_desc *stat_desc;  /* the corresponding static tree */
} tree_desc;



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

    uint16_t *prev;
    /* Link to older string with same hash index. To limit the size of this
     * array to 64K, this link is maintained only for the last 32K strings.
     * An index in this array is thus a window index modulo 32K.
     */

    uint16_t *head; /* Heads of the hash chains or NIL. */

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
    uint32_t prev_match;             /* previous match */
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

pub typedef void *alloc_func(void *, uint32_t, uint32_t);
pub typedef void  free_func(void *, void *);

pub typedef {
    /* total_in and total_out are provided as a convenience and are not used internally by
    inflate() or deflate() */
    size_t total_in;  /* total number of input bytes read so far */
    size_t total_out; /* total number of bytes output so far */

    const uint8_t *next_in; /* next input byte */
    size_t avail_in; /* number of bytes available at next_in */

    uint8_t *next_out; /* next output byte will go here */
    size_t avail_out; /* remaining free space at next_out */

    const char *msg;  /* last error message, NULL if no error */
    deflate_state *state; /* not visible by applications */

    int data_type;  /* best guess about the data type: binary or text for deflate, or the decoding state for inflate */
    uint32_t   adler;      /* Adler-32 or CRC-32 value of the uncompressed data */
    uint32_t   reserved;   /* reserved for future use */
} z_stream;





/* compression levels */
pub enum {
  Z_NO_COMPRESSION        = 0,
  Z_BEST_SPEED            = 1,
  Z_BEST_COMPRESSION      = 9,
  Z_DEFAULT_COMPRESSION   = -1,
};

pub enum {
    /* The deflate compression method (the only one supported in this version) */
    Z_DEFLATED = 8
};

/* compression strategy */
pub enum {
  Z_FILTERED            = 1,
  Z_HUFFMAN_ONLY        = 2,
  Z_RLE                 = 3,
  Z_FIXED               = 4,
  Z_DEFAULT_STRATEGY    = 0,
};





/* Output a byte on the stream.
 * IN assertion: there is enough room in pending_buf.
 */
void put_byte(deflate_state *s, uint8_t c) {
    s->pending_buf[s->pending++] = c;
}





/* In order to simplify the code, particularly on 16 bit machines, match
 * distances are limited to MAX_DIST instead of WSIZE.
 */
int MAX_DIST(deflate_state *s)  {
    return s->w_size - MIN_LOOKAHEAD;
}





void _tr_tally_lit(deflate_state *s, uint8_t c, bool *flush) {
    *flush = _tr_tally(s, 0, c);
}
void _tr_tally_dist(deflate_state *s, uint16_t distance, uint8_t length, bool *flush) {
    *flush = _tr_tally(s, distance, length);
}

enum {
    need_more,      /* block not completed, need more input or more output */
    block_done,     /* block flush performed */
    finish_started, /* finish started, need only more output at next deflate */
    finish_done     /* finish done, accept no more input or output */
};

/* Compression function. Returns the block state after the call. */
typedef int compress_func(deflate_state *, int);

enum {
    NIL = 0, /* Tail of hash chains */
    TOO_FAR = 4096, /* Matches of length 3 are discarded if their distance exceeds TOO_FAR */
};

/* Values for max_lazy_match, good_match and max_chain_length, depending on
 * the desired pack level (0..9). The values given below have been tuned to
 * exclude worst case performance for pathological files. Better values may be
 * found for specific files.
 */
typedef {
   uint16_t good_length; /* reduce lazy search above this match length */
   uint16_t max_lazy;    /* do not perform lazy search above this match length */
   uint16_t nice_length; /* quit search above this match length */
   uint16_t max_chain;
   compress_func *func;
} config;

const config configuration_table[10] = {
/*      good lazy nice chain */
/* 0 */ {0,    0,  0,    0, deflate_stored},  /* store only */
/* 1 */ {4,    4,  8,    4, deflate_fast}, /* max speed, no lazy matches */
/* 2 */ {4,    5, 16,    8, deflate_fast},
/* 3 */ {4,    6, 32,   32, deflate_fast},

/* 4 */ {4,    4, 16,   16, deflate_slow},  /* lazy matches */
/* 5 */ {8,   16, 32,   32, deflate_slow},
/* 6 */ {8,   16, 128, 128, deflate_slow},
/* 7 */ {8,   32, 128, 256, deflate_slow},
/* 8 */ {32, 128, 258, 1024, deflate_slow},
/* 9 */ {32, 258, 258, 4096, deflate_slow}}; /* max compression */

/* Note: the deflate() code requires max_lazy >= MIN_MATCH and max_chain >= 4
 * For deflate_fast() (levels <= 3) good is ignored and lazy has a different
 * meaning.
 */

/* rank Z_BLOCK between Z_NO_FLUSH and Z_PARTIAL_FLUSH */
int RANK(int f) {
    if (f > 4) {
        return f * 2 - 9;
    } else {
        return f * 2;
    }
}

/* ===========================================================================
 * Update a hash value with the given input byte
 * IN  assertion: all calls to UPDATE_HASH are made with consecutive input
 *    characters, so that a running hash key can be computed from the previous
 *    key instead of complete recalculation each time.
 */
void UPDATE_HASH(deflate_state *s, uint32_t c) {
    s->ins_h = ((s->ins_h << s->hash_shift) ^ (c)) & s->hash_mask;
}


void zmemzero(void *dest, size_t len) {
    memset(dest, 0, len);
}


/* ===========================================================================
 * Initialize the hash table (avoiding 64K overflow for 16 bit systems).
 * prev[] will be initialized on the fly.
 */
void CLEAR_HASH(deflate_state *s) {
    s->head[s->hash_size - 1] = NIL;
    zmemzero((uint8_t *)s->head, (unsigned)(s->hash_size - 1)*sizeof(*s->head));
}

/* ===========================================================================
 * Slide the hash table when sliding the window down (could be avoided with 32
 * bit values at the expense of memory usage). We slide even when level == 0 to
 * keep the hash table consistent if we switch back to level > 0 later.
 */
void slide_hash(deflate_state *s) {
    unsigned n;
    unsigned m;
    uint16_t *p;
    uint32_t wsize = s->w_size;

    n = s->hash_size;
    p = &s->head[n];
    while (true) {
        m = *--p;
        if (m >= wsize) {
            *p = (uint16_t)(m - wsize);
        } else {
            *p = NIL;
        }
        if (!--n) {
            break;
        }
    }
    n = wsize;
    p = &s->prev[n];
    while (true) {
        m = *--p;
        if (m >= wsize) {
            *p = (uint16_t)(m - wsize);
        } else {
            *p = (uint16_t)(NIL);
        }
        /* If n is not on any hash chain, prev[n] is garbage but
         * its value will never be used.
         */
        if (!--n) {
            break;
        }
    }
}

/*
 * Initializes the internal stream state for compression.
 
The compression level must be Z_DEFAULT_COMPRESSION, or between 0 and 9:
   1 gives best speed, 9 gives best compression, 0 gives no compression at all
   (the input data is simply copied a block at a time).  Z_DEFAULT_COMPRESSION
   requests a default compromise between speed and compression (currently
   equivalent to level 6).

     deflateInit returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_STREAM_ERROR if level is not a valid compression level, or
   Z_VERSION_ERROR if the zlib library version (zlib_version) is incompatible
   with the version assumed by the caller (ZLIB_VERSION).  msg is set to null
   if there is no error message.  deflateInit does not perform any compression:
   this will be done by deflate().
*/



//     windowBits can also be -8..-15 for raw deflate.  In this case, -windowBits
//    determines the window size.  deflate() will then generate raw deflate data
//    with no zlib header or trailer, and will not compute a check value.

//      windowBits can also be greater than 15 for optional gzip encoding.  Add
//    16 to windowBits to write a simple gzip header and trailer around the
//    compressed data instead of a zlib wrapper.  The gzip header will have no
//    file name, no extra data, no comment, no modification time (set to zero), no
//    header crc, and the operating system will be set to the appropriate value,
//    if the operating system was determined at compile time.  If a gzip stream is
//    being written, strm->adler is a CRC-32 instead of an Adler-32.

//      For raw deflate or gzip encoding, a request for a 256-byte window is
//    rejected as invalid, since only the zlib header provides a means of
//    transmitting the window size to the decompressor.

//      The memLevel parameter specifies how much memory should be allocated
//    for the internal compression state.  memLevel=1 uses minimum memory but is
//    slow and reduces compression ratio; memLevel=9 uses maximum memory for
//    optimal speed.  The default value is 8.  See zconf.h for total memory usage
//    as a function of windowBits and memLevel.

// The strategy (filter) parameter:
// Z_DEFAULT_STRATEGY for normal data
// Z_FILTERED for data produced by a filter (or predictor)
// Z_HUFFMAN_ONLY to force Huffman encoding only (no string match)
// Z_RLE to limit match distances to one (run-length encoding).
// Filtered data consists mostly of small values with a somewhat random distribution.
// In this case, the compression algorithm is tuned to compress them better.
// The effect of Z_FILTERED is to force more Huffman coding and less string matching.
// It is somewhat intermediate between Z_DEFAULT_STRATEGY and Z_HUFFMAN_ONLY.

// Z_RLE is designed to be almost as fast as Z_HUFFMAN_ONLY, but give better compression for PNG image data.

// The strategy parameter only affects the compression ratio but not the correctness of the compressed output even if it is not set appropriately.

// Z_FIXED prevents the use of dynamic Huffman codes, allowing for a simpler decoder for special applications.
pub typedef {
    int level;
    int method; // It must be Z_DEFLATED in this version of the library.
    int windowBits;

    // base two logarithm of the window size
    // (the size of the history buffer).  It should be in the range 8..15
    // Larger values result in better compression at the expense of memory usage.
    // The default value is 15 if deflateInit is used instead.
    //
    // A windowBits value of 8 (a window size of 256 bytes) is not supported.
    // As a result, a request for 8 will result in 9 (a 512-byte window).  In that case, providing 8 to
    // inflateInit2() will result in an error when the zlib header with 9 is
    // checked against the initialization of inflate().  The remedy is to not use 8
    // with deflateInit2() with this initialization, or at least in that case use 9
    // with inflateInit2().
    int memLevel;

    int strategy;
} deflate_config_t;

pub int deflateInit(z_stream *strm, int level) {
    deflate_config_t cfg = {
        .level = level,
        .method = Z_DEFLATED,
        .windowBits = MAX_WBITS,
        .memLevel = 8,
        .strategy = Z_DEFAULT_STRATEGY
    };
  return deflateInit2(strm, cfg);
    /* To do: ignore strm->next_in if we use it as window */
}


// Returns Z_OK or an error code.
pub int deflateInit2(z_stream *strm, deflate_config_t cfg) {
    if (strm == NULL) {
        return Z_STREAM_ERROR;
    }
    strm->msg = NULL;

    int level = cfg.level;
    if (level == Z_DEFAULT_COMPRESSION) {
        level = 6;
    }

    int method = cfg.method;
    int windowBits = cfg.windowBits;
    int memLevel = cfg.memLevel;
    int strategy = cfg.strategy;

    deflate_state *s;
    int wrap = 1;

    if (windowBits < 0) { /* suppress zlib wrapper */
        wrap = 0;
        if (windowBits < -15)
            return Z_STREAM_ERROR;
        windowBits = -windowBits;
    }
    else if (windowBits > 15) {
        wrap = 2;       /* write gzip wrapper instead */
        windowBits -= 16;
    }
    if (memLevel < 1 || memLevel > MAX_MEM_LEVEL || method != Z_DEFLATED ||
        windowBits < 8 || windowBits > 15 || level < 0 || level > 9 ||
        strategy < 0 || strategy > Z_FIXED || (windowBits == 8 && wrap != 1)) {
        return Z_STREAM_ERROR;
    }
    if (windowBits == 8) windowBits = 9;  /* until 256-byte window bug fixed */
    s = calloc(1, sizeof(deflate_state));
    if (s == NULL) return Z_MEM_ERROR;
    strm->state = s;
    s->strm = strm;
    s->status = INIT_STATE;     /* to pass state test in deflateReset() */

    s->wrap = wrap;
    s->gzhead = NULL;
    s->w_bits = (uint32_t)windowBits;
    s->w_size = 1 << s->w_bits;
    s->w_mask = s->w_size - 1;

    s->hash_bits = (uint32_t)memLevel + 7;
    s->hash_size = 1 << s->hash_bits;
    s->hash_mask = s->hash_size - 1;
    s->hash_shift =  ((s->hash_bits + MIN_MATCH-1) / MIN_MATCH);

    s->window = calloc(s->w_size, sizeof(uint16_t));
    s->prev   = calloc(s->w_size, sizeof(uint16_t));
    s->head   = calloc(s->hash_size, sizeof(uint16_t));

    s->high_water = 0;      /* nothing written to s->window yet */

    s->lit_bufsize = 1 << (memLevel + 6); /* 16K elements by default */

    /* We overlay pending_buf and sym_buf. This works since the average size
     * for length/distance pairs over any compressed block is assured to be 31
     * bits or less.
     *
     * Analysis: The longest fixed codes are a length code of 8 bits plus 5
     * extra bits, for lengths 131 to 257. The longest fixed distance codes are
     * 5 bits plus 13 extra bits, for distances 16385 to 32768. The longest
     * possible fixed-codes length/distance pair is then 31 bits total.
     *
     * sym_buf starts one-fourth of the way into pending_buf. So there are
     * three bytes in sym_buf for every four bytes in pending_buf. Each symbol
     * in sym_buf is three bytes -- two for the distance and one for the
     * literal/length. As each symbol is consumed, the pointer to the next
     * sym_buf value to read moves forward three bytes. From that symbol, up to
     * 31 bits are written to pending_buf. The closest the written pending_buf
     * bits gets to the next sym_buf symbol to read is just before the last
     * code is written. At that time, 31*(n - 2) bits have been written, just
     * after 24*(n - 2) bits have been consumed from sym_buf. sym_buf starts at
     * 8*n bits into pending_buf. (Note that the symbol buffer fills when n - 1
     * symbols are written.) The closest the writing gets to what is unread is
     * then n + 14 bits. Here n is lit_bufsize, which is 16384 by default, and
     * can range from 128 to 32768.
     *
     * Therefore, at a minimum, there are 142 bits of space between what is
     * written and what is read in the overlain buffers, so the symbols cannot
     * be overwritten by the compressed data. That space is actually 139 bits,
     * due to the three-bit fixed-code block header.
     *
     * That covers the case where either Z_FIXED is specified, forcing fixed
     * codes, or when the use of fixed codes is chosen, because that choice
     * results in a smaller compressed block than dynamic codes. That latter
     * condition then assures that the above analysis also covers all dynamic
     * blocks. A dynamic-code block will only be chosen to be emitted if it has
     * fewer bits than a fixed-code block would for the same set of symbols.
     * Therefore its average symbol length is assured to be less than 31. So
     * the compressed data for a dynamic block also cannot overwrite the
     * symbols from which it is being constructed.
     */

    s->pending_buf = calloc(s->lit_bufsize, 4);
    s->pending_buf_size = (uint32_t)s->lit_bufsize * 4;

    if (s->window == NULL || s->prev == NULL || s->head == NULL ||
        s->pending_buf == NULL) {
        s->status = FINISH_STATE;
        strm->msg = ERR_MSG(Z_MEM_ERROR);
        deflateEnd (strm);
        return Z_MEM_ERROR;
    }
    s->sym_buf = s->pending_buf + s->lit_bufsize;
    s->sym_end = (s->lit_bufsize - 1) * 3;
    /* We avoid equality with lit_bufsize*3 because of wraparound at 64K
     * on 16 bit machines and because stored blocks are restricted to
     * 64K-1 bytes.
     */

    s->level = level;
    s->strategy = strategy;
    s->method = (uint8_t)method;

    return deflateReset(strm);
}

/*
 * Check for a valid deflate stream state. Return 0 if ok, 1 if not.
 */
int deflateStateCheck(z_stream *strm) {
    if (strm == NULL) {
        return 1;
    }
    deflate_state *s = strm->state;
    if (s == NULL || s->strm != strm || (s->status != INIT_STATE &&
                                           s->status != GZIP_STATE &&
                                           s->status != EXTRA_STATE &&
                                           s->status != NAME_STATE &&
                                           s->status != COMMENT_STATE &&
                                           s->status != HCRC_STATE &&
                                           s->status != BUSY_STATE &&
                                           s->status != FINISH_STATE))
        return 1;
    return 0;
}

/*
     Initializes the compression dictionary from the given byte sequence
   without producing any compressed output.  When using the zlib format, this
   function must be called immediately after deflateInit, deflateInit2 or
   deflateReset, and before any call of deflate.  When doing raw deflate, this
   function must be called either before any call of deflate, or immediately
   after the completion of a deflate block, i.e. after all input has been
   consumed and all output has been delivered when using any of the flush
   options Z_BLOCK, Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, or Z_FULL_FLUSH.  The
   compressor and decompressor must use exactly the same dictionary (see
   inflateSetDictionary).

     The dictionary should consist of strings (byte sequences) that are likely
   to be encountered later in the data to be compressed, with the most commonly
   used strings preferably put towards the end of the dictionary.  Using a
   dictionary is most useful when the data to be compressed is short and can be
   predicted with good accuracy; the data can then be compressed better than
   with the default empty dictionary.

     Depending on the size of the compression data structures selected by
   deflateInit or deflateInit2, a part of the dictionary may in effect be
   discarded, for example if the dictionary is larger than the window size
   provided in deflateInit or deflateInit2.  Thus the strings most likely to be
   useful should be put at the end of the dictionary, not at the front.  In
   addition, the current implementation of deflate will use at most the window
   size minus 262 bytes of the provided dictionary.

     Upon return of this function, strm->adler is set to the Adler-32 value
   of the dictionary; the decompressor may later use this value to determine
   which dictionary has been used by the compressor.  (The Adler-32 value
   applies to the whole dictionary even if only a subset of the dictionary is
   actually used by the compressor.) If a raw deflate was requested, then the
   Adler-32 value is not computed and strm->adler is not set.

     deflateSetDictionary returns Z_OK if success, or Z_STREAM_ERROR if a
   parameter is invalid (e.g.  dictionary being NULL) or the stream state is
   inconsistent (for example if deflate has already been called for this stream
   or if not at a block boundary for raw deflate).  deflateSetDictionary does
   not perform any compression: this will be done by deflate().
*/
pub int deflateSetDictionary(
    z_stream *strm,
    const uint8_t *dictionary,
    uint32_t  dictLength
) {
    deflate_state *s;
    uint32_t str;
    uint32_t n;
    int wrap;
    unsigned avail;
    const uint8_t *next;

    if (deflateStateCheck(strm) || dictionary == NULL)
        return Z_STREAM_ERROR;
    s = strm->state;
    wrap = s->wrap;
    if (wrap == 2 || (wrap == 1 && s->status != INIT_STATE) || s->lookahead)
        return Z_STREAM_ERROR;

    /* when using zlib wrappers, compute Adler-32 for provided dictionary */
    if (wrap == 1)
        strm->adler = adler32.adler32(strm->adler, dictionary, dictLength);
    s->wrap = 0;                    /* avoid computing Adler-32 in read_buf */

    /* if dictionary would fill window, just replace the history */
    if (dictLength >= s->w_size) {
        if (wrap == 0) {            /* already empty otherwise */
            CLEAR_HASH(s);
            s->strstart = 0;
            s->block_start = 0L;
            s->insert = 0;
        }
        dictionary += dictLength - s->w_size;  /* use the tail */
        dictLength = s->w_size;
    }

    /* insert dictionary into window and hash */
    avail = strm->avail_in;
    next = strm->next_in;
    strm->avail_in = dictLength;
    strm->next_in = (uint8_t *)dictionary;
    fill_window(s);
    while (s->lookahead >= MIN_MATCH) {
        str = s->strstart;
        n = s->lookahead - (MIN_MATCH-1);
        while (true) {
            UPDATE_HASH(s, s->window[str + MIN_MATCH-1]);
            s->prev[str & s->w_mask] = s->head[s->ins_h];
            s->head[s->ins_h] = (uint16_t)str;
            str++;
            if (!--n) {
                break;
            }
        }
        s->strstart = str;
        s->lookahead = MIN_MATCH-1;
        fill_window(s);
    }
    s->strstart += s->lookahead;
    s->block_start = (long)s->strstart;
    s->insert = s->lookahead;
    s->lookahead = 0;
    s->match_length = s->prev_length = MIN_MATCH-1;
    s->match_available = 0;
    strm->next_in = next;
    strm->avail_in = avail;
    s->wrap = wrap;
    return Z_OK;
}

/*
     Returns the sliding dictionary being maintained by deflate.  dictLength is
   set to the number of bytes in the dictionary, and that many bytes are copied
   to dictionary.  dictionary must have enough space, where 32768 bytes is
   always enough.  If deflateGetDictionary() is called with dictionary equal to
   NULL, then only the dictionary length is returned, and nothing is copied.
   Similarly, if dictLength is NULL, then it is not set.

     deflateGetDictionary() may return a length less than the window size, even
   when more than the window size in input has been provided. It may return up
   to 258 bytes less in that case, due to how zlib's implementation of deflate
   manages the sliding window and lookahead for matches, where matches can be
   up to 258 bytes long. If the application needs the last window-size bytes of
   input, then that would need to be saved by the application outside of zlib.

     deflateGetDictionary returns Z_OK on success, or Z_STREAM_ERROR if the
   stream state is inconsistent.
*/
pub int deflateGetDictionary(
    z_stream *strm,
    uint8_t *dictionary,
    uint32_t  *dictLength
) {
    deflate_state *s;
    uint32_t len;

    if (deflateStateCheck(strm))
        return Z_STREAM_ERROR;
    s = strm->state;
    len = s->strstart + s->lookahead;
    if (len > s->w_size)
        len = s->w_size;
    if (dictionary != NULL && len)
        memcpy(dictionary, s->window + s->strstart + s->lookahead - len, len);
    if (dictLength != NULL)
        *dictLength = len;
    return Z_OK;
}

/* ========================================================================= */
pub int deflateResetKeep(z_stream *strm) {
    deflate_state *s;

    if (deflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }

    strm->total_in = strm->total_out = 0;
    strm->msg = NULL;
    strm->data_type = Z_UNKNOWN;

    s = strm->state;
    s->pending = 0;
    s->pending_out = s->pending_buf;

    if (s->wrap < 0) {
        s->wrap = -s->wrap; /* was made negative by deflate(..., Z_FINISH); */
    }
    if (s->wrap == 2) {
        s->status = GZIP_STATE;
        strm->adler = crc32.init();
    } else {
        s->status = INIT_STATE;
        strm->adler = adler32.init();
    }
    s->last_flush = -2;

    _tr_init(s);

    return Z_OK;
}

/*
     This function is equivalent to deflateEnd followed by deflateInit, but
   does not free and reallocate the internal compression state.  The stream
   will leave the compression level and any other attributes that may have been
   set unchanged.

     deflateReset returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent.
*/
pub int deflateReset(z_stream *strm) {
    int ret = deflateResetKeep(strm);
    if (ret == Z_OK) {
        lm_init(strm->state);
    }
    return ret;
}

/*
     deflateSetHeader() provides gzip header information for when a gzip
   stream is requested by deflateInit2().  deflateSetHeader() may be called
   after deflateInit2() or deflateReset() and before the first call of
   deflate().  The text, time, os, extra field, name, and comment information
   in the provided gz_header structure are written to the gzip header (xflag is
   ignored -- the extra flags are set according to the compression level).  The
   caller must assure that, if not NULL, name and comment are terminated with
   a zero byte, and that if extra is not NULL, that extra_len bytes are
   available there.  If hcrc is true, a gzip header crc is included.  Note that
   the current versions of the command-line version of gzip (up through version
   1.3.x) do not support header crc's, and will report that it is a "multi-part
   gzip file" and give up.

     If deflateSetHeader is not used, the default gzip header has text false,
   the time set to zero, and os set to 255, with no extra, name, or comment
   fields.  The gzip header is returned to the default state by deflateReset().

     deflateSetHeader returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent.
*/
pub int deflateSetHeader(z_stream *strm, gz_header *head) {
    if (deflateStateCheck(strm) || strm->state->wrap != 2) {
        return Z_STREAM_ERROR;
    }
    strm->state->gzhead = head;
    return Z_OK;
}

/*
     deflatePending() returns the number of bytes and bits of output that have
   been generated, but not yet provided in the available output.  The bytes not
   provided would be due to the available output space having being consumed.
   The number of bits of output not provided are between 0 and 7, where they
   await more bits to join them in order to fill out a full byte.  If pending
   or bits are NULL, then those values are not set.

     deflatePending returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent.
 */
pub int deflatePending(z_stream *strm, unsigned *pending, int *bits) {
    if (deflateStateCheck(strm)) return Z_STREAM_ERROR;
    if (pending != NULL) {
        *pending = strm->state->pending;
    }
    if (bits != NULL) {
        *bits = strm->state->bi_valid;
    }
    return Z_OK;
}

/*
     deflatePrime() inserts bits in the deflate output stream.  The intent
   is that this function is used to start off the deflate output with the bits
   leftover from a previous deflate stream when appending to it.  As such, this
   function can only be used for raw deflate, and must be used before the first
   deflate() call after a deflateInit2() or deflateReset().  bits must be less
   than or equal to 16, and that many of the least significant bits of value
   will be inserted in the output.

     deflatePrime returns Z_OK if success, Z_BUF_ERROR if there was not enough
   room in the internal buffer to insert the bits, or Z_STREAM_ERROR if the
   source stream state was inconsistent.
*/
pub int deflatePrime(z_stream *strm, int bits, int value) {
    if (deflateStateCheck(strm)) return Z_STREAM_ERROR;
    deflate_state *s = strm->state;
    int put;
    if (bits < 0 || bits > 16
        || s->sym_buf < s->pending_out + ((Buf_size + 7) >> 3)) {
        return Z_BUF_ERROR;
    }
    while(true) {
        put = Buf_size - s->bi_valid;
        if (put > bits)
            put = bits;
        s->bi_buf |= (uint16_t)((value & ((1 << put) - 1)) << s->bi_valid);
        s->bi_valid += put;
        _tr_flush_bits(s);
        value >>= put;
        bits -= put;
        if (!bits) {
            break;
        }
    }
    return Z_OK;
}

/*
     Dynamically update the compression level and compression strategy.  The
   interpretation of level and strategy is as in deflateInit2().  This can be
   used to switch between compression and straight copy of the input data, or
   to switch to a different kind of input data requiring a different strategy.
   If the compression approach (which is a function of the level) or the
   strategy is changed, and if there have been any deflate() calls since the
   state was initialized or reset, then the input available so far is
   compressed with the old level and strategy using deflate(strm, Z_BLOCK).
   There are three approaches for the compression levels 0, 1..3, and 4..9
   respectively.  The new level and strategy will take effect at the next call
   of deflate().

     If a deflate(strm, Z_BLOCK) is performed by deflateParams(), and it does
   not have enough output space to complete, then the parameter change will not
   take effect.  In this case, deflateParams() can be called again with the
   same parameters and more output space to try again.

     In order to assure a change in the parameters on the first try, the
   deflate stream should be flushed using deflate() with Z_BLOCK or other flush
   request until strm.avail_out is not zero, before calling deflateParams().
   Then no more input data should be provided before the deflateParams() call.
   If this is done, the old level and strategy will be applied to the data
   compressed before deflateParams(), and the new level and strategy will be
   applied to the the data compressed after deflateParams().

     deflateParams returns Z_OK on success, Z_STREAM_ERROR if the source stream
   state was inconsistent or if a parameter was invalid, or Z_BUF_ERROR if
   there was not enough output space to complete the compression of the
   available input data before a change in the strategy or approach.  Note that
   in the case of a Z_BUF_ERROR, the parameters are not changed.  A return
   value of Z_BUF_ERROR is not fatal, in which case deflateParams() can be
   retried with more output space.
*/
pub int deflateParams(
    z_stream *strm,
    int level,
    int strategy
) {
    if (deflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }
    deflate_state *s = strm->state;
    compress_func *func;

    if (level == Z_DEFAULT_COMPRESSION) level = 6;
    if (level < 0 || level > 9 || strategy < 0 || strategy > Z_FIXED) {
        return Z_STREAM_ERROR;
    }
    func = configuration_table[s->level].func;

    if ((strategy != s->strategy || func != configuration_table[level].func) &&
        s->last_flush != -2) {
        /* Flush the last buffer: */
        int err = deflate(strm, Z_BLOCK);
        if (err == Z_STREAM_ERROR)
            return err;
        if (strm->avail_in || (s->strstart - s->block_start) + s->lookahead)
            return Z_BUF_ERROR;
    }
    if (s->level != level) {
        if (s->level == 0 && s->matches != 0) {
            if (s->matches == 1)
                slide_hash(s);
            else
                CLEAR_HASH(s);
            s->matches = 0;
        }
        s->level = level;
        s->max_lazy_match   = configuration_table[level].max_lazy;
        s->good_match       = configuration_table[level].good_length;
        s->nice_match       = configuration_table[level].nice_length;
        s->max_chain_length = configuration_table[level].max_chain;
    }
    s->strategy = strategy;
    return Z_OK;
}

/*
     Fine tune deflate's internal compression parameters.  This should only be
   used by someone who understands the algorithm used by zlib's deflate for
   searching for the best matching string, and even then only by the most
   fanatic optimizer trying to squeeze out the last compressed bit for their
   specific input data.  Read the deflate.c source code for the meaning of the
   max_lazy, good_length, nice_length, and max_chain parameters.

     deflateTune() can be called after deflateInit() or deflateInit2(), and
   returns Z_OK on success, or Z_STREAM_ERROR for an invalid deflate stream.
 */
pub int deflateTune(
    z_stream *strm,
    int good_length,
    int max_lazy,
    int nice_length,
    int max_chain
) {
    deflate_state *s;
    if (deflateStateCheck(strm)) return Z_STREAM_ERROR;
    s = strm->state;
    s->good_match = (uint32_t)good_length;
    s->max_lazy_match = (uint32_t)max_lazy;
    s->nice_match = nice_length;
    s->max_chain_length = (uint32_t)max_chain;
    return Z_OK;
}

/*
     deflateBound() returns an upper bound on the compressed size after
   deflation of sourceLen bytes.  It must be called after deflateInit() or
   deflateInit2(), and after deflateSetHeader(), if used.  This would be used
   to allocate an output buffer for deflation in a single pass, and so would be
   called before deflate().  If that first deflate() call is provided the
   sourceLen input bytes, an output buffer allocated to the size returned by
   deflateBound(), and the flush value Z_FINISH, then deflate() is guaranteed
   to return Z_STREAM_END.  Note that it is possible for the compressed size to
   be larger than the value returned by deflateBound() if flush options other
   than Z_FINISH or Z_NO_FLUSH are used.
*/
/* =========================================================================
 * For the default windowBits of 15 and memLevel of 8, this function returns a
 * close to exact, as well as small, upper bound on the compressed size. This
 * is an expansion of ~0.03%, plus a small constant.
 *
 * For any setting other than those defaults for windowBits and memLevel, one
 * of two worst case bounds is returned. This is at most an expansion of ~4% or
 * ~13%, plus a small constant.
 *
 * Both the 0.03% and 4% derive from the overhead of stored blocks. The first
 * one is for stored blocks of 16383 bytes (memLevel == 8), whereas the second
 * is for stored blocks of 127 bytes (the worst case memLevel == 1). The
 * expansion results from five bytes of header for each stored block.
 *
 * The larger expansion of 13% results from a window size less than or equal to
 * the symbols buffer size (windowBits <= memLevel + 7). In that case some of
 * the data being compressed may have slid out of the sliding window, impeding
 * a stored block from being emitted. Then the only choice is a fixed or
 * dynamic block, where a fixed block limits the maximum expansion to 9 bits
 * per 8-bit byte, plus 10 bits for every block. The smallest block size for
 * which this can occur is 255 (memLevel == 2).
 *
 * Shifts are used to approximate divisions, for speed.
 */
pub uint32_t deflateBound(z_stream *strm, uint32_t sourceLen) {
    deflate_state *s;
    uint32_t fixedlen;
    uint32_t storelen;
    uint32_t wraplen;

    /* upper bound for fixed blocks with 9-bit literals and length 255
       (memLevel == 2, which is the lowest that may not use stored blocks) --
       ~13% overhead plus a small constant */
    fixedlen = sourceLen + (sourceLen >> 3) + (sourceLen >> 8) +
               (sourceLen >> 9) + 4;

    /* upper bound for stored blocks with length 127 (memLevel == 1) --
       ~4% overhead plus a small constant */
    storelen = sourceLen + (sourceLen >> 5) + (sourceLen >> 7) +
               (sourceLen >> 11) + 7;

    /* if can't get parameters, return larger bound plus a zlib wrapper */
    if (deflateStateCheck(strm)) {
        if (fixedlen > storelen) {
            return fixedlen + 6;
        } else {
            return storelen + 6;
        }
    }

    /* compute wrapper length */
    s = strm->state;
    switch (s->wrap) {
        /* raw deflate */
    case 0: {
        wraplen = 0;
    }
    /* zlib wrapper */
    case 1: {
        if (s->strstart) {
            wraplen = 6 + 4;
        } else {
            wraplen = 6 + 0;
        }
    }
    /* gzip wrapper */
    case 2: {
        wraplen = 18;
        if (s->gzhead != NULL) {          /* user-supplied gzip header */
            uint8_t *str;
            if (s->gzhead->extra != NULL)
                wraplen += 2 + s->gzhead->extra_len;
            str = s->gzhead->name;
            if (str != NULL)
                while (true) {
                    wraplen++;
                    if (!*str++) break;
                }
            str = s->gzhead->comment;
            if (str != NULL)
                while (true) {
                    wraplen++;
                    if (!*str++) break;
                }
            if (s->gzhead->hcrc)
                wraplen += 2;
        }
    }
    /* for compiler happiness */
    default: {wraplen = 6; }
    }

    /* if not default parameters, return one of the conservative bounds */
    if (s->w_bits != 15 || s->hash_bits != 8 + 7) {
        if (s->w_bits <= s->hash_bits) {
            return fixedlen + wraplen;
        } else {
            return storelen + wraplen;
        }
    }

    /* default settings: return tight bound for that case -- ~0.03% overhead
       plus a small constant */
    return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) +
           (sourceLen >> 25) + 13 - 6 + wraplen;
}

/* =========================================================================
 * Put a short in the pending buffer. The 16-bit value is put in MSB order.
 * IN assertion: the stream state is correct and there is enough room in
 * pending_buf.
 */
void putShortMSB(deflate_state *s, uint32_t b) {
    put_byte(s, (uint8_t)(b >> 8));
    put_byte(s, (uint8_t)(b & 0xff));
}

/* =========================================================================
 * Flush as much pending output as possible. All deflate() output, except for
 * some deflate_stored() output, goes through this function so some
 * applications may wish to modify it to avoid allocating a large
 * strm->next_out buffer and copying into it. (See also read_buf()).
 */
void flush_pending(z_stream *strm) {
    unsigned len;
    deflate_state *s = strm->state;

    _tr_flush_bits(s);
    len = s->pending;
    if (len > strm->avail_out) len = strm->avail_out;
    if (len == 0) return;

    memcpy(strm->next_out, s->pending_out, len);
    strm->next_out  += len;
    s->pending_out  += len;
    strm->total_out += len;
    strm->avail_out -= len;
    s->pending      -= len;
    if (s->pending == 0) {
        s->pending_out = s->pending_buf;
    }
}

/* ===========================================================================
 * Update the header CRC with the bytes s->pending_buf[beg..s->pending - 1].
 */
void HCRC_UPDATE(z_stream *strm, uint32_t beg) {
    deflate_state *s = strm->state;
    if (s->gzhead->hcrc && s->pending > beg) {
        strm->adler = crc32.crc32(strm->adler, s->pending_buf + beg, s->pending - beg);
    }
}

/*
    deflate compresses as much data as possible, and stops when the input
  buffer becomes empty or the output buffer becomes full.  It may introduce
  some output latency (reading input without producing any output) except when
  forced to flush.

deflate performs one or both of the following actions:

  - Compress more input starting at next_in and update next_in and avail_in
    accordingly.  If not all input can be processed (because there is not
    enough room in the output buffer), next_in and avail_in are updated and
    processing will resume at this point for the next call of deflate().

  - Generate more output starting at next_out and update next_out and avail_out
    accordingly.  This action is forced if the parameter flush is non zero.
    Forcing flush frequently degrades the compression ratio, so this parameter
    should be set only when necessary.  Some output may be provided even if
    flush is zero.

    Before the call of deflate(), the application should ensure that at least
  one of the actions is possible, by providing more input and/or consuming more
  output, and updating avail_in or avail_out accordingly; avail_out should
  never be zero before the call.  The application can consume the compressed
  output when it wants, for example when the output buffer is full (avail_out
  == 0), or after each call of deflate().  If deflate returns Z_OK and with
  zero avail_out, it must be called again after making room in the output
  buffer because there might be more output pending. See deflatePending(),
  which can be used if desired to determine whether or not there is more output
  in that case.

    Normally the parameter flush is set to Z_NO_FLUSH, which allows deflate to
  decide how much data to accumulate before producing output, in order to
  maximize compression.

    If the parameter flush is set to Z_SYNC_FLUSH, all pending output is
  flushed to the output buffer and the output is aligned on a byte boundary, so
  that the decompressor can get all input data available so far.  (In
  particular avail_in is zero after the call if enough output space has been
  provided before the call.) Flushing may degrade compression for some
  compression algorithms and so it should be used only when necessary.  This
  completes the current deflate block and follows it with an empty stored block
  that is three bits plus filler bits to the next byte, followed by four bytes
  (00 00 ff ff).

    If flush is set to stream.Z_PARTIAL_FLUSH, all pending output is flushed to the
  output buffer, but the output is not aligned to a byte boundary.  All of the
  input data so far will be available to the decompressor, as for Z_SYNC_FLUSH.
  This completes the current deflate block and follows it with an empty fixed
  codes block that is 10 bits long.  This assures that enough bytes are output
  in order for the decompressor to finish the block before the empty fixed
  codes block.

    If flush is set to Z_BLOCK, a deflate block is completed and emitted, as
  for Z_SYNC_FLUSH, but the output is not aligned on a byte boundary, and up to
  seven bits of the current block are held to be written as the next byte after
  the next deflate block is completed.  In this case, the decompressor may not
  be provided enough bits at this point in order to complete decompression of
  the data provided so far to the compressor.  It may need to wait for the next
  block to be emitted.  This is for advanced applications that need to control
  the emission of deflate blocks.

    If flush is set to Z_FULL_FLUSH, all output is flushed as with
  Z_SYNC_FLUSH, and the compression state is reset so that decompression can
  restart from this point if previous compressed data has been damaged or if
  random access is desired.  Using Z_FULL_FLUSH too often can seriously degrade
  compression.

    If deflate returns with avail_out == 0, this function must be called again
  with the same value of the flush parameter and more output space (updated
  avail_out), until the flush is complete (deflate returns with non-zero
  avail_out).  In the case of a Z_FULL_FLUSH or Z_SYNC_FLUSH, make sure that
  avail_out is greater than six to avoid repeated flush markers due to
  avail_out == 0 on return.

    If the parameter flush is set to Z_FINISH, pending input is processed,
  pending output is flushed and deflate returns with Z_STREAM_END if there was
  enough output space.  If deflate returns with Z_OK or Z_BUF_ERROR, this
  function must be called again with Z_FINISH and more output space (updated
  avail_out) but no more input data, until it returns with Z_STREAM_END or an
  error.  After deflate has returned Z_STREAM_END, the only possible operations
  on the stream are deflateReset or deflateEnd.

    Z_FINISH can be used in the first deflate call after deflateInit if all the
  compression is to be done in a single step.  In order to complete in one
  call, avail_out must be at least the value returned by deflateBound (see
  below).  Then deflate is guaranteed to return Z_STREAM_END.  If not enough
  output space is provided, deflate will not return Z_STREAM_END, and it must
  be called again as described above.

    deflate() sets strm->adler to the Adler-32 checksum of all input read
  so far (that is, total_in bytes).  If a gzip stream is being generated, then
  strm->adler will be the CRC-32 checksum of the input read so far.  (See
  deflateInit2 below.)

    deflate() may update strm->data_type if it can make a good guess about
  the input data type (stream.Z_BINARY or stream.Z_TEXT).  If in doubt, the data is
  considered binary.  This field is only for information purposes and does not
  affect the compression algorithm in any manner.

    deflate() returns Z_OK if some progress has been made (more input
  processed or more output produced), Z_STREAM_END if all input has been
  consumed and all output has been produced (only when flush is set to
  Z_FINISH), Z_STREAM_ERROR if the stream state was inconsistent (for example
  if next_in or next_out was NULL or the state was inadvertently written over
  by the application), or Z_BUF_ERROR if no progress is possible (for example
  avail_in or avail_out was zero).  Note that Z_BUF_ERROR is not fatal, and
  deflate() can be called again with more input and more output space to
  continue compressing.
*/
pub int deflate(z_stream *strm, int flush) {
    if (deflateStateCheck(strm) || flush > Z_BLOCK || flush < 0) {
        return Z_STREAM_ERROR;
    }

    deflate_state *s = strm->state;

    if (strm->next_out == NULL ||
        (strm->avail_in != 0 && strm->next_in == NULL) ||
        (s->status == FINISH_STATE && flush != Z_FINISH)) {
        return ERR_RETURN(strm, Z_STREAM_ERROR);
    }
    if (strm->avail_out == 0) {
        return ERR_RETURN(strm, Z_BUF_ERROR);
    }

    int old_flush = s->last_flush;
    s->last_flush = flush;

    /* Flush as much pending output as possible */
    if (s->pending != 0) {
        flush_pending(strm);
        if (strm->avail_out == 0) {
            /*
            Since avail_out is 0, deflate will be called again with
            more output space, but possibly with both pending and
            avail_in equal to zero. There won't be anything to do,
            but this is not an error situation so make sure we
            return OK instead of BUF_ERROR at next call of deflate:
            */
            s->last_flush = -1;
            return Z_OK;
        }
    }
    else if (strm->avail_in == 0 && RANK(flush) <= RANK(old_flush) && flush != Z_FINISH) {
        /* Make sure there is something to do and avoid duplicate consecutive
        flushes. For repeated and useless calls with Z_FINISH, we keep
        returning Z_STREAM_END instead of Z_BUF_ERROR. */
        return ERR_RETURN(strm, Z_BUF_ERROR);
    }

    /* User must not provide more input after the first FINISH: */
    if (s->status == FINISH_STATE && strm->avail_in != 0) {
        return ERR_RETURN(strm, Z_BUF_ERROR);
    }

    /* Write the header */
    if (s->status == INIT_STATE && s->wrap == 0) {
        s->status = BUSY_STATE;
    }
    if (s->status == INIT_STATE) {
        /* zlib header */
        uint32_t header = (Z_DEFLATED + ((s->w_bits - 8) << 4)) << 8;
        uint32_t level_flags;

        if (s->strategy >= Z_HUFFMAN_ONLY || s->level < 2)
            level_flags = 0;
        else if (s->level < 6)
            level_flags = 1;
        else if (s->level == 6)
            level_flags = 2;
        else
            level_flags = 3;
        header |= (level_flags << 6);
        if (s->strstart != 0) header |= PRESET_DICT;
        header += 31 - (header % 31);

        putShortMSB(s, header);

        /* Save the adler32 of the preset dictionary: */
        if (s->strstart != 0) {
            putShortMSB(s, (uint32_t)(strm->adler >> 16));
            putShortMSB(s, (uint32_t)(strm->adler & 0xffff));
        }
        strm->adler = adler32.init();
        s->status = BUSY_STATE;

        /* Compression must start with an empty pending buffer */
        flush_pending(strm);
        if (s->pending != 0) {
            s->last_flush = -1;
            return Z_OK;
        }
    }
    if (s->status == GZIP_STATE) {
        /* gzip header */
        strm->adler = crc32.init();
        put_byte(s, 31);
        put_byte(s, 139);
        put_byte(s, 8);
        if (s->gzhead == NULL) {
            put_byte(s, 0);
            put_byte(s, 0);
            put_byte(s, 0);
            put_byte(s, 0);
            put_byte(s, 0);
            put_byte(s, get_dunno_what1(s));
            put_byte(s, OS_CODE);
            s->status = BUSY_STATE;

            /* Compression must start with an empty pending buffer */
            flush_pending(strm);
            if (s->pending != 0) {
                s->last_flush = -1;
                return Z_OK;
            }
        }
        else {
            int x1286 = 0;
            if (s->gzhead->text) x1286++;
            if (s->gzhead->hcrc) x1286 += 2;
            if (s->gzhead->extra != NULL) x1286 += 4;
            if (s->gzhead->name != NULL) x1286 += 8;
            if (s->gzhead->comment != NULL) x1286 += 16;

            put_byte(s, x1286);
            put_byte(s, (uint8_t)(s->gzhead->time & 0xff));
            put_byte(s, (uint8_t)((s->gzhead->time >> 8) & 0xff));
            put_byte(s, (uint8_t)((s->gzhead->time >> 16) & 0xff));
            put_byte(s, (uint8_t)((s->gzhead->time >> 24) & 0xff));
            put_byte(s, get_dunno_what1(s));
            put_byte(s, s->gzhead->os & 0xff);
            if (s->gzhead->extra != NULL) {
                put_byte(s, s->gzhead->extra_len & 0xff);
                put_byte(s, (s->gzhead->extra_len >> 8) & 0xff);
            }
            if (s->gzhead->hcrc) {
                strm->adler = crc32.crc32(strm->adler, s->pending_buf, s->pending);
            }
            s->gzindex = 0;
            s->status = EXTRA_STATE;
        }
    }
    if (s->status == EXTRA_STATE) {
        if (s->gzhead->extra != NULL) {
            uint32_t beg = s->pending;   /* start of bytes to update crc */
            uint32_t left = (s->gzhead->extra_len & 0xffff) - s->gzindex;
            while (s->pending + left > s->pending_buf_size) {
                uint32_t copy = s->pending_buf_size - s->pending;
                memcpy(s->pending_buf + s->pending,
                        s->gzhead->extra + s->gzindex, copy);
                s->pending = s->pending_buf_size;
                HCRC_UPDATE(strm, beg);
                s->gzindex += copy;
                flush_pending(strm);
                if (s->pending != 0) {
                    s->last_flush = -1;
                    return Z_OK;
                }
                beg = 0;
                left -= copy;
            }
            memcpy(s->pending_buf + s->pending,
                    s->gzhead->extra + s->gzindex, left);
            s->pending += left;
            HCRC_UPDATE(strm, beg);
            s->gzindex = 0;
        }
        s->status = NAME_STATE;
    }
    if (s->status == NAME_STATE) {
        if (s->gzhead->name != NULL) {
            uint32_t beg = s->pending;   /* start of bytes to update crc */
            int val;
            while (true) {
                if (s->pending == s->pending_buf_size) {
                    HCRC_UPDATE(strm, beg);
                    flush_pending(strm);
                    if (s->pending != 0) {
                        s->last_flush = -1;
                        return Z_OK;
                    }
                    beg = 0;
                }
                val = s->gzhead->name[s->gzindex++];
                put_byte(s, val);
                if (val == 0) break;
            }
            HCRC_UPDATE(strm, beg);
            s->gzindex = 0;
        }
        s->status = COMMENT_STATE;
    }
    if (s->status == COMMENT_STATE) {
        if (s->gzhead->comment != NULL) {
            uint32_t beg = s->pending;   /* start of bytes to update crc */
            int val;
            while (true) {
                if (s->pending == s->pending_buf_size) {
                    HCRC_UPDATE(strm, beg);
                    flush_pending(strm);
                    if (s->pending != 0) {
                        s->last_flush = -1;
                        return Z_OK;
                    }
                    beg = 0;
                }
                val = s->gzhead->comment[s->gzindex++];
                put_byte(s, val);
                if (val == 0) break;
            }
            HCRC_UPDATE(strm, beg);
        }
        s->status = HCRC_STATE;
    }
    if (s->status == HCRC_STATE) {
        if (s->gzhead->hcrc) {
            if (s->pending + 2 > s->pending_buf_size) {
                flush_pending(strm);
                if (s->pending != 0) {
                    s->last_flush = -1;
                    return Z_OK;
                }
            }
            put_byte(s, (uint8_t)(strm->adler & 0xff));
            put_byte(s, (uint8_t)((strm->adler >> 8) & 0xff));
            strm->adler = crc32.init();
        }
        s->status = BUSY_STATE;

        /* Compression must start with an empty pending buffer */
        flush_pending(strm);
        if (s->pending != 0) {
            s->last_flush = -1;
            return Z_OK;
        }
    }

    /* Start a new block or continue the current one.
     */
    if (strm->avail_in != 0 || s->lookahead != 0 ||
        (flush != Z_NO_FLUSH && s->status != FINISH_STATE)) {
        int bstate;

        if (s->level == 0) {
            bstate = deflate_stored(s, flush);
        } else if (s->strategy == Z_HUFFMAN_ONLY) {
            bstate = deflate_huff(s, flush);
        } else if (s->strategy == Z_RLE) {
            bstate = s->strategy == deflate_rle(s, flush);
        } else {
            bstate = configuration_table[s->level].func(s, flush);
        }

        if (bstate == finish_started || bstate == finish_done) {
            s->status = FINISH_STATE;
        }
        if (bstate == need_more || bstate == finish_started) {
            if (strm->avail_out == 0) {
                s->last_flush = -1; /* avoid BUF_ERROR next call, see above */
            }
            return Z_OK;
            /* If flush != Z_NO_FLUSH && avail_out == 0, the next call
             * of deflate should use the same flush parameter to make sure
             * that the flush is complete. So we don't have to output an
             * empty block here, this will be done at next call. This also
             * ensures that for a very small output buffer, we emit at most
             * one empty block.
             */
        }
        if (bstate == block_done) {
            if (flush == Z_PARTIAL_FLUSH) {
                _tr_align(s);
            } else if (flush != Z_BLOCK) { /* FULL_FLUSH or SYNC_FLUSH */
                _tr_stored_block(s, (char*)0, 0L, 0);
                /* For a full flush, this empty block will be recognized
                 * as a special marker by inflate_sync().
                 */
                if (flush == Z_FULL_FLUSH) {
                    CLEAR_HASH(s);             /* forget history */
                    if (s->lookahead == 0) {
                        s->strstart = 0;
                        s->block_start = 0L;
                        s->insert = 0;
                    }
                }
            }
            flush_pending(strm);
            if (strm->avail_out == 0) {
              s->last_flush = -1; /* avoid BUF_ERROR at next call, see above */
              return Z_OK;
            }
        }
    }

    if (flush != Z_FINISH) return Z_OK;
    if (s->wrap <= 0) return Z_STREAM_END;

    /* Write the trailer */
    if (s->wrap == 2) {
        put_byte(s, (uint8_t)(strm->adler & 0xff));
        put_byte(s, (uint8_t)((strm->adler >> 8) & 0xff));
        put_byte(s, (uint8_t)((strm->adler >> 16) & 0xff));
        put_byte(s, (uint8_t)((strm->adler >> 24) & 0xff));
        put_byte(s, (uint8_t)(strm->total_in & 0xff));
        put_byte(s, (uint8_t)((strm->total_in >> 8) & 0xff));
        put_byte(s, (uint8_t)((strm->total_in >> 16) & 0xff));
        put_byte(s, (uint8_t)((strm->total_in >> 24) & 0xff));
    }
    else
    {
        putShortMSB(s, (uint32_t)(strm->adler >> 16));
        putShortMSB(s, (uint32_t)(strm->adler & 0xffff));
    }
    flush_pending(strm);
    /* If avail_out is zero, the application will call deflate again
     * to flush the rest.
     */
    if (s->wrap > 0) s->wrap = -s->wrap; /* write the trailer only once! */
    if (s->pending != 0) {
        return Z_OK;
    }
    return Z_STREAM_END;
}

int get_dunno_what1(deflate_state *s) {
    if (s->level == 9) { return 2; }
    if (s->strategy >= Z_HUFFMAN_ONLY || s->level < 2) {
        return 4;
    }
    return 0;
}

/*
 * Frees all dynamically allocated data structures for this stream.
 * Discards any unprocessed input and does not flush any pending output.
 * Returns Z_OK if success,
 * Z_STREAM_ERROR if the stream state was inconsistent,
 * Z_DATA_ERROR if the stream was freed prematurely (some input or output was discarded).
 * In the error case, msg may be set to a static string (which must not be deallocated).
 */
pub int deflateEnd(z_stream *strm) {
    if (deflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }
    int status = strm->state->status;

    /* Deallocate in reverse order of allocations: */
    if (strm->state->pending_buf) {
        free(strm->state->pending_buf);
    }
    if (strm->state->head) {
        free(strm->state->head);
    }
    if (strm->state->prev) {
        free(strm->state->prev);
    }
    if (strm->state->window) {
        free(strm->state->window);
    }

    free(strm->state);
    strm->state = NULL;

    if (status == BUSY_STATE) {
        return Z_DATA_ERROR;
    }
    return Z_OK;
}

/* ===========================================================================
 * Read a new buffer from the current input stream, update the adler32
 * and total number of bytes read.  All deflate() input goes through
 * this function so some applications may wish to modify it to avoid
 * allocating a large strm->next_in buffer and copying from it.
 * (See also flush_pending()).
 */
unsigned read_buf(
    z_stream *strm,
    uint8_t *buf,
    unsigned size
) {
    unsigned len = strm->avail_in;

    if (len > size) len = size;
    if (len == 0) return 0;

    strm->avail_in  -= len;

    memcpy(buf, strm->next_in, len);
    if (strm->state->wrap == 1) {
        strm->adler = adler32.adler32(strm->adler, buf, len);
    }
    else if (strm->state->wrap == 2) {
        strm->adler = crc32.crc32(strm->adler, buf, len);
    }
    strm->next_in  += len;
    strm->total_in += len;

    return len;
}

/* ===========================================================================
 * Initialize the "longest match" routines for a new zlib stream
 */
void lm_init(deflate_state *s) {
    s->window_size = (uint32_t)2L*s->w_size;

    CLEAR_HASH(s);

    /* Set the default configuration parameters:
     */
    s->max_lazy_match   = configuration_table[s->level].max_lazy;
    s->good_match       = configuration_table[s->level].good_length;
    s->nice_match       = configuration_table[s->level].nice_length;
    s->max_chain_length = configuration_table[s->level].max_chain;

    s->strstart = 0;
    s->block_start = 0L;
    s->lookahead = 0;
    s->insert = 0;
    s->match_length = s->prev_length = MIN_MATCH-1;
    s->match_available = 0;
    s->ins_h = 0;
}

/* ===========================================================================
 * Set match_start to the longest match starting at the given string and
 * return its length. Matches shorter or equal to prev_length are discarded,
 * in which case the result is equal to prev_length and match_start is
 * garbage.
 * IN assertions: cur_match is the head of the hash chain for the current
 *   string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
 * OUT assertion: the match length is not greater than s->lookahead.
 */
uint32_t longest_match(
    deflate_state *s,
    uint32_t cur_match // current match
) {
    unsigned chain_length = s->max_chain_length;/* max hash chain length */
    uint8_t *scan = s->window + s->strstart; /* current string */
    uint8_t *match;                      /* matched string */
    int len;                           /* length of current match */
    int best_len = (int)s->prev_length;         /* best match length so far */
    int nice_match = s->nice_match;             /* stop if match long enough */
    uint32_t limit = NIL;
    if (s->strstart > (uint32_t)MAX_DIST(s)) {
        limit = s->strstart - (uint32_t)MAX_DIST(s);
    }

    /* Stop when cur_match becomes <= limit. To simplify the code,
     * we prevent matches with the string of window index 0.
     */
    uint16_t *prev = s->prev;
    uint32_t wmask = s->w_mask;

    uint8_t *strend = s->window + s->strstart + MAX_MATCH;
    uint8_t scan_end1  = scan[best_len - 1];
    uint8_t scan_end   = scan[best_len];

    /* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
     * It is easy to get rid of this optimization if necessary.
     */
    assert.Assert(s->hash_bits >= 8 && MAX_MATCH == 258, "Code too clever");

    /* Do not waste too much time if we already have a good match: */
    if (s->prev_length >= s->good_match) {
        chain_length >>= 2;
    }
    /* Do not look for matches beyond the end of the input. This is necessary
     * to make deflate deterministic.
     */
    if ((uint32_t)nice_match > s->lookahead) nice_match = (int)s->lookahead;

    assert.Assert((uint32_t)s->strstart <= s->window_size - MIN_LOOKAHEAD,
           "need lookahead");

    while (true) {
        assert.Assert(cur_match < s->strstart, "no future");
        match = s->window + cur_match;

        /* Skip to next match if the match length cannot increase
         * or if the match length is less than 2.  Note that the checks below
         * for insufficient lookahead only occur occasionally for performance
         * reasons.  Therefore uninitialized memory will be accessed, and
         * conditional jumps will be made that depend on those values.
         * However the length of the match is limited to the lookahead, so
         * the output of deflate is not affected by the uninitialized values.
         */

        if (match[best_len]     != scan_end  ||
            match[best_len - 1] != scan_end1 ||
            *match              != *scan     ||
            *++match            != scan[1])      continue;

        /* The check at best_len - 1 can be removed because it will be made
         * again later. (This heuristic is not always a win.)
         * It is not necessary to compare scan[2] and match[2] since they
         * are always equal when the other bytes match, given that
         * the hash keys are equal and that HASH_BITS >= 8.
         */
        scan += 2;
        match++;
        assert.Assert(*scan == *match, "match[2]");

        /* We check for insufficient lookahead only every 8th comparison;
         * the 256th check will be made at strstart + 258.
         */
        while (true) {
            bool cont = (*++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 scan < strend);
            if (!cont) {
                break;
            }
        }

        assert.Assert(scan <= s->window + (unsigned)(s->window_size - 1),
               "wild scan");

        len = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;


        if (len > best_len) {
            s->match_start = cur_match;
            best_len = len;
            if (len >= nice_match) break;
            scan_end1  = scan[best_len - 1];
            scan_end   = scan[best_len];
        }
        bool cont = (cur_match = prev[cur_match & wmask]) > limit
             && --chain_length != 0;
        if (!cont) {
            break;
        }
    }

    if ((uint32_t)best_len <= s->lookahead) return (uint32_t)best_len;
    return s->lookahead;
}

/* Check that the match at match_start is indeed a match.*/
void check_match(deflate_state *s, uint32_t start, match, int length) {
    /* check that the match is indeed a match */
    if (memcmp(s->window + match, s->window + start, length) != 0) {
        fprintf(stderr, " start %u, match %u, length %d\n",
                start, match, length);
        while (true) {
            fprintf(stderr, "%c%c", s->window[match++], s->window[start++]);
            bool cont = (--length != 0);
            if (!cont) break;
        }
        panic("invalid match");
    }
    if (trace.level() > 1) {
        fprintf(stderr,"\\[%d,%d]", start - match, length);
        while (true) {
            putc(s->window[start++], stderr);
            bool cont = (--length != 0);
            if (!cont) break;
        }
    }
}

/* ===========================================================================
 * Fill the window when the lookahead becomes insufficient.
 * Updates strstart and lookahead.
 *
 * IN assertion: lookahead < MIN_LOOKAHEAD
 * OUT assertions: strstart <= window_size-MIN_LOOKAHEAD
 *    At least one byte has been read, or avail_in == 0; reads are
 *    performed for at least two bytes (required for the zip translate_eol
 *    option -- not supported here).
 */
void fill_window(deflate_state *s) {
    unsigned n;
    unsigned more;    /* Amount of free space at the end of the window. */
    uint32_t wsize = s->w_size;

    assert.Assert(s->lookahead < MIN_LOOKAHEAD, "already enough lookahead");

    while (true) {
        more = (unsigned)(s->window_size -(uint32_t)s->lookahead -(uint32_t)s->strstart);

        /* Deal with !@#$% 64K limit: */
        if (sizeof(int) <= 2) {
            if (more == 0 && s->strstart == 0 && s->lookahead == 0) {
                more = wsize;

            } else if (more == (unsigned)(-1)) {
                /* Very unlikely, but possible on 16 bit machine if
                 * strstart == 0 && lookahead == 1 (input done a byte at time)
                 */
                more--;
            }
        }

        /* If the window is almost full and there is insufficient lookahead,
         * move the upper half to the lower one to make room in the upper half.
         */
        if (s->strstart >= wsize + MAX_DIST(s)) {

            memcpy(s->window, s->window + wsize, (unsigned)wsize - more);
            s->match_start -= wsize;
            s->strstart    -= wsize; /* we now have strstart >= MAX_DIST */
            s->block_start -= (long) wsize;
            if (s->insert > s->strstart)
                s->insert = s->strstart;
            slide_hash(s);
            more += wsize;
        }
        if (s->strm->avail_in == 0) break;

        /* If there was no sliding:
         *    strstart <= WSIZE+MAX_DIST-1 && lookahead <= MIN_LOOKAHEAD - 1 &&
         *    more == window_size - lookahead - strstart
         * => more >= window_size - (MIN_LOOKAHEAD-1 + WSIZE + MAX_DIST-1)
         * => more >= window_size - 2*WSIZE + 2
         * In the BIG_MEM or MMAP case (not yet supported),
         *   window_size == input_size + MIN_LOOKAHEAD  &&
         *   strstart + s->lookahead <= input_size => more >= MIN_LOOKAHEAD.
         * Otherwise, window_size == 2*WSIZE so more >= 2.
         * If there was sliding, more >= WSIZE. So in all cases, more >= 2.
         */
        assert.Assert(more >= 2, "more < 2");

        n = read_buf(s->strm, s->window + s->strstart + s->lookahead, more);
        s->lookahead += n;

        /* Initialize the hash value now that we have some input: */
        if (s->lookahead + s->insert >= MIN_MATCH) {
            uint32_t str = s->strstart - s->insert;
            s->ins_h = s->window[str];
            UPDATE_HASH(s, s->window[str + 1]);
            while (s->insert) {
                UPDATE_HASH(s, s->window[str + MIN_MATCH-1]);
                s->prev[str & s->w_mask] = s->head[s->ins_h];
                s->head[s->ins_h] = (uint16_t)str;
                str++;
                s->insert--;
                if (s->lookahead + s->insert < MIN_MATCH)
                    break;
            }
        }
        /* If the whole input has less than MIN_MATCH bytes, ins_h is garbage,
         * but this is not important since only literal bytes will be emitted.
         */
        bool cont = (s->lookahead < MIN_LOOKAHEAD && s->strm->avail_in != 0);
        if (!cont) break;
    }

    /* If the WIN_INIT bytes after the end of the current data have never been
     * written, then zero those bytes in order to avoid memory check reports of
     * the use of uninitialized (or uninitialised as Julian writes) bytes by
     * the longest match routines.  Update the high water mark for the next
     * time through here.  WIN_INIT is set to MAX_MATCH since the longest match
     * routines allow scanning to strstart + MAX_MATCH, ignoring lookahead.
     */
    if (s->high_water < s->window_size) {
        uint32_t curr = s->strstart + (uint32_t)(s->lookahead);
        uint32_t init;

        if (s->high_water < curr) {
            /* Previous high water mark below current data -- zero WIN_INIT
             * bytes or up to end of window, whichever is less.
             */
            init = s->window_size - curr;
            if (init > WIN_INIT)
                init = WIN_INIT;
            zmemzero(s->window + curr, (unsigned)init);
            s->high_water = curr + init;
        }
        else if (s->high_water < (uint32_t)curr + WIN_INIT) {
            /* High water mark at or above current data, but below current data
             * plus WIN_INIT -- zero out to current data plus WIN_INIT, or up
             * to end of window, whichever is less.
             */
            init = (uint32_t)curr + WIN_INIT - s->high_water;
            if (init > s->window_size - s->high_water)
                init = s->window_size - s->high_water;
            zmemzero(s->window + s->high_water, (unsigned)init);
            s->high_water += init;
        }
    }

    assert.Assert((uint32_t)s->strstart <= s->window_size - MIN_LOOKAHEAD,
           "not enough room for search");
}

/* ===========================================================================
 * Flush the current block, with given end-of-file flag.
 * IN assertion: strstart is set to the end of the current match.
 */
void FLUSH_BLOCK_ONLY(deflate_state *s, int last) {
    char *foo = NULL;
    if (s->block_start >= 0L) {
        foo = ((char *)&s->window[(unsigned)s->block_start]);
    }
    _tr_flush_block(s, foo, (uint32_t)((long)s->strstart - s->block_start), (last));
    s->block_start = s->strstart;
    flush_pending(s->strm);
    trace.Tracev(stderr,"[FLUSH]");
}

enum {
    NO_RETURN = 123456789 // fake value to deal with macros
};

/* Same but force premature exit if necessary. */
int FLUSH_BLOCK(deflate_state *s, int last) {
    FLUSH_BLOCK_ONLY(s, last);
    if (s->strm->avail_out == 0) {
        if (last) {
            return finish_started;
        }
        return need_more;
    }
    return NO_RETURN;
}




/* ===========================================================================
 * Copy without compression as much as possible from the input stream, return
 * the current block state.
 *
 * In case deflateParams() is used to later switch to a non-zero compression
 * level, s->matches (otherwise unused when storing) keeps track of the number
 * of hash table slides to perform. If s->matches is 1, then one hash table
 * slide will be done when switching. If s->matches is 2, the maximum value
 * allowed here, then the hash table will be cleared, since two or more slides
 * is the same as a clear.
 *
 * deflate_stored() is written to minimize the number of times an input byte is
 * copied. It is most efficient with large input and output buffers, which
 * maximizes the opportunities to have a single copy from next_in to next_out.
 */
int deflate_stored(deflate_state *s, int flush) {
    /* Smallest worthy block size when not flushing or finishing. By default
     * this is 32K. This can be as small as 507 bytes for memLevel == 1. For
     * large input and output buffers, the stored block size will be larger.
     */
    unsigned min_block = min(s->pending_buf_size - 5, s->w_size);

    /* Copy as many min_block or larger stored blocks directly to next_out as
     * possible. If flushing, copy the remaining available input to next_out as
     * stored blocks, if there is enough space.
     */
    unsigned len;
    unsigned left;
    unsigned have;
    unsigned last = 0;
    unsigned used = s->strm->avail_in;
    while (true) {
        /* Set len to the maximum size block that we can copy directly with the
         * available input data and output space. Set left to how much of that
         * would be copied from what's left in the window.
         */
        len = MAX_STORED;       /* maximum deflate stored block length */
        have = (s->bi_valid + 42) >> 3;         /* number of header bytes */
        if (s->strm->avail_out < have)          /* need room for header */
            break;
            /* maximum stored block length that will fit in avail_out: */
        have = s->strm->avail_out - have;
        left = s->strstart - s->block_start;    /* bytes left in window */
        if (len > (uint32_t)left + s->strm->avail_in)
            len = left + s->strm->avail_in;     /* limit len to the input */
        if (len > have)
            len = have;                         /* limit len to the output */

        /* If the stored block would be less than min_block in length, or if
         * unable to copy all of the available input when flushing, then try
         * copying to the window and the pending buffer instead. Also don't
         * write an empty block when flushing -- deflate() does that.
         */
        if (len < min_block && ((len == 0 && flush != Z_FINISH) ||
                                flush == Z_NO_FLUSH ||
                                len != left + s->strm->avail_in))
            break;

        /* Make a dummy stored block in pending to get the header bytes,
         * including any pending bits. This also updates the debugging counts.
         */
        if (s->strm->avail_in) {
            last = flush == Z_FINISH && len == left + 1;
        } else {
            last = flush == Z_FINISH && len == left;
        }

        _tr_stored_block(s, (char *)0, 0L, last);

        /* Replace the lengths in the dummy stored block with len. */
        s->pending_buf[s->pending - 4] = len;
        s->pending_buf[s->pending - 3] = len >> 8;
        s->pending_buf[s->pending - 2] = ~len;
        s->pending_buf[s->pending - 1] = ~len >> 8;

        /* Write the stored block header bytes. */
        flush_pending(s->strm);

        /* Update debugging counts for the data about to be copied. */
        s->compressed_len += len << 3;
        s->bits_sent += len << 3;

        /* Copy uncompressed bytes from the window to next_out. */
        if (left) {
            if (left > len)
                left = len;
            memcpy(s->strm->next_out, s->window + s->block_start, left);
            s->strm->next_out += left;
            s->strm->avail_out -= left;
            s->strm->total_out += left;
            s->block_start += left;
            len -= left;
        }

        /* Copy uncompressed bytes directly from next_in to next_out, updating
         * the check value.
         */
        if (len) {
            read_buf(s->strm, s->strm->next_out, len);
            s->strm->next_out += len;
            s->strm->avail_out -= len;
            s->strm->total_out += len;
        }
        if (last != 0) {
            break;
        }
    }

    /* Update the sliding window with the last s->w_size bytes of the copied
     * data, or append all of the copied data to the existing window if less
     * than s->w_size bytes were copied. Also update the number of bytes to
     * insert in the hash tables, in the event that deflateParams() switches to
     * a non-zero compression level.
     */
    used -= s->strm->avail_in;      /* number of input bytes directly copied */
    if (used) {
        /* If any input was used, then no unused input remains in the window,
         * therefore s->block_start == s->strstart.
         */
        if (used >= s->w_size) {    /* supplant the previous history */
            s->matches = 2;         /* clear hash */
            memcpy(s->window, s->strm->next_in - s->w_size, s->w_size);
            s->strstart = s->w_size;
            s->insert = s->strstart;
        }
        else {
            if (s->window_size - s->strstart <= used) {
                /* Slide the window down. */
                s->strstart -= s->w_size;
                memcpy(s->window, s->window + s->w_size, s->strstart);
                if (s->matches < 2)
                    s->matches++;   /* add a pending slide_hash() */
                if (s->insert > s->strstart)
                    s->insert = s->strstart;
            }
            memcpy(s->window + s->strstart, s->strm->next_in - used, used);
            s->strstart += used;
            s->insert += min(used, s->w_size - s->insert);
        }
        s->block_start = s->strstart;
    }
    if (s->high_water < s->strstart)
        s->high_water = s->strstart;

    /* If the last block was written to next_out, then done. */
    if (last)
        return finish_done;

    /* If flushing and all input has been consumed, then done. */
    if (flush != Z_NO_FLUSH && flush != Z_FINISH &&
        s->strm->avail_in == 0 && (long)s->strstart == s->block_start)
        return block_done;

    /* Fill the window with any remaining input. */
    have = s->window_size - s->strstart;
    if (s->strm->avail_in > have && s->block_start >= (long)s->w_size) {
        /* Slide the window down. */
        s->block_start -= s->w_size;
        s->strstart -= s->w_size;
        memcpy(s->window, s->window + s->w_size, s->strstart);
        if (s->matches < 2)
            s->matches++;           /* add a pending slide_hash() */
        have += s->w_size;          /* more space now */
        if (s->insert > s->strstart)
            s->insert = s->strstart;
    }
    if (have > s->strm->avail_in)
        have = s->strm->avail_in;
    if (have) {
        read_buf(s->strm, s->window + s->strstart, have);
        s->strstart += have;
        s->insert += min(have, s->w_size - s->insert);
    }
    if (s->high_water < s->strstart)
        s->high_water = s->strstart;

    /* There was not enough avail_out to write a complete worthy or flushed
     * stored block to next_out. Write a stored block to pending instead, if we
     * have enough input for a worthy block, or if flushing and there is enough
     * room for the remaining input as a stored block in the pending buffer.
     */
    have = (s->bi_valid + 42) >> 3;         /* number of header bytes */
        /* maximum stored block length that will fit in pending: */
    have = min(s->pending_buf_size - have, MAX_STORED);
    min_block = min(have, s->w_size);
    left = s->strstart - s->block_start;
    if (left >= min_block ||
        ((left || flush == Z_FINISH) && flush != Z_NO_FLUSH &&
         s->strm->avail_in == 0 && left <= have)) {
        len = min(left, have);
        last = 0;
        if (flush == Z_FINISH && s->strm->avail_in == 0 && len == left) {
            last = 1;
        }
        _tr_stored_block(s, (char *)s->window + s->block_start, len, last);
        s->block_start += len;
        flush_pending(s->strm);
    }

    /* We've done all we can with the available input and output. */
    if (last) {
        return finish_started;
    }
    return need_more;
}

/* Compress as much as possible from the input stream, return the current
 * block state.
 * This function does not perform lazy evaluation of matches and inserts
 * new strings in the dictionary only for unmatched strings or for short
 * matches. It is used only for the fast compression options.
 */
int deflate_fast(deflate_state *s, int flush) {
    uint32_t hash_head;       /* head of the hash chain */
    int bflush;           /* set if current block must be flushed */

    while (true) {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need MAX_MATCH bytes
         * for the next match, plus MIN_MATCH bytes to insert the
         * string following the next match.
         */
        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
                return need_more;
            }
            if (s->lookahead == 0) break; /* flush the current block */
        }

        /* Insert the string window[strstart .. strstart + 2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        hash_head = NIL;
        if (s->lookahead >= MIN_MATCH) {
            INSERT_STRING(s, s->strstart, hash_head);
        }

        /* Find the longest match, discarding those <= prev_length.
         * At this point we have always match_length < MIN_MATCH
         */
        if (hash_head != NIL && s->strstart - hash_head <= MAX_DIST(s)) {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
            s->match_length = longest_match (s, hash_head);
            /* longest_match() sets match_start */
        }
        if (s->match_length >= MIN_MATCH) {
            check_match(s, s->strstart, s->match_start, s->match_length);

            _tr_tally_dist(s, s->strstart - s->match_start,
                           s->match_length - MIN_MATCH, bflush);

            s->lookahead -= s->match_length;

            /* Insert new strings in the hash table only if the match length
             * is not too large. This saves time but degrades compression.
             */
            if (s->match_length <= s->max_lazy_match &&
                s->lookahead >= MIN_MATCH) {
                s->match_length--; /* string at strstart already in table */
                while (true) {
                    s->strstart++;
                    INSERT_STRING(s, s->strstart, hash_head);
                    /* strstart never exceeds WSIZE-MAX_MATCH, so there are
                     * always MIN_MATCH bytes ahead.
                     */
                    bool cont = (--s->match_length != 0);
                    if (!cont) break;
                }
                s->strstart++;
            } else
            {
                s->strstart += s->match_length;
                s->match_length = 0;
                s->ins_h = s->window[s->strstart];
                UPDATE_HASH(s, s->window[s->strstart + 1]);
                /* If lookahead < MIN_MATCH, ins_h is garbage, but it does not
                 * matter since it will be recomputed at next deflate call.
                 */
            }
        } else {
            /* No match, output a literal byte */
            trace.Tracevv(stderr,"%c", s->window[s->strstart]);
            _tr_tally_lit(s, s->window[s->strstart], bflush);
            s->lookahead--;
            s->strstart++;
        }
        if (bflush) {
            int r = FLUSH_BLOCK(s, 0);
            if (r != NO_RETURN) {
                return r;
            }
        }
    }
    if (s->strstart < MIN_MATCH-1) {
        s->insert = s->strstart;
    } else {
        s->insert = MIN_MATCH-1;
    }

    if (flush == Z_FINISH) {
        int r = FLUSH_BLOCK(s, 1);
        if (r != NO_RETURN) {
            return r;
        }
        return finish_done;
    }
    if (s->sym_next) {
        int r = FLUSH_BLOCK(s, 0);
        if (r != NO_RETURN) {
                return r;
            }
    }
    return block_done;
}

/* ===========================================================================
 * Same as above, but achieves better compression. We use a lazy
 * evaluation for matches: a match is finally adopted only if there is
 * no better match at the next window position.
 */
int deflate_slow(deflate_state *s, int flush) {
    uint32_t hash_head;          /* head of hash chain */
    int bflush;              /* set if current block must be flushed */

    /* Process the input block. */
    while (true) {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need MAX_MATCH bytes
         * for the next match, plus MIN_MATCH bytes to insert the
         * string following the next match.
         */
        if (s->lookahead < MIN_LOOKAHEAD) {
            fill_window(s);
            if (s->lookahead < MIN_LOOKAHEAD && flush == Z_NO_FLUSH) {
                return need_more;
            }
            if (s->lookahead == 0) break; /* flush the current block */
        }

        /* Insert the string window[strstart .. strstart + 2] in the
         * dictionary, and set hash_head to the head of the hash chain:
         */
        hash_head = NIL;
        if (s->lookahead >= MIN_MATCH) {
            INSERT_STRING(s, s->strstart, hash_head);
        }

        /* Find the longest match, discarding those <= prev_length.
         */
        s->prev_length = s->match_length;
        s->prev_match = s->match_start;
        s->match_length = MIN_MATCH-1;

        if (hash_head != NIL && s->prev_length < s->max_lazy_match &&
            s->strstart - hash_head <= MAX_DIST(s)) {
            /* To simplify the code, we prevent matches with the string
             * of window index 0 (in particular we have to avoid a match
             * of the string with itself at the start of the input file).
             */
            s->match_length = longest_match (s, hash_head);
            /* longest_match() sets match_start */

            if (s->match_length <= 5 && (s->strategy == Z_FILTERED
                || (s->match_length == MIN_MATCH &&
                    s->strstart - s->match_start > TOO_FAR)
                )) {

                /* If prev_match is also MIN_MATCH, match_start is garbage
                 * but we will ignore the current match anyway.
                 */
                s->match_length = MIN_MATCH-1;
            }
        }
        /* If there was a match at the previous step and the current
         * match is not better, output the previous match:
         */
        if (s->prev_length >= MIN_MATCH && s->match_length <= s->prev_length) {
            uint32_t max_insert = s->strstart + s->lookahead - MIN_MATCH;
            /* Do not insert strings in hash table beyond this. */

            check_match(s, s->strstart - 1, s->prev_match, s->prev_length);

            _tr_tally_dist(s, s->strstart - 1 - s->prev_match,
                           s->prev_length - MIN_MATCH, bflush);

            /* Insert in hash table all strings up to the end of the match.
             * strstart - 1 and strstart are already inserted. If there is not
             * enough lookahead, the last two strings are not inserted in
             * the hash table.
             */
            s->lookahead -= s->prev_length - 1;
            s->prev_length -= 2;
            while (true) {
                if (++s->strstart <= max_insert) {
                    INSERT_STRING(s, s->strstart, hash_head);
                }
                bool cont = (--s->prev_length != 0);
                if (!cont) break;
            }
            s->match_available = 0;
            s->match_length = MIN_MATCH-1;
            s->strstart++;

            if (bflush) {
                int r = FLUSH_BLOCK(s, 0);
                if (r != NO_RETURN) {
                    return r;
                }
            }

        } else if (s->match_available) {
            /* If there was no match at the previous position, output a
             * single literal. If there was a match but the current match
             * is longer, truncate the previous match to a single literal.
             */
            trace.Tracevv(stderr,"%c", s->window[s->strstart - 1]);
            _tr_tally_lit(s, s->window[s->strstart - 1], bflush);
            if (bflush) {
                FLUSH_BLOCK_ONLY(s, 0);
            }
            s->strstart++;
            s->lookahead--;
            if (s->strm->avail_out == 0) return need_more;
        } else {
            /* There is no previous match to compare with, wait for
             * the next step to decide.
             */
            s->match_available = 1;
            s->strstart++;
            s->lookahead--;
        }
    }
    assert.Assert(flush != Z_NO_FLUSH, "no flush");
    if (s->match_available) {
        trace.Tracevv(stderr,"%c", s->window[s->strstart - 1]);
        _tr_tally_lit(s, s->window[s->strstart - 1], bflush);
        s->match_available = 0;
    }
    if (s->strstart < MIN_MATCH-1) {
        s->insert = s->strstart;
    } else {
        s->insert = MIN_MATCH-1;
    }
    if (flush == Z_FINISH) {
        int r = FLUSH_BLOCK(s, 1);
        if (r != NO_RETURN) {
                return r;
            }
        return finish_done;
    }
    if (s->sym_next) {
        int r = FLUSH_BLOCK(s, 0);
        if (r != NO_RETURN) {
                return r;
            }
    }
    return block_done;
}

/* ===========================================================================
 * Insert string str in the dictionary and set match_head to the previous head
 * of the hash chain (the most recent string with same hash key). Return
 * the previous length of the hash chain.
 * If this file is compiled with -DFASTEST, the compression level is forced
 * to 1, and no hash chains are maintained.
 * IN  assertion: all calls to INSERT_STRING are made with consecutive input
 *    characters and the first MIN_MATCH bytes of str are valid (except for
 *    the last MIN_MATCH-1 bytes of the input file).
 */
void INSERT_STRING(deflate_state *s, uint32_t str, uint32_t match_head) {
    UPDATE_HASH(s, s->window[(str) + (MIN_MATCH-1)]);
    *match_head = s->prev[(str) & s->w_mask] = s->head[s->ins_h];
    s->head[s->ins_h] = (uint16_t)(str);
}

/* ===========================================================================
 * For Z_RLE, simply look for runs of bytes, generate matches only of distance
 * one.  Do not maintain a hash table.  (It will be regenerated if this run of
 * deflate switches away from Z_RLE.)
 */
int deflate_rle(deflate_state *s, int flush) {
    int bflush;             /* set if current block must be flushed */
    uint32_t prev;              /* byte at distance one to match */
    uint8_t *scan;
    uint8_t *strend;   /* scan goes up to strend for length of run */

    while (true) {
        /* Make sure that we always have enough lookahead, except
         * at the end of the input file. We need MAX_MATCH bytes
         * for the longest run, plus one for the unrolled loop.
         */
        if (s->lookahead <= MAX_MATCH) {
            fill_window(s);
            if (s->lookahead <= MAX_MATCH && flush == Z_NO_FLUSH) {
                return need_more;
            }
            if (s->lookahead == 0) break; /* flush the current block */
        }

        /* See how many times the previous byte repeats */
        s->match_length = 0;
        if (s->lookahead >= MIN_MATCH && s->strstart > 0) {
            scan = s->window + s->strstart - 1;
            prev = *scan;
            if (prev == *++scan && prev == *++scan && prev == *++scan) {
                strend = s->window + s->strstart + MAX_MATCH;
                while (true) {
                    bool cont = (prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         prev == *++scan && prev == *++scan &&
                         scan < strend);
                    if (!cont) break;
                }
                s->match_length = MAX_MATCH - (uint32_t)(strend - scan);
                if (s->match_length > s->lookahead)
                    s->match_length = s->lookahead;
            }
            assert.Assert(scan <= s->window + (uint32_t)(s->window_size - 1),
                   "wild scan");
        }

        /* Emit match if have run of MIN_MATCH or longer, else emit literal */
        if (s->match_length >= MIN_MATCH) {
            check_match(s, s->strstart, s->strstart - 1, s->match_length);

            _tr_tally_dist(s, 1, s->match_length - MIN_MATCH, bflush);

            s->lookahead -= s->match_length;
            s->strstart += s->match_length;
            s->match_length = 0;
        } else {
            /* No match, output a literal byte */
            trace.Tracevv(stderr,"%c", s->window[s->strstart]);
            _tr_tally_lit(s, s->window[s->strstart], bflush);
            s->lookahead--;
            s->strstart++;
        }
        if (bflush) {
            int r = FLUSH_BLOCK(s, 0);
            if (r != NO_RETURN) {
                return r;
            }
        }
    }
    s->insert = 0;
    if (flush == Z_FINISH) {
        int r = FLUSH_BLOCK(s, 1);
        if (r != NO_RETURN) {
                return r;
            }
        return finish_done;
    }
    if (s->sym_next) {
        int r = FLUSH_BLOCK(s, 0);
        if (r != NO_RETURN) {
                return r;
            }
    }
        
    return block_done;
}

/* ===========================================================================
 * For Z_HUFFMAN_ONLY, do not look for matches.  Do not maintain a hash table.
 * (It will be regenerated if this run of deflate switches away from Huffman.)
 */
int deflate_huff(deflate_state *s, int flush) {
    int bflush;             /* set if current block must be flushed */

    while (true) {
        /* Make sure that we have a literal to write. */
        if (s->lookahead == 0) {
            fill_window(s);
            if (s->lookahead == 0) {
                if (flush == Z_NO_FLUSH)
                    return need_more;
                break;      /* flush the current block */
            }
        }

        /* Output a literal byte */
        s->match_length = 0;
        trace.Tracevv(stderr,"%c", s->window[s->strstart]);
        _tr_tally_lit(s, s->window[s->strstart], bflush);
        s->lookahead--;
        s->strstart++;
        if (bflush) {
            int r = FLUSH_BLOCK(s, 0);
            if (r != NO_RETURN) {
                return r;
            }
        }
    }
    s->insert = 0;
    if (flush == Z_FINISH) {
        int r = FLUSH_BLOCK(s, 1);
        if (r != NO_RETURN) {
                return r;
            }
        return finish_done;
    }
    if (s->sym_next) {
        int r = FLUSH_BLOCK(s, 0);
        if (r != NO_RETURN) {
                return r;
            }
    }
    return block_done;
}

/* To be used only when the state is known to be valid */
int ERR_RETURN(z_stream *strm, int err) {
    strm->msg = ERR_MSG(err);
    return err;
}

const char *ERR_MSG(int err) {
    return z_errmsg[Z_NEED_DICT-(err)];
}

char *z_errmsg[10] = {
    "need dictionary",     /* Z_NEED_DICT       2  */
    "stream end",          /* Z_STREAM_END      1  */
    "",                    /* Z_OK              0  */
    "file error",          /* Z_ERRNO         (-1) */
    "stream error",        /* Z_STREAM_ERROR  (-2) */
    "data error",          /* Z_DATA_ERROR    (-3) */
    "insufficient memory", /* Z_MEM_ERROR     (-4) */
    "buffer error",        /* Z_BUF_ERROR     (-5) */
    "incompatible version",/* Z_VERSION_ERROR (-6) */
    ""
};




// --------------------- trees ---------------------

/* trees.c -- output deflated data using Huffman coding
 * Copyright (C) 1995-2021 Jean-loup Gailly
 * detect_data_type() function provided freely by Cosmin Truta, 2006
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/*
 *  ALGORITHM
The more common source values are represented by shorter bit sequences.

The "deflation" process uses several Huffman trees ("code trees").
Each tree is stored in a compressed form which is itself a Huffman encoding of the lengths of all the code strings (in
 * ascending order by source values).
The actual code strings are reconstructed from the lengths in the inflate process, as described in the deflate specification.
 *
 *  REFERENCES
 *
 *      Deutsch, L.P.,"'Deflate' Compressed Data Format Specification".
 *      Available in ftp.uu.net:/pub/archiving/zip/doc/deflate-1.1.doc
 *
 *      Storer, James A.
 *          Data Compression:  Methods and Theory, pp. 49-50.
 *          Computer Science Press, 1988.  ISBN 0-7167-8156-5.
 *
 *      Sedgewick, R.
 *          Algorithms, p290.
 *          Addison-Wesley, 1983. ISBN 0-201-06672-6.
 */

/* @(#) $Id$ */



#define MAX_BL_BITS 7
/* Bit length codes must not exceed MAX_BL_BITS bits */

#define END_BLOCK 256
/* end of block literal code */

#define REP_3_6      16
/* repeat previous bit length 3-6 times (2 bits of repeat count) */

#define REPZ_3_10    17
/* repeat a zero length 3-10 times  (3 bits of repeat count) */

#define REPZ_11_138  18
/* repeat a zero length 11-138 times  (7 bits of repeat count) */


/* extra bits for each length code */
int extra_lbits[LENGTH_CODES] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};

/* extra bits for each distance code */
int extra_dbits[D_CODES] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

/* extra bits for each bit length code */
int extra_blbits[BL_CODES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7};

/* The lengths of the bit length codes are sent in order of decreasing
 * probability, to avoid transmitting the lengths for unused bit length codes.
 */
uint8_t bl_order[BL_CODES] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

#define DIST_CODE_LEN  512 /* see definition of array dist_code below */

bool GEN_TREES_H = false;

/* The static literal tree. Since the bit lengths are imposed, there is no
 * need for the L_CODES extra codes used during heap construction. However
 * The codes 286 and 287 are needed to build a canonical tree (see _tr_init
 * below).
 */
ct_data static_ltree[L_CODES+2] = {
{{ 12},{  8}}, {{140},{  8}}, {{ 76},{  8}}, {{204},{  8}}, {{ 44},{  8}},
{{172},{  8}}, {{108},{  8}}, {{236},{  8}}, {{ 28},{  8}}, {{156},{  8}},
{{ 92},{  8}}, {{220},{  8}}, {{ 60},{  8}}, {{188},{  8}}, {{124},{  8}},
{{252},{  8}}, {{  2},{  8}}, {{130},{  8}}, {{ 66},{  8}}, {{194},{  8}},
{{ 34},{  8}}, {{162},{  8}}, {{ 98},{  8}}, {{226},{  8}}, {{ 18},{  8}},
{{146},{  8}}, {{ 82},{  8}}, {{210},{  8}}, {{ 50},{  8}}, {{178},{  8}},
{{114},{  8}}, {{242},{  8}}, {{ 10},{  8}}, {{138},{  8}}, {{ 74},{  8}},
{{202},{  8}}, {{ 42},{  8}}, {{170},{  8}}, {{106},{  8}}, {{234},{  8}},
{{ 26},{  8}}, {{154},{  8}}, {{ 90},{  8}}, {{218},{  8}}, {{ 58},{  8}},
{{186},{  8}}, {{122},{  8}}, {{250},{  8}}, {{  6},{  8}}, {{134},{  8}},
{{ 70},{  8}}, {{198},{  8}}, {{ 38},{  8}}, {{166},{  8}}, {{102},{  8}},
{{230},{  8}}, {{ 22},{  8}}, {{150},{  8}}, {{ 86},{  8}}, {{214},{  8}},
{{ 54},{  8}}, {{182},{  8}}, {{118},{  8}}, {{246},{  8}}, {{ 14},{  8}},
{{142},{  8}}, {{ 78},{  8}}, {{206},{  8}}, {{ 46},{  8}}, {{174},{  8}},
{{110},{  8}}, {{238},{  8}}, {{ 30},{  8}}, {{158},{  8}}, {{ 94},{  8}},
{{222},{  8}}, {{ 62},{  8}}, {{190},{  8}}, {{126},{  8}}, {{254},{  8}},
{{  1},{  8}}, {{129},{  8}}, {{ 65},{  8}}, {{193},{  8}}, {{ 33},{  8}},
{{161},{  8}}, {{ 97},{  8}}, {{225},{  8}}, {{ 17},{  8}}, {{145},{  8}},
{{ 81},{  8}}, {{209},{  8}}, {{ 49},{  8}}, {{177},{  8}}, {{113},{  8}},
{{241},{  8}}, {{  9},{  8}}, {{137},{  8}}, {{ 73},{  8}}, {{201},{  8}},
{{ 41},{  8}}, {{169},{  8}}, {{105},{  8}}, {{233},{  8}}, {{ 25},{  8}},
{{153},{  8}}, {{ 89},{  8}}, {{217},{  8}}, {{ 57},{  8}}, {{185},{  8}},
{{121},{  8}}, {{249},{  8}}, {{  5},{  8}}, {{133},{  8}}, {{ 69},{  8}},
{{197},{  8}}, {{ 37},{  8}}, {{165},{  8}}, {{101},{  8}}, {{229},{  8}},
{{ 21},{  8}}, {{149},{  8}}, {{ 85},{  8}}, {{213},{  8}}, {{ 53},{  8}},
{{181},{  8}}, {{117},{  8}}, {{245},{  8}}, {{ 13},{  8}}, {{141},{  8}},
{{ 77},{  8}}, {{205},{  8}}, {{ 45},{  8}}, {{173},{  8}}, {{109},{  8}},
{{237},{  8}}, {{ 29},{  8}}, {{157},{  8}}, {{ 93},{  8}}, {{221},{  8}},
{{ 61},{  8}}, {{189},{  8}}, {{125},{  8}}, {{253},{  8}}, {{ 19},{  9}},
{{275},{  9}}, {{147},{  9}}, {{403},{  9}}, {{ 83},{  9}}, {{339},{  9}},
{{211},{  9}}, {{467},{  9}}, {{ 51},{  9}}, {{307},{  9}}, {{179},{  9}},
{{435},{  9}}, {{115},{  9}}, {{371},{  9}}, {{243},{  9}}, {{499},{  9}},
{{ 11},{  9}}, {{267},{  9}}, {{139},{  9}}, {{395},{  9}}, {{ 75},{  9}},
{{331},{  9}}, {{203},{  9}}, {{459},{  9}}, {{ 43},{  9}}, {{299},{  9}},
{{171},{  9}}, {{427},{  9}}, {{107},{  9}}, {{363},{  9}}, {{235},{  9}},
{{491},{  9}}, {{ 27},{  9}}, {{283},{  9}}, {{155},{  9}}, {{411},{  9}},
{{ 91},{  9}}, {{347},{  9}}, {{219},{  9}}, {{475},{  9}}, {{ 59},{  9}},
{{315},{  9}}, {{187},{  9}}, {{443},{  9}}, {{123},{  9}}, {{379},{  9}},
{{251},{  9}}, {{507},{  9}}, {{  7},{  9}}, {{263},{  9}}, {{135},{  9}},
{{391},{  9}}, {{ 71},{  9}}, {{327},{  9}}, {{199},{  9}}, {{455},{  9}},
{{ 39},{  9}}, {{295},{  9}}, {{167},{  9}}, {{423},{  9}}, {{103},{  9}},
{{359},{  9}}, {{231},{  9}}, {{487},{  9}}, {{ 23},{  9}}, {{279},{  9}},
{{151},{  9}}, {{407},{  9}}, {{ 87},{  9}}, {{343},{  9}}, {{215},{  9}},
{{471},{  9}}, {{ 55},{  9}}, {{311},{  9}}, {{183},{  9}}, {{439},{  9}},
{{119},{  9}}, {{375},{  9}}, {{247},{  9}}, {{503},{  9}}, {{ 15},{  9}},
{{271},{  9}}, {{143},{  9}}, {{399},{  9}}, {{ 79},{  9}}, {{335},{  9}},
{{207},{  9}}, {{463},{  9}}, {{ 47},{  9}}, {{303},{  9}}, {{175},{  9}},
{{431},{  9}}, {{111},{  9}}, {{367},{  9}}, {{239},{  9}}, {{495},{  9}},
{{ 31},{  9}}, {{287},{  9}}, {{159},{  9}}, {{415},{  9}}, {{ 95},{  9}},
{{351},{  9}}, {{223},{  9}}, {{479},{  9}}, {{ 63},{  9}}, {{319},{  9}},
{{191},{  9}}, {{447},{  9}}, {{127},{  9}}, {{383},{  9}}, {{255},{  9}},
{{511},{  9}}, {{  0},{  7}}, {{ 64},{  7}}, {{ 32},{  7}}, {{ 96},{  7}},
{{ 16},{  7}}, {{ 80},{  7}}, {{ 48},{  7}}, {{112},{  7}}, {{  8},{  7}},
{{ 72},{  7}}, {{ 40},{  7}}, {{104},{  7}}, {{ 24},{  7}}, {{ 88},{  7}},
{{ 56},{  7}}, {{120},{  7}}, {{  4},{  7}}, {{ 68},{  7}}, {{ 36},{  7}},
{{100},{  7}}, {{ 20},{  7}}, {{ 84},{  7}}, {{ 52},{  7}}, {{116},{  7}},
{{  3},{  8}}, {{131},{  8}}, {{ 67},{  8}}, {{195},{  8}}, {{ 35},{  8}},
{{163},{  8}}, {{ 99},{  8}}, {{227},{  8}}
};

/* The static distance tree. (Actually a trivial tree since all codes use
 * 5 bits.)
 */
ct_data static_dtree[D_CODES] = {
{{ 0},{ 5}}, {{16},{ 5}}, {{ 8},{ 5}}, {{24},{ 5}}, {{ 4},{ 5}},
{{20},{ 5}}, {{12},{ 5}}, {{28},{ 5}}, {{ 2},{ 5}}, {{18},{ 5}},
{{10},{ 5}}, {{26},{ 5}}, {{ 6},{ 5}}, {{22},{ 5}}, {{14},{ 5}},
{{30},{ 5}}, {{ 1},{ 5}}, {{17},{ 5}}, {{ 9},{ 5}}, {{25},{ 5}},
{{ 5},{ 5}}, {{21},{ 5}}, {{13},{ 5}}, {{29},{ 5}}, {{ 3},{ 5}},
{{19},{ 5}}, {{11},{ 5}}, {{27},{ 5}}, {{ 7},{ 5}}, {{23},{ 5}}
};

/* Distance codes. The first 256 values correspond to the distances
 * 3 .. 258, the last 256 values correspond to the top 8 bits of
 * the 15 bit distances.
 */
uint8_t  _dist_code[DIST_CODE_LEN] = {
 0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8,
 8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 10,
10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13,
13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15,
15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,  0,  0, 16, 17,
18, 18, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22,
23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27,
27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29
};

/* length code for each normalized match length (0 == MIN_MATCH) */
uint8_t  _length_code[MAX_MATCH-MIN_MATCH+1]= {
 0,  1,  2,  3,  4,  5,  6,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 12, 12,
13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16,
17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19,
19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22,
22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23,
23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26,
26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28
};

/* First normalized length for each code (0 = MIN_MATCH) */
int base_length[LENGTH_CODES] = {
0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56,
64, 80, 96, 112, 128, 160, 192, 224, 0
};

/* First normalized distance for each code (0 = distance of 1) */
int base_dist[D_CODES] = {
    0,     1,     2,     3,     4,     6,     8,    12,    16,    24,
   32,    48,    64,    96,   128,   192,   256,   384,   512,   768,
 1024,  1536,  2048,  3072,  4096,  6144,  8192, 12288, 16384, 24576
};

/* Mapping from a distance to a distance code. dist is the distance - 1 and
 * must not have side effects. _dist_code[256] and _dist_code[257] are never
 * used.
 */
int d_code(int dist) {
    if (dist < 256) {
        return _dist_code[dist];
    } else {
        return _dist_code[256+((dist)>>7)];
    }
}

const static_tree_desc static_l_desc = {static_ltree, extra_lbits, LITERALS+1, L_CODES, MAX_BITS};
const static_tree_desc static_d_desc = {static_dtree, extra_dbits, 0,          D_CODES, MAX_BITS};
const static_tree_desc static_bl_desc = {NULL, extra_blbits, 0,   BL_CODES, MAX_BL_BITS};

void send_code(deflate_state *s, int c, void *tree) {
    trace.Tracevv(stderr, "\ncd %3d ",c);
    send_bits(s, tree[c].fc.code, tree[c].dl.len);
}

/* ===========================================================================
 * Output a short LSB first on the stream.
 * IN assertion: there is enough room in pendingBuf.
 */
void put_short(deflate_state *s, uint16_t w) {
    put_byte(s, (uint8_t)((w) & 0xff));
    put_byte(s, (uint8_t)((uint16_t)(w) >> 8));
}

/* ===========================================================================
 * Send a value on a given number of bits.
 * IN assertion: length <= 16 and value fits in length bits.
 */
void send_bits(deflate_state *s, int value, int length) {
    trace.Tracevv(stderr," l %2d v %4x ", length, value);
    assert.Assert(length > 0 && length <= 15, "invalid length");
    s->bits_sent += (uint32_t)length;

    /* If not enough room in bi_buf, use (valid) bits from bi_buf and
     * (16 - bi_valid) bits from value, leaving (width - (16 - bi_valid))
     * unused bits in value.
     */
    if (s->bi_valid > (int)Buf_size - length) {
        s->bi_buf |= (uint16_t)value << s->bi_valid;
        put_short(s, s->bi_buf);
        s->bi_buf = (uint16_t)value >> (Buf_size - s->bi_valid);
        s->bi_valid += length - Buf_size;
    } else {
        s->bi_buf |= (uint16_t)value << s->bi_valid;
        s->bi_valid += length;
    }
}


/* ===========================================================================
 * Initialize the various 'constant' tables.
 */
bool static_init_done = false;
void tr_static_init() {
    if (static_init_done) return;

    int n;        /* iterates over tree elements */
    int bits;     /* bit counter */
    int length;   /* length value */
    int code;     /* code value */
    int dist;     /* distance index */
    uint16_t bl_count[MAX_BITS+1];
    /* number of codes at each bit length for an optimal tree */

    /* Initialize the mapping length (0..255) -> length code (0..28) */
    length = 0;
    for (int code = 0; code < LENGTH_CODES-1; code++) {
        base_length[code] = length;
        for (n = 0; n < (1 << extra_lbits[code]); n++) {
            _length_code[length++] = (uint8_t)code;
        }
    }
    assert.Assert(length == 256, "tr_static_init: length != 256");
    /* Note that the length 255 (match length 258) can be represented
     * in two different ways: code 284 + 5 bits or code 285, so we
     * overwrite length_code[255] to use the best encoding:
     */
    _length_code[length - 1] = (uint8_t)code;

    /* Initialize the mapping dist (0..32K) -> dist code (0..29) */
    dist = 0;
    for (int code = 0 ; code < 16; code++) {
        base_dist[code] = dist;
        for (n = 0; n < (1 << extra_dbits[code]); n++) {
            _dist_code[dist++] = (uint8_t)code;
        }
    }
    assert.Assert(dist == 256, "tr_static_init: dist != 256");
    dist >>= 7; /* from now on, all distances are divided by 128 */
    for ( ; code < D_CODES; code++) {
        base_dist[code] = dist << 7;
        for (n = 0; n < (1 << (extra_dbits[code] - 7)); n++) {
            _dist_code[256 + dist++] = (uint8_t)code;
        }
    }
    assert.Assert(dist == 256, "tr_static_init: 256 + dist != 512");

    /* Construct the codes of the static literal tree */
    for (bits = 0; bits <= MAX_BITS; bits++) {
        bl_count[bits] = 0;
    }
    n = 0;
    while (n <= 143) {
        static_ltree[n++].dl.len = 8;
        bl_count[8]++;
    }
    while (n <= 255) {
        static_ltree[n++].dl.len = 9;
        bl_count[9]++;
    }
    while (n <= 279) {
        static_ltree[n++].dl.len = 7;
        bl_count[7]++;
    }
    while (n <= 287) {
        static_ltree[n++].dl.len = 8;
        bl_count[8]++;
    }
    /* Codes 286 and 287 do not exist, but we must include them in the
     * tree construction to get a canonical Huffman tree (longest code
     * all ones)
     */
    gen_codes(static_ltree, L_CODES+1, bl_count);

    /* The static distance tree is trivial: */
    for (n = 0; n < D_CODES; n++) {
        static_dtree[n].dl.len = 5;
        static_dtree[n].fc.code = bi_reverse((unsigned)n, 5);
    }
    static_init_done = 1;

    if (GEN_TREES_H) {
        gen_trees_header();
    }
}

const char *SEPARATOR(int i, last, width) {
    if (i == last) {
        return "\n};\n\n";
    }
    if (i % width == width - 1) {
        return ",\n";
    }
    return ", ";
}

void gen_trees_header()
{
    FILE *header = fopen("trees.h", "w");
    int i;

    assert.Assert(header != NULL, "Can't open trees.h");
    fprintf(header,
            "/* header created automatically with -DGEN_TREES_H */\n\n");

    fprintf(header, "const ct_data static_ltree[L_CODES+2] = {\n");
    for (i = 0; i < L_CODES+2; i++) {
        fprintf(header, "{{%3u},{%3u}}%s", static_ltree[i].fc.code,
                static_ltree[i].dl.len, SEPARATOR(i, L_CODES+1, 5));
    }

    fprintf(header, "const ct_data static_dtree[D_CODES] = {\n");
    for (i = 0; i < D_CODES; i++) {
        fprintf(header, "{{%2u},{%2u}}%s", static_dtree[i].fc.code,
                static_dtree[i].dl.len, SEPARATOR(i, D_CODES-1, 5));
    }

    fprintf(header, "const uint8_t  _dist_code[DIST_CODE_LEN] = {\n");
    for (i = 0; i < DIST_CODE_LEN; i++) {
        fprintf(header, "%2u%s", _dist_code[i],
                SEPARATOR(i, DIST_CODE_LEN-1, 20));
    }

    fprintf(header,
        "const uint8_t  _length_code[MAX_MATCH-MIN_MATCH+1]= {\n");
    for (i = 0; i < MAX_MATCH-MIN_MATCH+1; i++) {
        fprintf(header, "%2u%s", _length_code[i],
                SEPARATOR(i, MAX_MATCH-MIN_MATCH, 20));
    }

    fprintf(header, "const int base_length[LENGTH_CODES] = {\n");
    for (i = 0; i < LENGTH_CODES; i++) {
        fprintf(header, "%1u%s", base_length[i],
                SEPARATOR(i, LENGTH_CODES-1, 20));
    }

    fprintf(header, "const int base_dist[D_CODES] = {\n");
    for (i = 0; i < D_CODES; i++) {
        fprintf(header, "%5u%s", base_dist[i],
                SEPARATOR(i, D_CODES-1, 10));
    }

    fclose(header);
}

/* ===========================================================================
 * Initialize the tree data structures for a new zlib stream.
 */
void _tr_init(deflate_state *s) {
    tr_static_init();

    s->l_desc.dyn_tree = s->dyn_ltree;
    s->l_desc.stat_desc = &static_l_desc;

    s->d_desc.dyn_tree = s->dyn_dtree;
    s->d_desc.stat_desc = &static_d_desc;

    s->bl_desc.dyn_tree = s->bl_tree;
    s->bl_desc.stat_desc = &static_bl_desc;

    s->bi_buf = 0;
    s->bi_valid = 0;
    s->compressed_len = 0L;
    s->bits_sent = 0L;

    /* Initialize the first block of the first file: */
    init_block(s);
}

/* ===========================================================================
 * Initialize a new block.
 */
void init_block(deflate_state *s) {
    /* Initialize the trees. */
    for (int n = 0; n < L_CODES;  n++) s->dyn_ltree[n].fc.freq = 0;
    for (int n = 0; n < D_CODES;  n++) s->dyn_dtree[n].fc.freq = 0;
    for (int n = 0; n < BL_CODES; n++) s->bl_tree[n].fc.freq = 0;
    s->dyn_ltree[END_BLOCK].fc.freq = 1;
    s->opt_len = s->static_len = 0L;
    s->sym_next = s->matches = 0;
}

#define SMALLEST 1
/* Index within the heap array of least frequent node in the Huffman tree */




/* ===========================================================================
 * Compares to subtrees, using the tree depth as tie breaker when
 * the subtrees have equal frequency. This minimizes the worst case length.
 */
bool smaller(ct_data tree, int n, m, depth) {
   return (
        tree[n].fc.freq < tree[m].fc.freq
        || (tree[n].fc.freq == tree[m].fc.freq && depth[n] <= depth[m])
    );
}

/* ===========================================================================
 * Restore the heap property by moving down the tree starting at node k,
 * exchanging a node with the smallest of its two sons if necessary, stopping
 * when the heap property is re-established (each father smaller than its
 * two sons). */
 /* tree is the tree to restore */
 /* k is the node to move down */
void pqdownheap(deflate_state *s, ct_data *tree, int k) {
    int v = s->heap[k];
    int j = k << 1;  /* left son of k */
    while (j <= s->heap_len) {
        /* Set j to the smallest of the two sons: */
        if (j < s->heap_len &&
            smaller(tree, s->heap[j + 1], s->heap[j], s->depth)) {
            j++;
        }
        /* Exit if v is smaller than both sons */
        if (smaller(tree, v, s->heap[j], s->depth)) break;

        /* Exchange v with the smallest son */
        s->heap[k] = s->heap[j];  k = j;

        /* And continue down the tree, setting j to the left son of k */
        j <<= 1;
    }
    s->heap[k] = v;
}

/* ===========================================================================
 * Compute the optimal bit lengths for a tree and update the total bit length
 * for the current block.
 * IN assertion: the fields freq and dad are set, heap[heap_max] and
 *    above are the tree nodes sorted by increasing frequency.
 * OUT assertions: the field len is set to the optimal bit length, the
 *     array bl_count contains the frequencies for each bit length.
 *     The length opt_len is updated; static_len is also updated if stree is
 *     not null.
 */
 /* desc is the tree descriptor */
void gen_bitlen(deflate_state *s, tree_desc *desc) {
    ct_data *tree        = desc->dyn_tree;
    int max_code         = desc->max_code;
    const ct_data *stree = desc->stat_desc->static_tree;
    const int *extra    = desc->stat_desc->extra_bits;
    int base             = desc->stat_desc->extra_base;
    int max_length       = desc->stat_desc->max_length;
    int h;              /* heap index */
    int n;
    int m;           /* iterate over the tree elements */
    int bits;           /* bit length */
    int xbits;          /* extra bits */
    uint16_t f;              /* frequency */
    int overflow = 0;   /* number of elements with bit length too large */

    for (bits = 0; bits <= MAX_BITS; bits++) s->bl_count[bits] = 0;

    /* In a first pass, compute the optimal bit lengths (which may
     * overflow in the case of the bit length tree).
     */
    tree[s->heap[s->heap_max]].dl.len = 0; /* root of the heap */

    for (h = s->heap_max + 1; h < HEAP_SIZE; h++) {
        n = s->heap[h];
        bits = tree[tree[n].dl.dad].dl.len + 1;
        if (bits > max_length) {
            bits = max_length;
            overflow++;
        }
        tree[n].dl.len = (uint16_t)bits;
        /* We overwrite tree[n].Dad which is no longer needed */

        if (n > max_code) continue; /* not a leaf node */

        s->bl_count[bits]++;
        xbits = 0;
        if (n >= base) xbits = extra[n - base];
        f = tree[n].fc.freq;
        s->opt_len += (uint32_t)f * (unsigned)(bits + xbits);
        if (stree) s->static_len += (uint32_t)f * (unsigned)(stree[n].dl.len + xbits);
    }
    if (overflow == 0) return;

    trace.Tracev(stderr,"\nbit length overflow\n");
    /* This happens for example on obj2 and pic of the Calgary corpus */

    /* Find the first bit length which could increase: */
    while (true) {
        bits = max_length - 1;
        while (s->bl_count[bits] == 0) bits--;
        s->bl_count[bits]--;        /* move one leaf down the tree */
        s->bl_count[bits + 1] += 2; /* move one overflow item as its brother */
        s->bl_count[max_length]--;
        /* The brother of the overflow item also moves one step up,
         * but this does not affect bl_count[max_length]
         */
        overflow -= 2;
        if (overflow <= 0) break;
    }

    /* Now recompute all bit lengths, scanning in increasing frequency.
     * h is still equal to HEAP_SIZE. (It is simpler to reconstruct all
     * lengths instead of fixing only the wrong ones. This idea is taken
     * from 'ar' written by Haruhiko Okumura.)
     */
    for (bits = max_length; bits != 0; bits--) {
        n = s->bl_count[bits];
        while (n != 0) {
            m = s->heap[--h];
            if (m > max_code) continue;
            if ((unsigned) tree[m].dl.len != (unsigned) bits) {
                trace.Tracev(stderr,"code %d bits %d->%d\n", m, tree[m].dl.len, bits);
                s->opt_len += ((uint32_t)bits - tree[m].dl.len) * tree[m].fc.freq;
                tree[m].dl.len = (uint16_t)bits;
            }
            n--;
        }
    }
}

/* ===========================================================================
 * Generate the codes for a given tree and bit counts (which need not be
 * optimal).
 * IN assertion: the array bl_count contains the bit length statistics for
 * the given tree and the field len is set for all tree elements.
 * OUT assertion: the field code is set for all tree elements of non
 *     zero code length.
 */
//  ct_data *tree;             /* the tree to decorate */
//     int max_code;              /* largest code with non zero frequency */
//     *bl_count;            /* number of codes at each bit length */
void gen_codes(
    ct_data *tree,
    int max_code,
    uint16_t *bl_count
) {
    uint16_t next_code[MAX_BITS+1]; /* next code value for each bit length */
    unsigned code = 0;         /* running code value */
    int bits;                  /* bit index */
    int n;                     /* code index */

    /* The distribution counts are first used to generate the code values
     * without bit reversal.
     */
    for (bits = 1; bits <= MAX_BITS; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = (uint16_t)code;
    }
    /* Check that the bit counts in bl_count are consistent. The last code
     * must be all ones.
     */
    assert.Assert(code + bl_count[MAX_BITS] - 1 == (1 << MAX_BITS) - 1, "inconsistent bit counts");
    trace.Tracev(stderr,"\ngen_codes: max_code %d ", max_code);

    for (n = 0;  n <= max_code; n++) {
        int len = tree[n].dl.len;
        if (len == 0) continue;
        /* Now reverse the bits */
        tree[n].fc.code = (uint16_t)bi_reverse(next_code[len]++, len);

        if (tree != static_ltree) {
            int c = ' ';
            if (isgraph(n)) {
                c = n;
            }
            trace.Tracevv(stderr, "\nn %3d %c l %2d c %4x (%x) ", n, c, len, tree[n].fc.code, next_code[len] - 1);
        }
    }
}

/* ===========================================================================
 * Construct one Huffman tree and assigns the code bit strings and lengths.
 * Update the total bit length for the current block.
 * IN assertion: the field freq is set for all tree elements.
 * OUT assertions: the fields len and code are set to the optimal bit length
 *     and corresponding code. The length opt_len is updated; static_len is
 *     also updated if stree is not null. The field max_code is set.
 */
void build_tree(deflate_state *s, tree_desc *desc) {
    ct_data *tree         = desc->dyn_tree;
    ct_data *stree  = desc->stat_desc->static_tree;
    int elems             = desc->stat_desc->elems;
    int n;
    int m;          /* iterate over heap elements */
    int max_code = -1; /* largest code with non zero frequency */
    int node;          /* new node being created */

    /* Construct the initial heap, with least frequent element in
     * heap[SMALLEST]. The sons of heap[n] are heap[2*n] and heap[2*n + 1].
     * heap[0] is not used.
     */
    s->heap_len = 0;
    s->heap_max = HEAP_SIZE;

    for (n = 0; n < elems; n++) {
        if (tree[n].fc.freq != 0) {
            s->heap[++(s->heap_len)] = max_code = n;
            s->depth[n] = 0;
        } else {
            tree[n].dl.len = 0;
        }
    }

    /* The pkzip format requires that at least one distance code exists,
     * and that at least one bit should be sent even if there is only one
     * possible code. So to avoid special checks later on we force at least
     * two codes of non zero frequency.
     */
    while (s->heap_len < 2) {
        if (max_code < 2) {
            node = s->heap[++(s->heap_len)] = ++max_code;
        } else {
            node = s->heap[++(s->heap_len)] = 0;
        }
        tree[node].fc.freq = 1;
        s->depth[node] = 0;
        s->opt_len--; if (stree) s->static_len -= stree[node].dl.len;
        /* node is 0 or 1 so it does not have extra bits */
    }
    desc->max_code = max_code;

    /* The elements heap[heap_len/2 + 1 .. heap_len] are leaves of the tree,
     * establish sub-heaps of increasing lengths:
     */
    for (n = s->heap_len/2; n >= 1; n--) pqdownheap(s, tree, n);

    /* Construct the Huffman tree by repeatedly combining the least two
     * frequent nodes.
     */
    node = elems;              /* next internal node of the tree */
    while (true) {
        /* n = node of least frequency */
        // Remove the smallest element from the heap and recreate the heap with
        // one less element. Updates heap and heap_len.
        n = s->heap[SMALLEST];
        s->heap[SMALLEST] = s->heap[s->heap_len--];
        pqdownheap(s, tree, SMALLEST);

        m = s->heap[SMALLEST]; /* m = node of next least frequency */

        s->heap[--(s->heap_max)] = n; /* keep the nodes sorted by frequency */
        s->heap[--(s->heap_max)] = m;

        /* Create a new node father of n and m */
        tree[node].fc.freq = tree[n].fc.freq + tree[m].fc.freq;
        int x;
        if (s->depth[n] >= s->depth[m]) {
            x = s->depth[n];
        } else {
            x = s->depth[m];
        }
        s->depth[node] = (uint8_t)(x + 1);
        tree[n].dl.dad = tree[m].dl.dad = (uint16_t)node;
        /* and insert the new node in the heap */
        s->heap[SMALLEST] = node++;
        pqdownheap(s, tree, SMALLEST);
        bool cont = (s->heap_len >= 2);
        if (!cont) break;
    }

    s->heap[--(s->heap_max)] = s->heap[SMALLEST];

    /* At this point, the fields freq and dad are set. We can now
     * generate the bit lengths.
     */
    gen_bitlen(s, desc);

    /* The field len is now set, we can generate the bit codes */
    gen_codes(tree, max_code, s->bl_count);
}

/* ===========================================================================
 * Scan a literal or distance tree to determine the frequencies of the codes
 * in the bit length tree.
 */
// deflate_state *s;
//     ct_data *tree;   /* the tree to be scanned */
//     int max_code;    /* and its largest code of non zero frequency */
void scan_tree(
    deflate_state *s,
    ct_data *tree,
    int max_code
) {
    int n;                     /* iterates over all tree elements */
    int prevlen = -1;          /* last emitted length */
    int curlen;                /* length of current code */
    int nextlen = tree[0].dl.len; /* length of next code */
    int count = 0;             /* repeat count of the current code */
    int max_count = 7;         /* max repeat count */
    int min_count = 4;         /* min repeat count */

    if (nextlen == 0) {
        max_count = 138;
        min_count = 3;
    }
    tree[max_code + 1].dl.len = (uint16_t)0xffff; /* guard */

    for (n = 0; n <= max_code; n++) {
        curlen = nextlen; nextlen = tree[n + 1].dl.len;
        if (++count < max_count && curlen == nextlen) {
            continue;
        } else if (count < min_count) {
            s->bl_tree[curlen].fc.freq += count;
        } else if (curlen != 0) {
            if (curlen != prevlen) s->bl_tree[curlen].fc.freq++;
            s->bl_tree[REP_3_6].fc.freq++;
        } else if (count <= 10) {
            s->bl_tree[REPZ_3_10].fc.freq++;
        } else {
            s->bl_tree[REPZ_11_138].fc.freq++;
        }
        count = 0;
        prevlen = curlen;
        if (nextlen == 0) {
            max_count = 138;
            min_count = 3;
        } else if (curlen == nextlen) {
            max_count = 6;
            min_count = 3;
        } else {
            max_count = 7;
            min_count = 4;
        }
    }
}

/* ===========================================================================
 * Send a literal or distance tree in compressed form, using the codes in
 * bl_tree.
 */
//  deflate_state *s;
//     ct_data *tree; /* the tree to be scanned */
//     int max_code;       /* and its largest code of non zero frequency */
void send_tree(
    deflate_state *s,
    ct_data *tree,
    int max_code
) {
    int n;                     /* iterates over all tree elements */
    int prevlen = -1;          /* last emitted length */
    int curlen;                /* length of current code */
    int nextlen = tree[0].dl.len; /* length of next code */
    int count = 0;             /* repeat count of the current code */
    int max_count = 7;         /* max repeat count */
    int min_count = 4;         /* min repeat count */

    /* tree[max_code + 1].dl.len = -1; */  /* guard already set */
    if (nextlen == 0) {
        max_count = 138;
        min_count = 3;
    }

    for (n = 0; n <= max_code; n++) {
        curlen = nextlen; nextlen = tree[n + 1].dl.len;
        if (++count < max_count && curlen == nextlen) {
            continue;
        } else if (count < min_count) {
            while (true) {
                send_code(s, curlen, s->bl_tree);
                bool cont = (--count != 0);
                if (!cont) break;
            }
        } else if (curlen != 0) {
            if (curlen != prevlen) {
                send_code(s, curlen, s->bl_tree); count--;
            }
            assert.Assert(count >= 3 && count <= 6, " 3_6?");
            send_code(s, REP_3_6, s->bl_tree); send_bits(s, count - 3, 2);

        } else if (count <= 10) {
            send_code(s, REPZ_3_10, s->bl_tree); send_bits(s, count - 3, 3);

        } else {
            send_code(s, REPZ_11_138, s->bl_tree); send_bits(s, count - 11, 7);
        }
        count = 0;
        prevlen = curlen;
        if (nextlen == 0) {
            max_count = 138;
            min_count = 3;
        } else if (curlen == nextlen) {
            max_count = 6;
            min_count = 3;
        } else {
            max_count = 7;
            min_count = 4;
        }
    }
}

/* ===========================================================================
 * Construct the Huffman tree for the bit lengths and return the index in
 * bl_order of the last bit length code to send.
 */
int build_bl_tree(deflate_state *s) {
    int max_blindex;  /* index of last bit length code of non zero freq */

    /* Determine the bit length frequencies for literal and distance trees */
    scan_tree(s, s->dyn_ltree, s->l_desc.max_code);
    scan_tree(s, s->dyn_dtree, s->d_desc.max_code);

    /* Build the bit length tree: */
    build_tree(s, &(s->bl_desc));
    /* opt_len now includes the length of the tree representations, except the
     * lengths of the bit lengths codes and the 5 + 5 + 4 bits for the counts.
     */

    /* Determine the number of bit length codes to send. The pkzip format
     * requires that at least 4 bit length codes be sent. (appnote.txt says
     * 3 but the actual value used is 4.)
     */
    for (max_blindex = BL_CODES-1; max_blindex >= 3; max_blindex--) {
        if (s->bl_tree[bl_order[max_blindex]].dl.len != 0) break;
    }
    /* Update opt_len to include the bit length tree and counts */
    s->opt_len += 3*((uint32_t)max_blindex + 1) + 5 + 5 + 4;
    trace.Tracev(stderr, "\ndyn trees: dyn %ld, stat %ld", s->opt_len, s->static_len);

    return max_blindex;
}

/* ===========================================================================
 * Send the header for a block using dynamic Huffman trees: the counts, the
 * lengths of the bit length codes, the literal tree and the distance tree.
 * IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
 */
//  deflate_state *s;
//     int lcodes, dcodes, blcodes; /* number of codes for each tree */
void send_all_trees(
    deflate_state *s,
    int lcodes, dcodes, blcodes
) {
    int rank;                    /* index in bl_order */

    assert.Assert(lcodes >= 257 && dcodes >= 1 && blcodes >= 4, "not enough codes");
    assert.Assert(lcodes <= L_CODES && dcodes <= D_CODES && blcodes <= BL_CODES, "too many codes");
    trace.Tracev(stderr, "\nbl counts: ");
    send_bits(s, lcodes - 257, 5);  /* not +255 as stated in appnote.txt */
    send_bits(s, dcodes - 1,   5);
    send_bits(s, blcodes - 4,  4);  /* not -3 as stated in appnote.txt */
    for (rank = 0; rank < blcodes; rank++) {
        trace.Tracev(stderr, "\nbl code %2d ", bl_order[rank]);
        send_bits(s, s->bl_tree[bl_order[rank]].dl.len, 3);
    }
    trace.Tracev(stderr, "\nbl tree: sent %ld", s->bits_sent);

    send_tree(s, s->dyn_ltree, lcodes - 1);  /* literal tree */
    trace.Tracev(stderr, "\nlit tree: sent %ld", s->bits_sent);

    send_tree(s, s->dyn_dtree, dcodes - 1);  /* distance tree */
    trace.Tracev(stderr, "\ndist tree: sent %ld", s->bits_sent);
}

/* ===========================================================================
 * Send a stored block
 */
//  deflate_state *s;
//   *buf;       /* input block */
//     uint32_t stored_len;   /* length of input block */
//     int last;         /* one if this is the last block for a file */
void  _tr_stored_block(
    deflate_state *s,
    char *buf,
    uint32_t stored_len,
    int last
) {
    send_bits(s, (STORED_BLOCK<<1) + last, 3);  /* send block type */
    bi_windup(s);        /* align on byte boundary */
    put_short(s, (uint16_t)stored_len);
    put_short(s, (uint16_t)~stored_len);
    if (stored_len) {
        memcpy(s->pending_buf + s->pending, (uint8_t *)buf, stored_len);
    }
    s->pending += stored_len;
    s->compressed_len = (s->compressed_len + 3 + 7) & (uint32_t)~7L;
    s->compressed_len += (stored_len + 4) << 3;
    s->bits_sent += 2*16;
    s->bits_sent += stored_len << 3;
}

/* ===========================================================================
 * Flush the bits in the bit buffer to pending output (leaves at most 7 bits)
 */
void  _tr_flush_bits(deflate_state *s) {
    bi_flush(s);
}

/* ===========================================================================
 * Send one empty static block to give enough lookahead for inflate.
 * This takes 10 bits, of which 7 may remain in the bit buffer.
 */
void  _tr_align(deflate_state *s) {
    send_bits(s, STATIC_TREES<<1, 3);
    send_code(s, END_BLOCK, static_ltree);
    s->compressed_len += 10L; /* 3 for block type, 7 for EOB */
    bi_flush(s);
}

/* ===========================================================================
 * Determine the best encoding for the current block: dynamic trees, static
 * trees or store, and write out the encoded block.
 */
// deflate_state *s;
// *buf;       /* input block, or NULL if too old */
// uint32_t stored_len;   /* length of input block */
// int last;         /* one if this is the last block for a file */
void  _tr_flush_block(
    deflate_state *s,
    char *buf,
    uint32_t stored_len,
    int last
) {
    uint32_t opt_lenb;
    uint32_t static_lenb; /* opt_len and static_len in bytes */
    int max_blindex = 0;  /* index of last bit length code of non zero freq */

    /* Build the Huffman trees unless a stored block is forced */
    if (s->level > 0) {

        /* Check if the file is binary or text */
        if (s->strm->data_type == Z_UNKNOWN) {
            s->strm->data_type = detect_data_type(s);
        }

        /* Construct the literal and distance trees */
        build_tree(s, &(s->l_desc));
        trace.Tracev(stderr, "\nlit data: dyn %ld, stat %ld", s->opt_len, s->static_len);

        build_tree(s, &(s->d_desc));
        trace.Tracev(stderr, "\ndist data: dyn %ld, stat %ld", s->opt_len, s->static_len);
        /* At this point, opt_len and static_len are the total bit lengths of
         * the compressed block data, excluding the tree representations.
         */

        /* Build the bit length tree for the above two trees, and get the index
         * in bl_order of the last bit length code to send.
         */
        max_blindex = build_bl_tree(s);

        /* Determine the best encoding. Compute the block lengths in bytes. */
        opt_lenb = (s->opt_len + 3 + 7) >> 3;
        static_lenb = (s->static_len + 3 + 7) >> 3;

        trace.Tracev(stderr, "\nopt %lu(%lu) stat %lu(%lu) stored %lu lit %u ",
                opt_lenb, s->opt_len, static_lenb, s->static_len, stored_len,
                s->sym_next / 3);

        if (static_lenb <= opt_lenb || s->strategy == Z_FIXED)
            opt_lenb = static_lenb;

    } else {
        assert.Assert(buf != NULL, "lost buf");
        opt_lenb = static_lenb = stored_len + 5; /* force a stored block */
    }

    if (stored_len + 4 <= opt_lenb && buf != NULL) {
                       /* 4: two words for the lengths */
        /* The test buf != NULL is only necessary if LIT_BUFSIZE > WSIZE.
         * Otherwise we can't have processed more than WSIZE input bytes since
         * the last block flush, because compression would have been
         * successful. If LIT_BUFSIZE <= WSIZE, it is never too late to
         * transform a block into a stored block.
         */
        _tr_stored_block(s, buf, stored_len, last);

    } else if (static_lenb == opt_lenb) {
        send_bits(s, (STATIC_TREES<<1) + last, 3);
        compress_block(s, static_ltree, static_dtree);
        s->compressed_len += 3 + s->static_len;
    } else {
        send_bits(s, (DYN_TREES<<1) + last, 3);
        send_all_trees(s, s->l_desc.max_code + 1, s->d_desc.max_code + 1, max_blindex + 1);
        compress_block(s, s->dyn_ltree, s->dyn_dtree);
        s->compressed_len += 3 + s->opt_len;
    }
    assert.Assert(s->compressed_len == s->bits_sent, "bad compressed size");
    /* The above check is made mod 2^32, for files larger than 512 MB
     * and uLong implemented on 32 bits.
     */
    init_block(s);

    if (last) {
        bi_windup(s);
        s->compressed_len += 7;  /* align on byte boundary */
    }
    trace.Tracev(stderr,"\ncomprlen %lu(%lu) ", s->compressed_len >> 3, s->compressed_len - 7*last);
}

/* ===========================================================================
 * Save the match info and tally the frequency counts. Return true if
 * the current block must be flushed.
 */
// deflate_state *s;
// unsigned dist;  /* distance of matched string */
// unsigned lc;    /* match length - MIN_MATCH or unmatched char (dist==0) */
int  _tr_tally(
    deflate_state *s,
    unsigned dist,
    unsigned lc
) {
    s->sym_buf[s->sym_next++] = (uint8_t)dist;
    s->sym_buf[s->sym_next++] = (uint8_t)(dist >> 8);
    s->sym_buf[s->sym_next++] = (uint8_t)lc;
    if (dist == 0) {
        /* lc is the unmatched char */
        s->dyn_ltree[lc].fc.freq++;
    } else {
        s->matches++;
        /* Here, lc is the match length - MIN_MATCH */
        dist--;             /* dist = match distance - 1 */
        assert.Assert((uint16_t)dist < (uint16_t)MAX_DIST(s) &&
               (uint16_t)lc <= (uint16_t)(MAX_MATCH-MIN_MATCH) &&
               (uint16_t)d_code(dist) < (uint16_t)D_CODES,  "_tr_tally: bad match");

        s->dyn_ltree[_length_code[lc] + LITERALS + 1].fc.freq++;
        s->dyn_dtree[d_code(dist)].fc.freq++;
    }
    return (s->sym_next == s->sym_end);
}

/* ===========================================================================
 * Send the block data compressed using the given Huffman trees
 */
//  deflate_state *s;
//     const ct_data *ltree; /* literal tree */
//     const ct_data *dtree; /* distance tree */
void compress_block(
    deflate_state *s,
    const ct_data *ltree,
    const ct_data *dtree
) {
    unsigned dist;      /* distance of matched string */
    int lc;             /* match length or unmatched char (if dist == 0) */
    unsigned sx = 0;    /* running index in sym_buf */
    unsigned code;      /* the code to send */
    int extra;          /* number of extra bits to send */

    if (s->sym_next != 0) {
        while (true) {
            dist = s->sym_buf[sx++] & 0xff;
            dist += (unsigned)(s->sym_buf[sx++] & 0xff) << 8;
            lc = s->sym_buf[sx++];
            if (dist == 0) {
                send_code(s, lc, ltree); /* send a literal byte */
                if (isgraph(lc)) {
                    trace.Tracevv(stderr," '%c' ", lc);
                }
            } else {
                /* Here, lc is the match length - MIN_MATCH */
                code = _length_code[lc];
                send_code(s, code + LITERALS + 1, ltree);   /* send length code */
                extra = extra_lbits[code];
                if (extra != 0) {
                    lc -= base_length[code];
                    send_bits(s, lc, extra);       /* send the extra length bits */
                }
                dist--; /* dist is now the match distance - 1 */
                code = d_code(dist);
                assert.Assert(code < D_CODES, "bad d_code");

                send_code(s, code, dtree);       /* send the distance code */
                extra = extra_dbits[code];
                if (extra != 0) {
                    dist -= (unsigned)base_dist[code];
                    send_bits(s, dist, extra);   /* send the extra distance bits */
                }
            } /* literal or match pair ? */

            /* Check that the overlay between pending_buf and sym_buf is ok: */
            assert.Assert(s->pending < s->lit_bufsize + sx, "pendingBuf overflow");
            if (sx >= s->sym_next) break;
        }
    }

    send_code(s, END_BLOCK, ltree);
}

/* ===========================================================================
 * Check if the data type is TEXT or BINARY, using the following algorithm:
 * - TEXT if the two conditions below are satisfied:
 *    a) There are no non-portable control characters belonging to the
 *       "block list" (0..6, 14..25, 28..31).
 *    b) There is at least one printable character belonging to the
 *       "allow list" (9 {TAB}, 10 {LF}, 13 {CR}, 32..255).
 * - BINARY otherwise.
 * - The following partially-portable control characters form a
 *   "gray list" that is ignored in this detection algorithm:
 *   (7 {BEL}, 8 {BS}, 11 {VT}, 12 {FF}, 26 {SUB}, 27 {ESC}).
 * IN assertion: the fields fc.freq of dyn_ltree are set.
 */
int detect_data_type(deflate_state *s) {
    /* block_mask is the bit mask of block-listed bytes
     * set bits 0..6, 14..25, and 28..31
     * 0xf3ffc07f = binary 11110011111111111100000001111111
     */
    uint32_t block_mask = 0xf3ffc07fUL;

    /* Check for non-textual ("block-listed") bytes. */
    for (int n = 0; n <= 31; n++) {
        if ((block_mask & 1) && (s->dyn_ltree[n].fc.freq != 0)) {
            return Z_BINARY;
        }
        block_mask >>= 1;
    }

    /* Check for textual ("allow-listed") bytes. */
    if (s->dyn_ltree[9].fc.freq != 0 || s->dyn_ltree[10].fc.freq != 0
            || s->dyn_ltree[13].fc.freq != 0) {
        return Z_TEXT;
    }
    for (int n = 32; n < LITERALS; n++) {
        if (s->dyn_ltree[n].fc.freq != 0) {
            return Z_TEXT;
        }
    }

    /* There are no "block-listed" or "allow-listed" bytes:
     * this stream either is empty or has tolerated ("gray-listed") bytes only.
     */
    return Z_BINARY;
}

/* ===========================================================================
 * Reverse the first len bits of a code, using straightforward code (a faster
 * method would use a table)
 * IN assertion: 1 <= len <= 15
 */
uint32_t bi_reverse(uint32_t code, size_t len) {
    uint32_t res = 0;
    while (true) {
        res |= code & 1;
        code >>= 1;
        res <<= 1;
        if (--len == 0) break;
    }
    return res >> 1;
}

/* ===========================================================================
 * Flush the bit buffer, keeping at most 7 bits in it.
 */
void bi_flush(deflate_state *s) {
    if (s->bi_valid == 16) {
        put_short(s, s->bi_buf);
        s->bi_buf = 0;
        s->bi_valid = 0;
    } else if (s->bi_valid >= 8) {
        put_byte(s, (uint8_t)s->bi_buf);
        s->bi_buf >>= 8;
        s->bi_valid -= 8;
    }
}

/* ===========================================================================
 * Flush the bit buffer and align the output on a byte boundary
 */
void bi_windup(deflate_state *s) {
    if (s->bi_valid > 8) {
        put_short(s, s->bi_buf);
    } else if (s->bi_valid > 0) {
        put_byte(s, (uint8_t)s->bi_buf);
    }
    s->bi_buf = 0;
    s->bi_valid = 0;
    s->bits_sent = (s->bits_sent + 7) & ~7;
}


// -----------------------------

// Max size of a Huffman code.
#define MAXBITS 15

// ----------------------------

/*
 * Initializes state for decompression.
 */
pub int inflateInit(z_stream *strm) {
    if (strm == NULL) {
        return Z_STREAM_ERROR;
    }

    strm->msg = NULL;

    // Allocate the state.
    inflate_state *state = calloc(1, sizeof(inflate_state));
    if (state == NULL) {
        return Z_MEM_ERROR;
    }
    state->strm = strm;
    state->window = NULL;
    state->mode = HEAD;     /* to pass state test in inflateReset2() */
    strm->state = state;

    // ...
    int ret = inflateReset2(strm, DEF_WBITS);
    if (ret != Z_OK) {
        free(state);
        strm->state = NULL;
    }
    return ret;
    /*
    The fields next_in, avail_in must be initialized before by the caller.
    So next_in, and avail_in, next_out, and avail_out are unused and unchanged.  The current
    implementation of inflateInit() does not process any header information --
    that is deferred until inflate() is called.

    returns Z_OK if success, Z_MEM_ERROR if there was not enough
    memory, Z_STREAM_ERROR if the parameters are invalid, such as a null pointer to the structure.

The windowBits parameter is the base two logarithm of the maximum window size (the size of the history buffer).
It should be in the range 8..15 for this version of the library.
The default value is 15 if inflateInit is used instead. 
windowBits must be greater than or equal to the windowBits value provided to deflateInit2() while compressing, or it must be equal to 15 if deflateInit2() was not used.  If a compressed stream with a larger window
   size is given as input, inflate() will return with the error code
   Z_DATA_ERROR instead of trying to allocate a larger window.

     windowBits can also be zero to request that inflate use the window size in
   the zlib header of the compressed stream.

     windowBits can also be -8..-15 for raw inflate.  In this case, -windowBits
   determines the window size.  inflate() will then process raw deflate data,
   not looking for a zlib or gzip header, not generating a check value, and not
   looking for any check values for comparison at the end of the stream.  This
   is for use with other formats that use the deflate compressed data format
   such as zip.  Those formats provide their own check values.  If a custom
   format is developed using the raw deflate format for compressed data, it is
   recommended that a check value such as an Adler-32 or a CRC-32 be applied to
   the uncompressed data as is done in the zlib, gzip, and zip formats.  For
   most applications, the zlib format should be used as is.  Note that comments
   above on the use in deflateInit2() applies to the magnitude of windowBits.

     windowBits can also be greater than 15 for optional gzip decoding.  Add
   32 to windowBits to enable zlib and gzip decoding with automatic header
   detection, or add 16 to decode only the gzip format (the zlib format will
   return a Z_DATA_ERROR).  If a gzip stream is being decoded, strm->adler is a
   CRC-32 instead of an Adler-32.  Unlike the gunzip utility and gzread() (see
   below), inflate() will *not* automatically decode concatenated gzip members.
   inflate() will return Z_STREAM_END at the end of the gzip member.  The state
   would need to be reset to continue decoding a subsequent gzip member.  This
   *must* be done if there is more data after a gzip member, in order for the
   decompression to be compliant with the gzip standard (RFC 1952).

     inflateInit2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_VERSION_ERROR if the zlib library version is incompatible with the
   version assumed by the caller, or Z_STREAM_ERROR if the parameters are
   invalid, such as a null pointer to the structure.  msg is set to null if
   there is no error message.  inflateInit2 does not perform any decompression
   apart from possibly reading the zlib header if present: actual decompression
   will be done by inflate().  (So next_in and avail_in may be modified, but
   next_out and avail_out are unused and unchanged.) The current implementation
   of inflateInit2() does not process any header information -- that is
   deferred until inflate() is called.
*/
    
}


/* default windowBits for decompression. MAX_WBITS is for compression only */
#define DEF_WBITS MAX_WBITS

const int GZIP_MAGIC = 0x8b1f;

bool PKZIP_BUG_WORKAROUND = false;


/* Type of code to build for inflate_table() */
enum {
    CODES,
    LENS,
    DISTS
};

/* Maximum size of the dynamic table.  The maximum number of code structures is
   1444, which is the sum of 852 for literal/length codes and 592 for distance
   codes.  These values were found by exhaustive searches using the program
   examples/enough.c found in the zlib distribution.  The arguments to that
   program are the number of symbols, the initial root table size, and the
   maximum bit length of a code.  "enough 286 9 15" for literal/length codes
   returns returns 852, and "enough 30 6 15" for distance codes returns 592.
   The initial root table size (9 or 6) is found in the fifth argument of the
   inflate_table() calls in inflate.c and infback.c.  If the root table size is
   changed, then these maximum sizes would be need to be recalculated and
   updated. */
#define ENOUGH_LENS 852
#define ENOUGH_DISTS 592
enum {
    ENOUGH = ENOUGH_LENS + ENOUGH_DISTS
};

/* Structure for decoding tables.  Each entry provides either the
   information needed to do the operation requested by the code that
   indexed that table entry, or it provides a pointer to another
   table that indexes more bits of the code.  op indicates whether
   the entry is a pointer to another table, a literal, a length or
   distance, an end-of-block, or an invalid code.  For a table
   pointer, the low four bits of op is the number of index bits of
   that table.  For a length or distance, the low four bits of op
   is the number of extra bits to get after the code.  bits is
   the number of bits in this code or part of the code to drop off
   of the bit buffer.  val is the actual byte to output in the case
   of a literal, the base length or distance, or the offset from
   the current table to the next table.  Each entry is four bytes. */
typedef {
    uint8_t op;           /* operation, extra bits, table bits */
    uint8_t bits;         /* bits in this part of the code */
    uint16_t val;         /* offset in table or code value */
} code_t;
/* op values as set by inflate_table():
    00000000 - literal
    0000tttt - table link, tttt != 0 is the number of table index bits
    0001eeee - length or distance, eeee is the number of extra bits
    01100000 - end of block
    01000000 - invalid code
 */

 /* Reverse the bytes in a 32-bit value */
uint32_t ZSWAP32(uint32_t q) {
    return ((q >> 24) & 0xff)
        + ((q >> 8) & 0xff00)
        + ((q & 0xff00) << 8)
        + ((q & 0xff) << 24);
}


/* Possible inflate modes between inflate() calls */
enum {
    HEAD = 16180,   /* i: waiting for magic header */
    FLAGS,      /* i: waiting for method and flags (gzip) */
    TIME,       /* i: waiting for modification time (gzip) */
    OS,         /* i: waiting for extra flags and operating system (gzip) */
    EXLEN,      /* i: waiting for extra length (gzip) */
    EXTRA,      /* i: waiting for extra bytes (gzip) */
    NAME,       /* i: waiting for end of file name (gzip) */
    COMMENT,    /* i: waiting for end of comment (gzip) */
    HCRC,       /* i: waiting for header crc (gzip) */
    DICTID,     /* i: waiting for dictionary check value */
    DICT,       /* waiting for inflateSetDictionary() call */
        TYPE,       /* i: waiting for type bits, including last-flag bit */
        TYPEDO,     /* i: same, but skip check to exit inflate on new block */
        STORED,     /* i: waiting for stored size (length and complement) */
        COPY_,      /* i/o: same as COPY below, but only first time in */
        COPY,       /* i/o: waiting for input or output to copy stored block */
        TABLE,      /* i: waiting for dynamic block table lengths */
        LENLENS,    /* i: waiting for code length code lengths */
        CODELENS,   /* i: waiting for length/lit and distance code lengths */
            LEN_,       /* i: same as LEN below, but only first time in */
            LEN,        /* i: waiting for length/lit/eob code */
            LENEXT,     /* i: waiting for length extra bits */
            DIST,       /* i: waiting for distance code */
            DISTEXT,    /* i: waiting for distance extra bits */
            MATCH,      /* o: waiting for output space to copy string */
            LIT,        /* o: waiting for output space to write literal */
    CHECK,      /* i: waiting for 32-bit check value */
    LENGTH,     /* i: waiting for 32-bit length (gzip) */
    DONE,       /* finished check, done -- remain here until reset */
    BAD,        /* got a data error -- remain here until reset */
    MEM,        /* got an inflate() memory error -- remain here until reset */
    SYNC        /* looking for synchronization bytes to restart inflate() */
}; // inflate_mode;

/*
    State transitions between above modes -

    (most modes can go to BAD or MEM on error -- not shown for clarity)

    Process header:
        HEAD -> (gzip) or (zlib) or (raw)
        (gzip) -> FLAGS -> TIME -> OS -> EXLEN -> EXTRA -> NAME -> COMMENT ->
                  HCRC -> TYPE
        (zlib) -> DICTID or TYPE
        DICTID -> DICT -> TYPE
        (raw) -> TYPEDO
    Read deflate blocks:
            TYPE -> TYPEDO -> STORED or TABLE or LEN_ or CHECK
            STORED -> COPY_ -> COPY -> TYPE
            TABLE -> LENLENS -> CODELENS -> LEN_
            LEN_ -> LEN
    Read deflate codes in fixed or dynamic block:
                LEN -> LENEXT or LIT or TYPE
                LENEXT -> DIST -> DISTEXT -> MATCH -> LEN
                LIT -> LEN
    Process trailer:
        CHECK -> LENGTH -> DONE
 */




/* State maintained between inflate() calls -- approximately 7K bytes, not
   including the allocated sliding window, which is up to 32K bytes. */
typedef {
    z_stream *strm;             /* pointer back to this zlib stream */
    int mode;          /* current inflate mode */
    int last;                   /* true if processing last block */
    int wrap;                   /* bit 0 true for zlib, bit 1 true for gzip,
                                   bit 2 true to validate check value */
    int havedict;               /* true if dictionary provided */
    int flags;                  /* gzip header method and flags, 0 if zlib, or
                                   -1 if raw or no header yet */
    unsigned dmax;              /* zlib header max distance (INFLATE_STRICT) */
    uint32_t check;        /* protected copy of check value */
    uint32_t total;        /* protected copy of output count */
    gz_header *head;            /* where to save gzip header information */
        /* sliding window */
    unsigned wbits;             /* log base 2 of requested window size */
    unsigned wsize;             /* window size or zero if not using window */
    unsigned whave;             /* valid bytes in the window */
    unsigned wnext;             /* window write index */
    uint8_t *window;  /* allocated sliding window, if needed */
        /* bit accumulator */
    uint32_t hold;         /* input bit accumulator */
    unsigned bits;              /* number of bits in "in" */
        /* for string and stored block copying */
    unsigned length;            /* literal or length of data to copy */
    unsigned offset;            /* distance back to copy string from */
        /* for table and code decoding */
    unsigned extra;             /* extra bits needed */
        /* fixed and dynamic code tables */
    code_t *lencode;    /* starting table for length/literal codes */
    code_t *distcode;   /* starting table for distance codes */
    unsigned lenbits;           /* index bits for lencode */
    unsigned distbits;          /* index bits for distcode */
        /* dynamic table building */
    unsigned ncode;             /* number of code length code lengths */
    unsigned nlen;              /* number of length code lengths */
    unsigned ndist;             /* number of distance code lengths */
    unsigned have;              /* number of code lengths in lens[] */
    code_t *next;             /* next available space in codes[] */
    uint16_t lens[320];   /* temporary storage for code lengths */
    uint16_t work[288];   /* work area for code table building */
    code_t codes[ENOUGH];         /* space for code tables */
    int sane;                   /* if false, allow invalid distance too far */
    int back;                   /* bits back of last unprocessed length/lit */
    unsigned was;               /* initial length of match */
} inflate_state;

bool should_validate_check_value(inflate_state *s) {
    return s->wrap & 4;
}


int inflateStateCheck(z_stream *strm) {
    if (!strm) {
        return 1;
    }
    inflate_state *state = strm->state;
    if (!state || state->strm != strm || state->mode < HEAD || state->mode > SYNC) {
        return 1;
    }
    return 0;
}

pub int inflateResetKeep(z_stream *strm) {
    inflate_state *state;

    if (inflateStateCheck(strm)) return Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    strm->total_in = strm->total_out = state->total = 0;
    strm->msg = NULL;
    if (state->wrap)        /* to support ill-conceived Java test suite */
        strm->adler = state->wrap & 1;
    state->mode = HEAD;
    state->last = 0;
    state->havedict = 0;
    state->flags = -1;
    state->dmax = 32768U;
    state->head = NULL;
    state->hold = 0;
    state->bits = 0;
    state->lencode = state->distcode = state->next = state->codes;
    state->sane = 1;
    state->back = -1;
    trace.Tracev(stderr, "inflate: reset\n");
    return Z_OK;
}


/*
     This function is equivalent to inflateEnd followed by inflateInit,
   but does not free and reallocate the internal decompression state.  The
   stream will keep attributes that may have been set by inflateInit2.

     inflateReset returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent.
*/
pub int inflateReset(z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }
    inflate_state *state = (inflate_state *)strm->state;
    state->wsize = 0;
    state->whave = 0;
    state->wnext = 0;
    return inflateResetKeep(strm);
}

/*
     This function is the same as inflateReset, but it also permits changing
   the wrap and window size requests.  The windowBits parameter is interpreted
   the same as it is for inflateInit2.  If the window size is changed, then the
   memory allocated for the window is freed, and the window will be reallocated
   by inflate() if needed.

     inflateReset2 returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent, or if
   the windowBits parameter is invalid.
*/
pub int inflateReset2(z_stream *strm, int windowBits) {
    /* get the state */
    if (inflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }
    inflate_state *state = strm->state;

    int wrap = 0;
    /* extract wrap request from windowBits parameter */
    if (windowBits < 0) {
        if (windowBits < -15)
            return Z_STREAM_ERROR;
        wrap = 0;
        windowBits = -windowBits;
    }
    else {
        wrap = (windowBits >> 4) + 5;
        if (windowBits < 48)
            windowBits &= 15;
    }

    /* set number of window bits, free window if different */
    if (windowBits && (windowBits < 8 || windowBits > 15))
        return Z_STREAM_ERROR;
    if (state->window != NULL && state->wbits != (unsigned)windowBits) {
        free(state->window);
        state->window = NULL;
    }

    /* update state and reset the rest of it */
    state->wrap = wrap;
    state->wbits = (unsigned)windowBits;
    return inflateReset(strm);
}




/*
     This function inserts bits in the inflate input stream.  The intent is
   that this function is used to start inflating at a bit position in the
   middle of a byte.  The provided bits will be used before any bytes are used
   from next_in.  This function should only be used with raw inflate, and
   should be used before the first inflate() call after inflateInit2() or
   inflateReset().  bits must be less than or equal to 16, and that many of the
   least significant bits of value will be inserted in the input.

     If bits is negative, then the input stream bit buffer is emptied.  Then
   inflatePrime() can be called again to put bits in the buffer.  This is used
   to clear out bits leftover after feeding inflate a block description prior
   to feeding inflate codes.

     inflatePrime returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent.
*/
pub int inflatePrime(
    z_stream *strm,
    int bits,
    int value
) {
    inflate_state *state;

    if (inflateStateCheck(strm)) return Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    if (bits < 0) {
        state->hold = 0;
        state->bits = 0;
        return Z_OK;
    }
    if (bits > 16 || state->bits + (uint32_t)bits > 32) return Z_STREAM_ERROR;
    value &= (1L << bits) - 1;
    state->hold += (unsigned)value << state->bits;
    state->bits += (uint32_t)bits;
    return Z_OK;
}

/*
   Return state with length and distance decoding tables and index sizes set to
   fixed code decoding.  Normally this returns fixed tables from inffixed.h.
   If BUILDFIXED is defined, then instead this routine builds the tables the
   first time it's called, and returns those tables the first time and
   thereafter.  This reduces the size of the code by about 2K bytes, in
   exchange for a little execution time.  However, BUILDFIXED should not be
   used for threaded applications, since the rewriting of the tables and fixedtables_init
   may not be thread-safe.
 */
bool fixedtables_init = false;
code_t *lenfix = NULL;
code_t *distfix = NULL;
code_t fixed[544] = {};
code_t *fixedtables_next = NULL;
void fixedtables(inflate_state *state) {
    /* build fixed huffman tables if first call (may not be thread safe) */
    if (!fixedtables_init) {
        unsigned sym;
        unsigned bits;


        /* literal/length table */
        sym = 0;
        while (sym < 144) state->lens[sym++] = 8;
        while (sym < 256) state->lens[sym++] = 9;
        while (sym < 280) state->lens[sym++] = 7;
        while (sym < 288) state->lens[sym++] = 8;
        fixedtables_next = fixed;
        lenfix = fixedtables_next;
        bits = 9;
        inflate_table(LENS, state->lens, 288, &(fixedtables_next), &(bits), state->work);

        /* distance table */
        sym = 0;
        while (sym < 32) state->lens[sym++] = 5;
        distfix = fixedtables_next;
        bits = 5;
        inflate_table(DISTS, state->lens, 32, &(fixedtables_next), &(bits), state->work);

        /* do this just once */
        fixedtables_init = true;
    }
    state->lencode = lenfix;
    state->lenbits = 9;
    state->distcode = distfix;
    state->distbits = 5;
}

/*
   Write out the inffixed.h that is #include'd above.  Defining MAKEFIXED also
   defines BUILDFIXED, so the tables are built on the fly.  makefixed() writes
   those tables to stdout, which would be piped to inffixed.h.  A small program
   can simply call makefixed to do this:

    void makefixed(void);

    int main(void)
    {
        makefixed();
        return 0;
    }
    a.out > inffixed.h
 */
pub void makefixed()
{
    unsigned low;
    unsigned size;
    inflate_state state;

    fixedtables(&state);
    puts("    /* inffixed.h -- table for decoding fixed codes");
    puts("     * Generated automatically by makefixed().");
    puts("     */");
    puts("");
    puts("    /* WARNING: this file should *not* be used by applications.");
    puts("       It is part of the implementation of this library and is");
    puts("       subject to change. Applications should only use zlib.h.");
    puts("     */");
    puts("");
    size = 1U << 9;
    printf("    static const code lenfix[%u] = {", size);
    low = 0;
    while (true) {
        if ((low % 7) == 0) printf("\n        ");
        int x;
        if (low & 127 == 99) {
            x = 64;
        } else {
            x = state.lencode[low].op;
        }
        printf("{%u,%u,%d}", x, state.lencode[low].bits, state.lencode[low].val);
        if (++low == size) {
            break;
        }
        putchar(',');
    }
    puts("\n    };");
    size = 1U << 5;
    printf("\n    static const code distfix[%u] = {", size);
    low = 0;
    while (true) {
        if ((low % 6) == 0) printf("\n        ");
        printf("{%u,%u,%d}", state.distcode[low].op, state.distcode[low].bits,
               state.distcode[low].val);
        if (++low == size) break;
        putchar(',');
    }
    puts("\n    };");
}

/*
   Update the window with the last wsize (normally 32K) bytes written before
   returning.  If window does not exist yet, create it.  This is only called
   when a window is already in use, or when output has been written during this
   inflate call, but the end of the deflate stream has not been reached yet.
   It is also called to create a window for dictionary data when a dictionary
   is loaded.

   Providing output buffers larger than 32K to inflate() should provide a speed
   advantage, since only the last 32K of output is copied to the sliding window
   upon return from inflate(), and since all distances after the first 32K of
   output will fall in the output data, making match copies simpler and faster.
   The advantage may be dependent on the size of the processor's data caches.
 */
int updatewindow(
    z_stream *strm,
    const uint8_t *end,
    unsigned copy
)

{
    inflate_state *state;
    unsigned dist;

    state = (inflate_state *)strm->state;

    /* if it hasn't been done already, allocate space for the window */
    if (state->window == NULL) {
        state->window = calloc(1U << state->wbits, 1);
        if (state->window == NULL) {
            return 1;
        }
    }

    /* if window not in use yet, initialize */
    if (state->wsize == 0) {
        state->wsize = 1U << state->wbits;
        state->wnext = 0;
        state->whave = 0;
    }

    /* copy state->wsize or less output bytes into the circular window */
    if (copy >= state->wsize) {
        memcpy(state->window, end - state->wsize, state->wsize);
        state->wnext = 0;
        state->whave = state->wsize;
    }
    else {
        dist = state->wsize - state->wnext;
        if (dist > copy) dist = copy;
        memcpy(state->window + state->wnext, end - copy, dist);
        copy -= dist;
        if (copy) {
            memcpy(state->window, end - copy, copy);
            state->wnext = copy;
            state->whave = state->wsize;
        }
        else {
            state->wnext += dist;
            if (state->wnext == state->wsize) state->wnext = 0;
            if (state->whave < state->wsize) state->whave += dist;
        }
    }
    return 0;
}

/* check function to use adler32.adler32() for zlib or crc32() for gzip */
int UPDATE_CHECK(ctx_t *ctx, char *buf, size_t len) {
    if (ctx->state->flags) {
        return crc32.crc32(ctx->state->check, buf, len);
    }
    return adler32.adler32(ctx->state->check, buf, len);
}

/* check macros for header crc */
void CRC2(ctx_t *ctx) {
    uint16_t word = ctx->hold;
    uint8_t hbuf[] = {
        word & 0xFF,
        (word >> 8) & 0xFF
    };
    ctx->state->check = crc32.crc32(ctx->state->check, hbuf, 2);
}

/* Load registers with state in inflate() for speed */
void LOAD(ctx_t *ctx) {
    ctx->put = ctx->strm->next_out;
    ctx->left = ctx->strm->avail_out;
    ctx->next = ctx->strm->next_in;
    ctx->have = ctx->strm->avail_in;
    ctx->hold = ctx->state->hold;
    ctx->bits = ctx->state->bits;
}

/* Restore state from registers in inflate() */
void RESTORE(ctx_t *ctx) {
    ctx->strm->next_out = ctx->put;
    ctx->strm->avail_out = ctx->left;
    ctx->strm->next_in = ctx->next;
    ctx->strm->avail_in = ctx->have;
    ctx->state->hold = ctx->hold;
    ctx->state->bits = ctx->bits;
}

/* Clear the input bit accumulator */
void CLEARBITS(ctx_t *ctx) {
    ctx->hold = 0;
    ctx->bits = 0;
}

/* Get a byte of input into the bit accumulator, or return from inflate()
   if there is no input available. */
bool PULLBYTE(ctx_t *ctx) {
    if (ctx->have == 0) {
        return false;
    }
    // Get next byte, extend it to 32 bits because we'll shift it.
    uint32_t x = (uint32_t)(*ctx->next);
    // Shift to accomodate for the bits already in the accumulator.
    x <<= ctx->bits;
    // Merge to the accumulator.
    ctx->hold += x;

    ctx->have--;
    ctx->next++;
    ctx->bits += 8;
    return true;
}

/* Pulls at least n bits in the bit accumulator.
Returns false if there is not enough available input. */
bool PULLBITS(ctx_t *ctx, size_t n) {
    while (ctx->bits < n) {
        if (!PULLBYTE(ctx)) {
            return false;
        }
    }
    return true;
}

/* Return the low n bits of the bit accumulator (n < 16) */
int BITS(ctx_t *ctx, size_t n) {
    if (n >= 16) {
        panic("n >= 16");
    }
    return ctx->hold & ((1U << n) - 1);
}

/* Remove n bits from the bit accumulator */
void DROPBITS(ctx_t *ctx, size_t n) {
    ctx->hold >>= n;
    ctx->bits -= n;
}


/* permutation of code lengths */
const uint16_t order[19] =
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/*

inflate decompresses as much data as possible and stops when the input buffer
becomes empty or the output buffer becomes full.

It may introduce some output latency (reading input without producing any output) except when forced to flush.

inflate performs one or both of the following actions:

Takes partial input available in next_in, decodes and puts into next_out.
Positions avail_in and avail_out are updated accordingly.

The caller should ensure that input has some data to decode and output has
free space by managing next_in, next_out, avail_in, avail_out.

The application can consume the uncompressed output when it wants, for example
when the output buffer is full (avail_out == 0), or after each call of inflate().

If inflate returns Z_OK and with zero avail_out, it must be called again after making room in the output buffer because there might be more output pending.

The flush parameter:

- Z_SYNC_FLUSH: flush as much output as possible to the output buffer
- Z_BLOCK: stop if and when it gets to the next deflate block boundary.  When decoding
  the zlib or gzip format, this will cause inflate() to return immediately
  after the header and before the first block.  When doing a raw inflate,
  inflate() will go ahead and process the first block, and will return when it
  gets to the end of that block, or when it runs out of data.

Z_BLOCK option assists in appending to or combining deflate streams.
  To assist in this, on return inflate() always sets strm->data_type to the
  number of unused bits in the last byte taken from strm->next_in, plus 64 if
  inflate() is currently decoding the last block in the deflate stream, plus
  128 if inflate() returned immediately after decoding an end-of-block code or
  decoding the complete header up to just before the first byte of the deflate
  stream.  The end-of-block will not be indicated until all of the uncompressed
  data from that block has been written to strm->next_out.  The number of
  unused bits may in general be greater than seven, except when bit 7 of
  data_type is set, in which case the number of unused bits will be less than
  eight.  data_type is set as noted here every time inflate() returns for all
  flush options, and so can be used to determine the amount of currently
  consumed input in bits.

    The stream.Z_TREES option behaves as Z_BLOCK does, but it also returns when the
  end of each deflate block header is reached, before any actual data in that
  block is decoded.  This allows the caller to determine the length of the
  deflate block header for later use in random access within a deflate block.
  256 is added to the value of strm->data_type when inflate() returns
  immediately after reaching the end of the deflate block header.

inflate() should normally be called until it returns Z_STREAM_END or an
  error.  However if all decompression is to be performed in a single step (a
  single call of inflate), the parameter flush should be set to Z_FINISH.  In
  this case all pending input is processed and all pending output is flushed;
  avail_out must be large enough to hold all of the uncompressed data for the
  operation to complete.  (The size of the uncompressed data may have been
  saved by the compressor for this purpose.)  The use of Z_FINISH is not
  required to perform an inflation in one step.  However it may be used to
  inform inflate that a faster approach can be used for the single inflate()
  call.  Z_FINISH also informs inflate to not maintain a sliding window if the
  stream completes, which reduces inflate's memory footprint.  If the stream
  does not complete, either because not all of the stream is provided or not
  enough output space is provided, then a sliding window will be allocated and
  inflate() can be called again to continue the operation as if Z_NO_FLUSH had
  been used.

     In this implementation, inflate() always flushes as much output as
  possible to the output buffer, and always uses the faster approach on the
  first call.  So the effects of the flush parameter in this implementation are
  on the return value of inflate() as noted below, when inflate() returns early
  when Z_BLOCK or stream.Z_TREES is used, and when inflate() avoids the allocation of
  memory for a sliding window when Z_FINISH is used.

     If a preset dictionary is needed after this call (see inflateSetDictionary
  below), inflate sets strm->adler to the Adler-32 checksum of the dictionary
  chosen by the compressor and returns stream.Z_NEED_DICT; otherwise it sets
  strm->adler to the Adler-32 checksum of all output produced so far (that is,
  total_out bytes) and returns Z_OK, Z_STREAM_END or an error code as described
  below.  At the end of the stream, inflate() checks that its computed Adler-32
  checksum is equal to that saved by the compressor and returns Z_STREAM_END
  only if the checksum is correct.

    inflate() can decompress and check either zlib-wrapped or gzip-wrapped
  deflate data.  The header type is detected automatically, if requested when
  initializing with inflateInit2().  Any information contained in the gzip
  header is not retained unless inflateGetHeader() is used.  When processing
  gzip-wrapped deflate data, strm->adler32.adler32 is set to the CRC-32 of the output
  produced so far.  The CRC-32 is checked against the gzip trailer, as is the
  uncompressed length, modulo 2^32.

    inflate() returns Z_OK if some progress has been made (more input processed
  or more output produced), Z_STREAM_END if the end of the compressed data has
  been reached and all uncompressed output has been produced, stream.Z_NEED_DICT if a
  preset dictionary is needed at this point, Z_DATA_ERROR if the input data was
  corrupted (input stream not conforming to the zlib format or incorrect check
  value, in which case strm->msg points to a string with a more specific
  error), Z_STREAM_ERROR if the stream structure was inconsistent (for example
  next_in or next_out was NULL, or the state was inadvertently written over
  by the application), Z_MEM_ERROR if there was not enough memory, Z_BUF_ERROR
  if no progress was possible or if there was not enough room in the output
  buffer when Z_FINISH is used.  Note that Z_BUF_ERROR is not fatal, and
  inflate() can be called again with more input and more output space to
  continue decompressing.  If Z_DATA_ERROR is returned, the application may
  then call inflateSync() to look for a good compression block if a partial
  recovery of the data is to be attempted.
*/

/*
 * Process as much input data and generate as much output data as possible
 * before returning.

   inflate() uses a state machine structured roughly as follows:

    while (true) switch (state) {
    ...
    case STATEn:
        if (not enough input data or output space to make progress)
            return;
        ... make progress ...
        state = STATEm;
        break;
    ...
    }

   so when inflate() is called again, the same case is attempted again

The PULLBITS() macro is usually the way the state evaluates whether it can
proceed or should return.

        PULLBITS(n);
        ... do something with BITS(n) ...
        DROPBITS(n);

   where PULLBITS(n) either returns from inflate() if there isn't enough
   input left to load n bits into the accumulator, or it continues.  BITS(n)
   gives the low n bits in the accumulator.  When done, DROPBITS(n) drops
   the low n bits off the accumulator.  CLEARBITS(ctx) clears the accumulator
   and sets the number of available bits to zero.  BYTEBITS() discards just
   enough bits to put the accumulator on a byte boundary.  After BYTEBITS()
   and a PULLBITS(ctx, 8), then BITS(ctx, 8) would return the next byte in the stream.

   Some states loop until they get enough input, making sure that enough
   state information is maintained to continue the loop where it left off
   if PULLBITS() returns in the loop.  For example, want, need, and keep
   would all have to actually be part of the saved state in case PULLBITS()
   returns:

    case STATEw:
        while (want < need) {
            PULLBITS(n);
            keep[want++] = BITS(n);
            DROPBITS(n);
        }
        state = STATEx;
    case STATEx:

   As shown above, if the next state is also the next case, then the break
   is omitted.

   A state may also return if there is not enough output space available to
   complete that state.  Those states are copying stored data, writing a
   literal byte, and copying a matching string.



   In this implementation, the flush parameter of inflate() only affects the
   return code (per zlib.h).  inflate() always writes as much as possible to
   strm->next_out, given the space available and the provided input--the effect
   documented in zlib.h of Z_SYNC_FLUSH.  Furthermore, inflate() always defers
   the allocation of and copying into a sliding window until necessary, which
   provides the effect documented in zlib.h for Z_FINISH when the entire input
   stream available.  So the only thing the flush parameter actually does is:
   when flush is set to Z_FINISH, inflate() cannot return Z_OK.  Instead it
   will return Z_BUF_ERROR if it has not reached the end of the stream.
 */
typedef {
    uint32_t hold; /* bit buffer */
    unsigned in, out; /* save starting available input and output */
    unsigned have, left; /* available input and output */
    unsigned copy;              /* number of stored or match bytes to copy */
    unsigned len;               /* length to copy for repeats, bits to drop */
    uint8_t *put;     /* next output */
    inflate_state *state;
    z_stream *strm;
    code_t here; /* current decoding table entry */
    code_t last; /* parent table entry */
    int ret;                    /* return code */
    uint8_t *next;    /* next input */
    unsigned bits;              /* bits in bit buffer */
    uint8_t *from;    /* where to copy match bytes from */
    int flush;
} ctx_t;
pub int inflate(z_stream *strm, int flush) {
    ctx_t ctx = {
        .strm = strm,
        .flush = flush
    };
    if (inflateStateCheck(strm) || !strm->next_out || (!strm->next_in && strm->avail_in != 0)) {
        return Z_STREAM_ERROR;
    }

    ctx.state = strm->state;
    if (ctx.state->mode == TYPE) {
        ctx.state->mode = TYPEDO;      /* skip check */
    }
    LOAD(ctx);
    ctx.in = ctx.have;
    ctx.out = ctx.left;
    ctx.ret = Z_OK;
    while (true) {
        int r = 0;
        switch (ctx.state->mode) {
            case HEAD:      { r = st_head(ctx); }
            case FLAGS:     { r = st_flags(ctx); }
            case TIME:      { r = st_time(ctx); }
            case OS:        { r = st_os(ctx); }
            case EXLEN:     { r = st_exlen(ctx); }
            case EXTRA:     { r = st_extra(ctx); }
            case NAME:      { r = st_name(ctx); }
            case COMMENT:   { r = st_comment(ctx); }
            case HCRC:      { r = st_hcrc(ctx); }
            case DICTID:    { r = st_dictid(ctx); }
            case DICT:      { r = st_dict(ctx); }
            case TYPE:      { r = st_type(ctx); }
            case TYPEDO:    { r = st_typedo(ctx); }
            case STORED:    { r = st_stored(ctx); }
            case COPY_: {
                ctx.state->mode = COPY;
                r = st_copy(ctx);
            }
            case COPY:      { r = st_copy(ctx); }
            case TABLE:     { r = st_table(ctx); }
            case LENLENS:   { r = st_lenlens(ctx); }
            case CODELENS:  { r = st_codelens(ctx); }
            case LEN_: {
                ctx.state->mode = LEN;
                r = st_len(ctx);
            }
            case LEN:       { r = st_len(ctx); }
            case LENEXT:    { r = st_lenext(ctx); }
            case DIST:      { r = st_dist(ctx); }
            case DISTEXT:   { r = st_distext(ctx); }
            case MATCH:     { r = st_match(ctx); }
            case LIT:       { r = st_lit(ctx); }
            case CHECK:     { r = st_check(ctx); }
            case LENGTH:    { r = st_length(ctx); }
            case DONE: {
                ctx.ret = Z_STREAM_END;
                r = inf_leave(&ctx);
            }
            case BAD: {
                ctx.ret = Z_DATA_ERROR;
                r = inf_leave(&ctx);
            }
            case MEM: { r = Z_MEM_ERROR; }
            case SYNC: { r = Z_STREAM_ERROR; }
            default: { r = Z_STREAM_ERROR; }
        }
        if (r == BREAK) break;
        if (r == CONTINUE) continue;
        return r;
    }
    return inf_leave(&ctx);
}

enum {
    BREAK = 123456789,
    CONTINUE = 987654321,
    FALLTHROUGH = 123123,
};

int st_head(ctx_t *ctx) {
    if (ctx->state->wrap == 0) {
        ctx->state->mode = TYPEDO;
        return BREAK;
    }
    if (!PULLBITS(ctx, 16)) {
        return inf_leave(ctx);
    }
    if ((ctx->state->wrap & 2) && ctx->hold == GZIP_MAGIC) {
        /* gzip header */
        if (ctx->state->wbits == 0) {
            ctx->state->wbits = 15;
        }
        ctx->state->check = crc32.init();
        CRC2(ctx);
        CLEARBITS(ctx);
        ctx->state->mode = FLAGS;
        return BREAK;
    }
    if (ctx->state->head != NULL) {
        ctx->state->head->done = -1;
    }
    if (!(ctx->state->wrap & 1) ||   /* check if zlib header allowed */
        ((BITS(ctx, 8) << 8) + (ctx->hold >> 8)) % 31) {
        ctx->strm->msg = "incorrect header check";
        ctx->state->mode = BAD;
        return BREAK;
    }
    if (BITS(ctx, 4) != deflate.Z_DEFLATED) {
        ctx->strm->msg = "unknown compression method";
        ctx->state->mode = BAD;
        return BREAK;
    }
    DROPBITS(ctx, 4);
    ctx->len = BITS(ctx, 4) + 8;
    if (ctx->state->wbits == 0) {
        ctx->state->wbits = ctx->len;
    }
    if (ctx->len > 15 || ctx->len > ctx->state->wbits) {
        ctx->strm->msg = "invalid window size";
        ctx->state->mode = BAD;
        return BREAK;
    }
    ctx->state->dmax = 1U << ctx->len;
    ctx->state->flags = 0;               /* indicate zlib header */
    trace.Tracev(stderr, "inflate:   zlib header ok\n");
    ctx->state->check = adler32.init();
    ctx->strm->adler = adler32.init();
    if (ctx->hold & 0x200) {
        ctx->state->mode = DICTID;
    } else {
        ctx->state->mode = TYPE;
    }
    CLEARBITS(ctx);
    return CONTINUE;
}

int st_flags(ctx_t *ctx) {
    if (!PULLBITS(ctx, 16)) {
        return inf_leave(ctx);
    }
    ctx->state->flags = (int)(ctx->hold);
    if ((ctx->state->flags & 0xff) != deflate.Z_DEFLATED) {
        ctx->strm->msg = "unknown compression method";
        ctx->state->mode = BAD;
        return BREAK;
    }
    if (ctx->state->flags & 0xe000) {
        ctx->strm->msg = "unknown header flags set";
        ctx->state->mode = BAD;
        return BREAK;
    }
    if (ctx->state->head != NULL) {
        ctx->state->head->text = (int)((ctx->hold >> 8) & 1);
    }
    if ((ctx->state->flags & 0x0200) && should_validate_check_value(ctx->state)) {
        CRC2(ctx);
    }
    CLEARBITS(ctx);
    ctx->state->mode = TIME;
    return st_time(ctx);
}

int st_time(ctx_t *ctx) {
    if (!PULLBITS(ctx, 32)) {
        return inf_leave(ctx);
    }
    // Save the timestamp.
    if (ctx->state->head != NULL) {
        ctx->state->head->time = ctx->hold;
    }

    if ((ctx->state->flags & 0x0200) && should_validate_check_value(ctx->state)) {
        uint32_t word = ctx->hold;
        uint8_t hbuf[] = {
            word & 0xFF,
            (word >> 8) & 0xFF,
            (word >> 16) & 0xFF,
            (word >> 24) & 0xFF
        };
        ctx->state->check = crc32.crc32(ctx->state->check, hbuf, 4);
    }
    CLEARBITS(ctx);
    ctx->state->mode = OS;
    return st_os(ctx);
}

int st_os(ctx_t *ctx) {
    if (!PULLBITS(ctx, 16)) {
        return inf_leave(ctx);
    }
    if ((ctx->state->flags & 0x0200) && should_validate_check_value(ctx->state)) {
        CRC2(ctx);
    }

    if (ctx->state->head != NULL) {
        ctx->state->head->xflags = (int)(ctx->hold & 0xff);
        ctx->state->head->os = (int)(ctx->hold >> 8);
    }
    CLEARBITS(ctx);
    ctx->state->mode = EXLEN;
    return st_exlen(ctx);
}

int st_exlen(ctx_t *ctx) {
    if (ctx->state->flags & 0x0400) {
        if (!PULLBITS(ctx, 16)) {
            return inf_leave(ctx);
        }
        if ((ctx->state->flags & 0x0200) && should_validate_check_value(ctx->state)) {
            CRC2(ctx);
        }
        ctx->state->length = (unsigned)(ctx->hold);
        if (ctx->state->head != NULL) {
            ctx->state->head->extra_len = (unsigned)ctx->hold;
        }
        CLEARBITS(ctx);
    }
    else if (ctx->state->head != NULL) {
        ctx->state->head->extra = NULL;
    }
    ctx->state->mode = EXTRA;
    return st_extra(ctx);
}

int st_extra(ctx_t *ctx) {
    int is400 = ctx->state->flags & 0x0400;
    if (!is400) {
        ctx->state->length = 0;
        ctx->state->mode = NAME;
        return st_name(ctx);
    }
    
    ctx->copy = ctx->state->length;
    if (ctx->copy > ctx->have) {
        ctx->copy = ctx->have;
    }
    if (ctx->copy) {
        if (ctx->state->head && ctx->state->head->extra &&
            (ctx->len = ctx->state->head->extra_len - ctx->state->length) < ctx->state->head->extra_max)
        {
            int x;
            if (ctx->len + ctx->copy > ctx->state->head->extra_max) {
                x = ctx->state->head->extra_max - ctx->len;
            } else {
                x = ctx->copy;
            }
            memcpy(ctx->state->head->extra + ctx->len, ctx->next, x);
        }
        if ((ctx->state->flags & 0x0200) && should_validate_check_value(ctx->state)) {
            ctx->state->check = crc32.crc32(ctx->state->check, ctx->next, ctx->copy);
        }
        ctx->have -= ctx->copy;
        ctx->next += ctx->copy;
        ctx->state->length -= ctx->copy;
    }
    if (ctx->state->length) {
        return inf_leave(ctx);
    }
    ctx->state->length = 0;
    ctx->state->mode = NAME;
    return st_name(ctx);
}

int st_name(ctx_t *ctx) {
    if (ctx->state->flags & 0x0800) {
        if (ctx->have == 0) {
            return inf_leave(ctx);
        }
        ctx->copy = 0;
        while (true) {
            ctx->len = (unsigned)(ctx->next[ctx->copy++]);
            if (ctx->state->head && ctx->state->head->name && ctx->state->length < ctx->state->head->name_max) {
                ctx->state->head->name[ctx->state->length++] = (uint8_t)ctx->len;
            }
            bool cont = (ctx->len && ctx->copy < ctx->have);
            if (!cont) break;
        }
        if ((ctx->state->flags & 0x0200) && should_validate_check_value(ctx->state)) {
            ctx->state->check = crc32.crc32(ctx->state->check, ctx->next, ctx->copy);
        }
        ctx->have -= ctx->copy;
        ctx->next += ctx->copy;
        if (ctx->len) {
            return inf_leave(ctx);
        }
    }
    else if (ctx->state->head) {
        ctx->state->head->name = NULL;
    }
    ctx->state->length = 0;
    ctx->state->mode = COMMENT;
    return st_comment(ctx);
}

int st_comment(ctx_t *ctx) {
    if (ctx->state->flags & 0x1000) {
        if (ctx->have == 0) {
            return inf_leave(ctx);
        }
        ctx->copy = 0;
        while (true) {
            ctx->len = (unsigned)(ctx->next[ctx->copy++]);
            if (ctx->state->head != NULL &&
                    ctx->state->head->comment != NULL &&
                    ctx->state->length < ctx->state->head->comm_max)
                ctx->state->head->comment[ctx->state->length++] = (uint8_t)ctx->len;
            bool cont = (ctx->len && ctx->copy < ctx->have);
            if (!cont) break;
        }
        if ((ctx->state->flags & 0x0200) && should_validate_check_value(ctx->state)) {
            ctx->state->check = crc32.crc32(ctx->state->check, ctx->next, ctx->copy);
        }
        ctx->have -= ctx->copy;
        ctx->next += ctx->copy;
        if (ctx->len) {
            return inf_leave(ctx);
        }
    }
    else if (ctx->state->head) {
        ctx->state->head->comment = NULL;
    }
    ctx->state->mode = HCRC;
    return st_hcrc(ctx);
}

int st_hcrc(ctx_t *ctx) {
    if (ctx->state->flags & 0x0200) {
        if (!PULLBITS(ctx, 16)) {
            return inf_leave(ctx);
        }
        if (should_validate_check_value(ctx->state) && ctx->hold != (ctx->state->check & 0xffff)) {
            ctx->strm->msg = "header crc mismatch";
            ctx->state->mode = BAD;
            return BREAK;
        }
        CLEARBITS(ctx);
    }
    if (ctx->state->head) {
        ctx->state->head->hcrc = (int)((ctx->state->flags >> 9) & 1);
        ctx->state->head->done = 1;
    }
    ctx->strm->adler = ctx->state->check = crc32.init();
    ctx->state->mode = TYPE;
    return CONTINUE;
}

int st_dictid(ctx_t *ctx) {
    if (!PULLBITS(ctx, 32)) {
        return inf_leave(ctx);
    }
    ctx->state->check = ZSWAP32(ctx->hold);
    ctx->strm->adler = ctx->state->check;
    CLEARBITS(ctx);
    ctx->state->mode = DICT;
    return st_dict(ctx);
}

int st_dict(ctx_t *ctx) {
    if (ctx->state->havedict == 0) {
        RESTORE(ctx);
        return Z_NEED_DICT;
    }
    ctx->strm->adler = ctx->state->check = adler32.init();
    ctx->state->mode = TYPE;
    return st_type(ctx);
}

int st_type(ctx_t *ctx) {
    if (ctx->flush == Z_BLOCK || ctx->flush == Z_TREES) {
        return inf_leave(ctx);
    }
    return st_typedo(ctx);
}

int st_typedo(ctx_t *ctx) {
    if (ctx->state->last) {
        /* Remove zero to seven bits as needed to go to a byte boundary */
        ctx->hold >>= ctx->bits & 7;
        ctx->bits -= ctx->bits & 7;
        ctx->state->mode = CHECK;
        return BREAK;
    }
    if (!PULLBITS(ctx, 3)) {
        return inf_leave(ctx);
    }
    ctx->state->last = BITS(ctx, 1);
    DROPBITS(ctx, 1);
    switch (BITS(ctx, 2)) {
        case 0: {                             /* stored block */
            if (ctx->state->last) {
                trace.Tracev(stderr, "inflate:     stored block (last)\n");
            } else {
                trace.Tracev(stderr, "inflate:     stored block\n");
            }
            ctx->state->mode = STORED;
        }
        case 1: {                             /* fixed block */
            fixedtables(ctx->state);
            if (ctx->state->last) {
                trace.Tracev(stderr, "inflate:     fixed codes block (last)\n");
            } else {
                trace.Tracev(stderr, "inflate:     fixed codes block\n");
            }
            ctx->state->mode = LEN_;             /* decode codes */
            if (ctx->flush == Z_TREES) {
                DROPBITS(ctx, 2);
                return inf_leave(ctx);
            }
        }
        case 2: {                             /* dynamic block */
            if (ctx->state->last) {
                trace.Tracev(stderr, "inflate:     dynamic codes block (last)\n");
            } else {
                trace.Tracev(stderr, "inflate:     dynamic codes block\n");
            }
            ctx->state->mode = TABLE;
        }
        case 3: {
            ctx->strm->msg = "invalid block type";
            ctx->state->mode = BAD;
        }
    }
    DROPBITS(ctx, 2);
    return CONTINUE;
}

int st_stored(ctx_t *ctx) {
    /* go to byte boundary */
    /* Remove zero to seven bits as needed to go to a byte boundary */
    ctx->hold >>= ctx->bits & 7;
    ctx->bits -= ctx->bits & 7;
    if (!PULLBITS(ctx, 32)) {
        return inf_leave(ctx);
    }
    if ((ctx->hold & 0xffff) != ((ctx->hold >> 16) ^ 0xffff)) {
        ctx->strm->msg = "invalid stored block lengths";
        ctx->state->mode = BAD;
        return BREAK;
    }
    ctx->state->length = (unsigned) ctx->hold & 0xffff;
    trace.Tracev(stderr, "inflate:       stored length %u\n", ctx->state->length);
    CLEARBITS(ctx);
    ctx->state->mode = COPY_;
    if (ctx->flush == Z_TREES) {
        return inf_leave(ctx);
    }
    ctx->state->mode = COPY;
    return st_copy(ctx);
}

int st_copy(ctx_t *ctx) {
    ctx->copy = ctx->state->length;
    if (ctx->copy) {
        if (ctx->copy > ctx->have) {
            ctx->copy = ctx->have;
        }
        if (ctx->copy > ctx->left) {
            ctx->copy = ctx->left;
        }
        if (ctx->copy == 0) {
            return inf_leave(ctx);
        }
        memcpy(ctx->put, ctx->next, ctx->copy);
        ctx->have -= ctx->copy;
        ctx->next += ctx->copy;
        ctx->left -= ctx->copy;
        ctx->put += ctx->copy;
        ctx->state->length -= ctx->copy;
        break;
    }
    trace.Tracev(stderr, "inflate:       stored end\n");
    ctx->state->mode = TYPE;
    return CONTINUE;
}

int st_table(ctx_t *ctx) {
    if (!PULLBITS(ctx, 14)) {
        return inf_leave(ctx);
    }
    ctx->state->nlen = BITS(ctx, 5) + 257;
    DROPBITS(ctx, 5);
    ctx->state->ndist = BITS(ctx, 5) + 1;
    DROPBITS(ctx, 5);
    ctx->state->ncode = BITS(ctx, 4) + 4;
    DROPBITS(ctx, 4);
    if (!PKZIP_BUG_WORKAROUND) {
        if (ctx->state->nlen > 286 || ctx->state->ndist > 30) {
            ctx->strm->msg = "too many length or distance symbols";
            ctx->state->mode = BAD;
            return BREAK;
        }
    }
    trace.Tracev(stderr, "inflate:       table sizes ok\n");
    ctx->state->have = 0;
    ctx->state->mode = LENLENS;
    return st_lenlens(ctx);
}

int st_lenlens(ctx_t *ctx) {
    while (ctx->state->have < ctx->state->ncode) {
        if (!PULLBITS(ctx, 3)) {
            return inf_leave(ctx);
        }
        ctx->state->lens[order[ctx->state->have++]] = (uint16_t)BITS(ctx, 3);
        DROPBITS(ctx, 3);
    }
    while (ctx->state->have < 19) {
        ctx->state->lens[order[ctx->state->have++]] = 0;
    }
    ctx->state->next = ctx->state->codes;
    ctx->state->lencode = ctx->state->next;
    ctx->state->lenbits = 7;
    ctx->ret = inflate_table(CODES, ctx->state->lens, 19,
        &(ctx->state->next),
        &(ctx->state->lenbits),
        ctx->state->work);
    if (ctx->ret) {
        ctx->strm->msg = "invalid code lengths set";
        ctx->state->mode = BAD;
        return BREAK;
    }
    trace.Tracev(stderr, "inflate:       code lengths ok\n");
    ctx->state->have = 0;
    ctx->state->mode = CODELENS;
    return st_codelens(ctx);
}

int st_codelens(ctx_t *ctx) {
    while (ctx->state->have < ctx->state->nlen + ctx->state->ndist) {
        while (true) {
            ctx->here = ctx->state->lencode[BITS(ctx, ctx->state->lenbits)];
            if (ctx->here.bits <= ctx->bits) {
                break;
            }
            if (!PULLBYTE(ctx)) {
                return inf_leave(ctx);
            }
        }
        if (ctx->here.val < 16) {
            DROPBITS(ctx, ctx->here.bits);
            ctx->state->lens[ctx->state->have++] = ctx->here.val;
        }
        else {
            if (ctx->here.val == 16) {
                if (!PULLBITS(ctx, ctx->here.bits + 2)) {
                    return inf_leave(ctx);
                }
                DROPBITS(ctx, ctx->here.bits);
                if (ctx->state->have == 0) {
                    ctx->strm->msg = "invalid bit length repeat";
                    ctx->state->mode = BAD;
                    break;
                }
                ctx->len = ctx->state->lens[ctx->state->have - 1];
                ctx->copy = 3 + BITS(ctx, 2);
                DROPBITS(ctx, 2);
            }
            else if (ctx->here.val == 17) {
                if (!PULLBITS(ctx, ctx->here.bits + 3)) {
                    return inf_leave(ctx);
                }
                DROPBITS(ctx, ctx->here.bits);
                ctx->len = 0;
                ctx->copy = 3 + BITS(ctx, 3);
                DROPBITS(ctx, 3);
            }
            else {
                if (!PULLBITS(ctx, ctx->here.bits + 7)) {
                    return inf_leave(ctx);
                }
                DROPBITS(ctx, ctx->here.bits);
                ctx->len = 0;
                ctx->copy = 11 + BITS(ctx, 7);
                DROPBITS(ctx, 7);
            }
            if (ctx->state->have + ctx->copy > ctx->state->nlen + ctx->state->ndist) {
                ctx->strm->msg = "invalid bit length repeat";
                ctx->state->mode = BAD;
                break;
            }
            while (ctx->copy--) {
                ctx->state->lens[ctx->state->have++] = ctx->len;
            }
        }
    }

    /* handle error breaks in while */
    if (ctx->state->mode == BAD) break;

    /* check for end-of-block code (better have one) */
    if (ctx->state->lens[256] == 0) {
        ctx->strm->msg = "invalid code -- missing end-of-block";
        ctx->state->mode = BAD;
        break;
    }

    /* build code tables -- note: do not change the lenbits or distbits
        values here (9 and 6) without reading the comments in inftrees.h
        concerning the ENOUGH constants, which depend on those values */
    ctx->state->next = ctx->state->codes;
    ctx->state->lencode = ctx->state->next;
    ctx->state->lenbits = 9;
    ctx->ret = inflate_table(LENS, ctx->state->lens, ctx->state->nlen, &(ctx->state->next),
                        &(ctx->state->lenbits), ctx->state->work);
    if (ctx->ret) {
        ctx->strm->msg = "invalid literal/lengths set";
        ctx->state->mode = BAD;
        break;
    }
    ctx->state->distcode = ctx->state->next;
    ctx->state->distbits = 6;
    ctx->ret = inflate_table(DISTS, ctx->state->lens + ctx->state->nlen, ctx->state->ndist,
                    &(ctx->state->next), &(ctx->state->distbits), ctx->state->work);
    if (ctx->ret) {
        ctx->strm->msg = "invalid distances set";
        ctx->state->mode = BAD;
        break;
    }
    trace.Tracev(stderr, "inflate:       codes ok\n");
    ctx->state->mode = LEN_;
    if (ctx->flush == Z_TREES) {
        return inf_leave(ctx);
    }
    ctx->state->mode = LEN;
    return st_len(ctx);
}

int st_len(ctx_t *ctx) {
    if (ctx->have >= 6 && ctx->left >= 258) {
        RESTORE(ctx);
        inflate_fast(ctx->strm, ctx->out);
        LOAD(ctx);
        if (ctx->state->mode == TYPE) {
            ctx->state->back = -1;
        }
        return BREAK;
    }
    ctx->state->back = 0;
    while (true) {
        ctx->here = ctx->state->lencode[BITS(ctx, ctx->state->lenbits)];
        if ((unsigned)(ctx->here.bits) <= ctx->bits) {
            break;
        }
        if (!PULLBYTE(ctx)) {
            return inf_leave(ctx);
        }
    }
    if (ctx->here.op && (ctx->here.op & 0xf0) == 0) {
        ctx->last = ctx->here;
        while (true) {
            int x = BITS(ctx, ctx->last.bits + ctx->last.op) >> ctx->last.bits;
            ctx->here = ctx->state->lencode[ctx->last.val + x];
            if ((unsigned)(ctx->last.bits + ctx->here.bits) <= ctx->bits) break;
            if (!PULLBYTE(ctx)) {
                return inf_leave(ctx);
            }
        }
        DROPBITS(ctx, ctx->last.bits);
        ctx->state->back += ctx->last.bits;
    }
    DROPBITS(ctx, ctx->here.bits);
    ctx->state->back += ctx->here.bits;
    ctx->state->length = (unsigned)ctx->here.val;
    if ((int)(ctx->here.op) == 0) {
        if (ctx->here.val >= 0x20 && ctx->here.val < 0x7f) {
            trace.Tracevv(stderr, "inflate:         literal '%c'\n", ctx->here.val);
        } else {
            trace.Tracevv(stderr, "inflate:         literal 0x%02x\n", ctx->here.val);
        }
        ctx->state->mode = LIT;
        break;
    }
    if (ctx->here.op & 32) {
        trace.Tracevv(stderr, "inflate:         end of block\n");
        ctx->state->back = -1;
        ctx->state->mode = TYPE;
        break;
    }
    if (ctx->here.op & 64) {
        ctx->strm->msg = "invalid literal/length code";
        ctx->state->mode = BAD;
        break;
    }
    ctx->state->extra = (unsigned)(ctx->here.op) & 15;
    ctx->state->mode = LENEXT;
    return st_lenext(ctx);
}

int st_lenext(ctx_t *ctx) {
    if (ctx->state->extra) {
        if (!PULLBITS(ctx, ctx->state->extra)) {
            return inf_leave(ctx);
        }
        ctx->state->length += BITS(ctx, ctx->state->extra);
        DROPBITS(ctx, ctx->state->extra);
        ctx->state->back += ctx->state->extra;
    }
    trace.Tracevv(stderr, "inflate:         length %u\n", ctx->state->length);
    ctx->state->was = ctx->state->length;
    ctx->state->mode = DIST;
    return st_dist(ctx);
}

int st_dist(ctx_t *ctx) {
    while (true) {
        ctx->here = ctx->state->distcode[BITS(ctx, ctx->state->distbits)];
        if ((unsigned)(ctx->here.bits) <= ctx->bits) break;
        if (!PULLBYTE(ctx)) {
            return inf_leave(ctx);
        }
    }
    if ((ctx->here.op & 0xf0) == 0) {
        ctx->last = ctx->here;
        while (true) {
            int x = BITS(ctx, ctx->last.bits + ctx->last.op) >> ctx->last.bits;
            ctx->here = ctx->state->distcode[ctx->last.val + x];
            if ((unsigned)(ctx->last.bits + ctx->here.bits) <= ctx->bits) {
                break;
            }
            if (!PULLBYTE(ctx)) {
                return inf_leave(ctx);
            }
        }
        DROPBITS(ctx, ctx->last.bits);
        ctx->state->back += ctx->last.bits;
    }
    DROPBITS(ctx, ctx->here.bits);
    ctx->state->back += ctx->here.bits;
    if (ctx->here.op & 64) {
        ctx->strm->msg = "invalid distance code";
        ctx->state->mode = BAD;
        return BREAK;
    }
    ctx->state->offset = (unsigned)ctx->here.val;
    ctx->state->extra = (unsigned)(ctx->here.op) & 15;
    ctx->state->mode = DISTEXT;
    return st_distext(ctx);
}

int st_distext(ctx_t *ctx) {
    if (ctx->state->extra) {
        if (!PULLBITS(ctx, ctx->state->extra)) {
            return inf_leave(ctx);
        }
        ctx->state->offset += BITS(ctx, ctx->state->extra);
        DROPBITS(ctx, ctx->state->extra);
        ctx->state->back += ctx->state->extra;
    }
    trace.Tracevv(stderr, "inflate:         distance %u\n", ctx->state->offset);
    ctx->state->mode = MATCH;
    return st_match(ctx);
}

int st_match(ctx_t *ctx) {
    if (ctx->left == 0) {
        return inf_leave(ctx);
    }
    ctx->copy = ctx->out - ctx->left;
    if (ctx->state->offset > ctx->copy) {
        /* copy from window */
        ctx->copy = ctx->state->offset - ctx->copy;
        if (ctx->copy > ctx->state->whave) {
            if (ctx->state->sane) {
                ctx->strm->msg = "invalid distance too far back";
                ctx->state->mode = BAD;
                return BREAK;
            }
            trace.Trace(stderr, "inflate.c too far\n");
            ctx->copy -= ctx->state->whave;
            if (ctx->copy > ctx->state->length) {
                ctx->copy = ctx->state->length;
            }
            if (ctx->copy > ctx->left) {
                ctx->copy = ctx->left;
            }
            ctx->left -= ctx->copy;
            ctx->state->length -= ctx->copy;
            while (true) {
                *ctx->put++ = 0;
                if (!--ctx->copy) {
                    break;
                }
            }
            if (ctx->state->length == 0) {
                ctx->state->mode = LEN;
            }
            return BREAK;
        }
        if (ctx->copy > ctx->state->wnext) {
            ctx->copy -= ctx->state->wnext;
            ctx->from = ctx->state->window + (ctx->state->wsize - ctx->copy);
        }
        else {
            ctx->from = ctx->state->window + (ctx->state->wnext - ctx->copy);
        }
        if (ctx->copy > ctx->state->length) {
            ctx->copy = ctx->state->length;
        }
    }
    else {                              /* ctx->copy from output */
        ctx->from = ctx->put - ctx->state->offset;
        ctx->copy = ctx->state->length;
    }
    if (ctx->copy > ctx->left) {
        ctx->copy = ctx->left;
    }
    ctx->left -= ctx->copy;
    ctx->state->length -= ctx->copy;
    while (true) {
        *ctx->put++ = *ctx->from++;
        bool cont = (--ctx->copy);
        if (!cont) break;
    }
    if (ctx->state->length == 0) {
        ctx->state->mode = LEN;
    }
    return CONTINUE;
}

int st_lit(ctx_t *ctx) {
    if (ctx->left == 0) {
        return inf_leave(ctx);
    }
    *(ctx->put) = ctx->state->length;
    ctx->put++;
    ctx->left--;
    ctx->state->mode = LEN;
    return CONTINUE;
}

int st_check(ctx_t *ctx) {
    if (ctx->state->wrap) {
        if (!PULLBITS(ctx, 32)) {
            return inf_leave(ctx);
        }
        ctx->out -= ctx->left;
        ctx->strm->total_out += ctx->out;
        ctx->state->total += ctx->out;
        if (should_validate_check_value(ctx->state) && ctx->out) {
            ctx->strm->adler = ctx->state->check = UPDATE_CHECK(ctx, ctx->put - ctx->out, ctx->out);
        }
        ctx->out = ctx->left;
        int x;
        if (ctx->state->flags) {
            x = ctx->hold;
        } else {
            x = ZSWAP32(ctx->hold);
        }
        if (should_validate_check_value(ctx->state) && x != ctx->state->check) {
            ctx->strm->msg = "incorrect data check";
            ctx->state->mode = BAD;
            return BREAK;
        }
        CLEARBITS(ctx);
        trace.Tracev(stderr, "inflate:   check matches trailer\n");
    }
    ctx->state->mode = LENGTH;
    return st_length(ctx);
}

int st_length(ctx_t *ctx) {
    if (ctx->state->wrap && ctx->state->flags) {
        if (!PULLBITS(ctx, 32)) {
            return inf_leave(ctx);
        }
        if (should_validate_check_value(ctx->state) && ctx->hold != (ctx->state->total & 0xffffffff)) {
            ctx->strm->msg = "incorrect length check";
            ctx->state->mode = BAD;
            return BREAK;
        }
        CLEARBITS(ctx);
        trace.Tracev(stderr, "inflate:   length matches trailer\n");
    }
    ctx->state->mode = DONE;
    ctx->ret = Z_STREAM_END;
    return inf_leave(ctx);
}

/*
       Return from inflate(), updating the total counts and the check value.
       If there was no progress during the inflate() call, return a buffer
       error.  Call updatewindow() to create and/or update the window state.
       Note: a memory error from inflate() is non-recoverable.
     */
// When returning, a "return inf_leave(ctx)" is used to update the total counters,
//    update the check value, and determine whether any progress has been made
//    during that inflate() call in order to return the proper return code.
//    Progress is defined as a change in either strm->avail_in or strm->avail_out.
//    When there is a window, return inf_leave(ctx) will update the window with the last
//    output written.  If a return inf_leave(ctx) occurs in the middle of decompression
//    and there is no window currently, return inf_leave(ctx) will create one and copy
//    output to the window for the next call of inflate().
int inf_leave(ctx_t *ctx) {
    RESTORE(ctx);
    if (ctx->state->wsize
        || (ctx->out != ctx->strm->avail_out
            && ctx->state->mode < BAD
            && (ctx->state->mode < CHECK || ctx->flush != Z_FINISH))
    ) {
        if (updatewindow(ctx->strm, ctx->strm->next_out, ctx->out - ctx->strm->avail_out)) {
            ctx->state->mode = MEM;
            return Z_MEM_ERROR;
        }
    }
    ctx->in -= ctx->strm->avail_in;
    ctx->out -= ctx->strm->avail_out;
    ctx->strm->total_in += ctx->in;
    ctx->strm->total_out += ctx->out;
    ctx->state->total += ctx->out;
    if (should_validate_check_value(ctx->state) && ctx->out) {
        ctx->strm->adler = ctx->state->check = UPDATE_CHECK(ctx, ctx->strm->next_out - ctx->out, ctx->out);
    }
    int x1 = 0;
    if (ctx->state->last) {
        x1 = 64;
    }
    int x2 = 0;
    if (ctx->state->mode == TYPE) {
        x2 = 128;
    }
    int x3 = 0;
    if (ctx->state->mode == LEN_ || ctx->state->mode == COPY_) {
        x3 = 256;
    }
    ctx->strm->data_type = (int)ctx->state->bits + x1 + x2 + x3;
    if (((ctx->in == 0 && ctx->out == 0) || ctx->flush == Z_FINISH) && ctx->ret == Z_OK) {
        ctx->ret = Z_BUF_ERROR;
    }
    return ctx->ret;
}

/*
 * Frees all dynamically allocated data structures for this stream.
 * Discards any unprocessed input and does not flush any pending output.
 * Returns Z_OK if success, or Z_STREAM_ERROR if the
 * stream state was inconsistent.
 */
pub int inflateEnd(z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }
    inflate_state *state = strm->state;
    if (state->window != NULL) {
        free(state->window);
    }
    free(strm->state);
    strm->state = NULL;
    trace.Tracev(stderr, "inflate: end\n");
    return Z_OK;
}

/*
     Returns the sliding dictionary being maintained by inflate.  dictLength is
   set to the number of bytes in the dictionary, and that many bytes are copied
   to dictionary.  dictionary must have enough space, where 32768 bytes is
   always enough.  If inflateGetDictionary() is called with dictionary equal to
   NULL, then only the dictionary length is returned, and nothing is copied.
   Similarly, if dictLength is NULL, then it is not set.

     inflateGetDictionary returns Z_OK on success, or Z_STREAM_ERROR if the
   stream state is inconsistent.
*/
pub int inflateGetDictionary(
    z_stream *strm,
    uint8_t *dictionary,
    uint32_t *dictLength
) {
    if (inflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }
    inflate_state *state = (inflate_state *)strm->state;

    /* copy dictionary */
    if (state->whave && dictionary != NULL) {
        memcpy(dictionary, state->window + state->wnext, state->whave - state->wnext);
        memcpy(dictionary + state->whave - state->wnext, state->window, state->wnext);
    }
    if (dictLength != NULL) {
        *dictLength = state->whave;
    }
    return Z_OK;
}

/*
     Initializes the decompression dictionary from the given uncompressed byte
   sequence.  This function must be called immediately after a call of inflate,
   if that call returned stream.Z_NEED_DICT.  The dictionary chosen by the compressor
   can be determined from the Adler-32 value returned by that call of inflate.
   The compressor and decompressor must use exactly the same dictionary (see
   deflateSetDictionary).  For raw inflate, this function can be called at any
   time to set the dictionary.  If the provided dictionary is smaller than the
   window and there is already data in the window, then the provided dictionary
   will amend what's there.  The application must insure that the dictionary
   that was used for compression is provided.

     inflateSetDictionary returns Z_OK if success, Z_STREAM_ERROR if a
   parameter is invalid (e.g.  dictionary being NULL) or the stream state is
   inconsistent, Z_DATA_ERROR if the given dictionary doesn't match the
   expected one (incorrect Adler-32 value).  inflateSetDictionary does not
   perform any decompression: this will be done by subsequent calls of
   inflate().
*/
pub int inflateSetDictionary(
    z_stream *strm,
    const uint8_t *dictionary,
    uint32_t dictLength
) {
    inflate_state *state;
    uint32_t dictid;
    int ret;

    /* check state */
    if (inflateStateCheck(strm)) return Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    if (state->wrap != 0 && state->mode != DICT)
        return Z_STREAM_ERROR;

    /* check for correct dictionary identifier */
    if (state->mode == DICT) {
        dictid = adler32.adler32(0L, NULL, 0);
        dictid = adler32.adler32(dictid, dictionary, dictLength);
        if (dictid != state->check)
            return Z_DATA_ERROR;
    }

    /* copy dictionary to window using updatewindow(), which will amend the
       existing dictionary if appropriate */
    ret = updatewindow(strm, dictionary + dictLength, dictLength);
    if (ret) {
        state->mode = MEM;
        return Z_MEM_ERROR;
    }
    state->havedict = 1;
    trace.Tracev(stderr, "inflate:   dictionary set\n");
    return Z_OK;
}


/*
     inflateGetHeader() requests that gzip header information be stored in the
   provided gz_header structure.  inflateGetHeader() may be called after
   inflateInit2() or inflateReset(), and before the first call of inflate().
   As inflate() processes the gzip stream, head->done is zero until the header
   is completed, at which time head->done is set to one.  If a zlib stream is
   being decoded, then head->done is set to -1 to indicate that there will be
   no gzip header information forthcoming.  Note that Z_BLOCK or Z_TREES can be
   used to force inflate() to return immediately after header processing is
   complete and before any actual data is decompressed.

     The text, time, xflags, and os fields are filled in with the gzip header
   contents.  hcrc is set to true if there is a header CRC.  (The header CRC
   was valid if done is set to one.) If extra is not NULL, then extra_max
   contains the maximum number of bytes to write to extra.  Once done is true,
   extra_len contains the actual extra field length, and extra contains the
   extra field, or that field truncated if extra_max is less than extra_len.
   If name is not NULL, then up to name_max characters are written there,
   terminated with a zero unless the length is greater than name_max.  If
   comment is not NULL, then up to comm_max characters are written there,
   terminated with a zero unless the length is greater than comm_max.  When any
   of extra, name, or comment are not NULL and the respective field is not
   present in the header, then that field is set to NULL to signal its
   absence.  This allows the use of deflateSetHeader() with the returned
   structure to duplicate the header.  However if those fields are set to
   allocated memory, then the application will need to save those pointers
   elsewhere so that they can be eventually freed.

     If inflateGetHeader is not used, then the header information is simply
   discarded.  The header is always checked for validity, including the header
   CRC if present.  inflateReset() will reset the process to discard the header
   information.  The application would need to call inflateGetHeader() again to
   retrieve the header from the next gzip stream.

     inflateGetHeader returns Z_OK if success, or Z_STREAM_ERROR if the source
   stream state was inconsistent.
*/
pub int inflateGetHeader(z_stream *strm, gz_header *head) {
    if (inflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }
    inflate_state *state = strm->state;
    if ((state->wrap & 2) == 0) {
        return Z_STREAM_ERROR;
    }

    /* save header structure */
    state->head = head;
    head->done = 0;
    return Z_OK;
}

/*
   Search buf[0..len-1] for the pattern: 0, 0, 0xff, 0xff.  Return when found
   or when out of input.  When called, *have is the number of pattern bytes
   found in order so far, in 0..3.  On return *have is updated to the new
   state.  If on return *have equals four, then the pattern was found and the
   return value is how many bytes were read including the last byte of the
   pattern.  If *have is less than four, then the pattern has not been found
   yet and the return value is len.  In the latter case, syncsearch() can be
   called again with more data and the *have state.  *have is initialized to
   zero for the first call.
 */
unsigned syncsearch(unsigned *have, const uint8_t *buf, unsigned len) {
    unsigned got = *have;
    unsigned next = 0;
    while (next < len && got < 4) {
        int x = 0xff;
        if (got < 2) {
            x = 0;
        }
        if ((int)(buf[next]) == x) {
            got++;
        }
        else if (buf[next]) {
            got = 0;
        }
        else {
            got = 4 - got;
        }
        next++;
    }
    *have = got;
    return next;
}


/*
     Skips invalid compressed data until a possible full flush point (see above
   for the description of deflate with Z_FULL_FLUSH) can be found, or until all
   available input is skipped.  No output is provided.

     inflateSync searches for a 00 00 FF FF pattern in the compressed data.
   All full flush points have this pattern, but not all occurrences of this
   pattern are full flush points.

     inflateSync returns Z_OK if a possible full flush point has been found,
   Z_BUF_ERROR if no more input was provided, Z_DATA_ERROR if no flush point
   has been found, or Z_STREAM_ERROR if the stream structure was inconsistent.
   In the success case, the application may save the current current value of
   total_in which indicates where valid compressed data was found.  In the
   error case, the application may repeatedly call inflateSync, providing more
   input each time, until success or end of the input data.
*/
pub int inflateSync(z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return Z_STREAM_ERROR;
    }
    inflate_state *state = strm->state;
    if (strm->avail_in == 0 && state->bits < 8) {
        return Z_BUF_ERROR;
    }

    unsigned len;               /* number of bytes to look at or looked at */
    int flags;                  /* temporary to save header status */
    uint32_t in;
    uint32_t out;      /* temporary to save total_in and total_out */
    uint8_t buf[4];       /* to restore bit buffer to byte string */

    /* if first time, start search in bit buffer */
    if (state->mode != SYNC) {
        state->mode = SYNC;
        state->hold <<= state->bits & 7;
        state->bits -= state->bits & 7;
        len = 0;
        while (state->bits >= 8) {
            buf[len++] = (uint8_t)(state->hold);
            state->hold >>= 8;
            state->bits -= 8;
        }
        state->have = 0;
        syncsearch(&(state->have), buf, len);
    }

    /* search available input */
    len = syncsearch(&(state->have), strm->next_in, strm->avail_in);
    strm->avail_in -= len;
    strm->next_in += len;
    strm->total_in += len;

    /* return no joy or set up to restart inflate() on a new block */
    if (state->have != 4) {
        return Z_DATA_ERROR;
    }
    if (state->flags == -1) {
        state->wrap = 0;    /* if no header yet, treat as raw */
    }
    else {
        state->wrap &= ~4;  /* no point in computing a check value now */
    }
    flags = state->flags;
    in = strm->total_in;  out = strm->total_out;
    inflateReset(strm);
    strm->total_in = in;  strm->total_out = out;
    state->flags = flags;
    state->mode = TYPE;
    return Z_OK;
}

/*
   Returns true if inflate is currently at the end of a block generated by
   Z_SYNC_FLUSH or Z_FULL_FLUSH. This function is used by one PPP
   implementation to provide an additional safety check. PPP uses
   Z_SYNC_FLUSH but removes the length bytes of the resulting empty stored
   block. When decompressing, PPP checks that at the end of input packet,
   inflate is waiting for these length bytes.
 */
pub int inflateSyncPoint(z_stream *strm) {
    if (inflateStateCheck(strm)) return Z_STREAM_ERROR;
    return strm->state->mode == STORED
        && strm->state->bits == 0;
}

/*
     Sets the destination stream as a complete copy of the source stream.

     This function can be useful when randomly accessing a large stream.  The
   first pass through the stream can periodically record the inflate state,
   allowing restarting inflate at those points when randomly accessing the
   stream.

     inflateCopy returns Z_OK if success, Z_MEM_ERROR if there was not
   enough memory, Z_STREAM_ERROR if the source stream state was inconsistent.
   msg is left unchanged in both source and
   destination.
*/
pub int inflateCopy(z_stream *dest, *source) {
    inflate_state *state;
    inflate_state *copy;
    uint8_t *window;
    unsigned wsize;

    /* check input */
    if (inflateStateCheck(source) || dest == NULL) {
        return Z_STREAM_ERROR;
    }
    state = (inflate_state *)source->state;

    /* allocate space */
    copy = calloc(1, sizeof(inflate_state));
    if (copy == NULL) {
        return Z_MEM_ERROR;
    }
    window = NULL;
    if (state->window != NULL) {
        window = calloc(1U << state->wbits, sizeof(uint8_t));
        if (window == NULL) {
            free(copy);
            return Z_MEM_ERROR;
        }
    }

    /* copy state */
    memcpy(dest, source, sizeof(z_stream));
    memcpy(copy, state, sizeof(inflate_state));
    copy->strm = dest;
    if (state->lencode >= state->codes &&
        state->lencode <= state->codes + ENOUGH - 1) {
        copy->lencode = copy->codes + (state->lencode - state->codes);
        copy->distcode = copy->codes + (state->distcode - state->codes);
    }
    copy->next = copy->codes + (state->next - state->codes);
    if (window != NULL) {
        wsize = 1U << state->wbits;
        memcpy(window, state->window, wsize);
    }
    copy->window = window;
    dest->state = copy;
    return Z_OK;
}

pub int inflateUndermine(z_stream *strm, int subvert) {
    inflate_state *state;

    if (inflateStateCheck(strm)) return Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    state->sane = !subvert;
    return Z_OK;
}

pub int inflateValidate(z_stream *strm, int check) {
    inflate_state *state;
    if (inflateStateCheck(strm)) return Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    if (check && state->wrap)
        state->wrap |= 4;
    else
        state->wrap &= ~4;
    return Z_OK;
}


/*
     This function returns two values, one in the lower 16 bits of the return
   value, and the other in the remaining upper bits, obtained by shifting the
   return value down 16 bits.  If the upper value is -1 and the lower value is
   zero, then inflate() is currently decoding information outside of a block.
   If the upper value is -1 and the lower value is non-zero, then inflate is in
   the middle of a stored block, with the lower value equaling the number of
   bytes from the input remaining to copy.  If the upper value is not -1, then
   it is the number of bits back from the current bit position in the input of
   the code (literal or length/distance pair) currently being processed.  In
   that case the lower value is the number of bytes already emitted for that
   code.

     A code is being processed if inflate is waiting for more input to complete
   decoding of the code, or if it has completed decoding but is waiting for
   more output space to write the literal or match data.

     inflateMark() is used to mark locations in the input data for random
   access, which may be at bit positions, and to note those cases where the
   output of a code may span boundaries of random access blocks.  The current
   location in the input stream can be determined from avail_in and data_type
   as noted in the description for the Z_BLOCK flush parameter for inflate.

     inflateMark returns the value noted above, or -65536 if the provided
   source stream state was inconsistent.
*/
pub long inflateMark(z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return -(1L << 16);
    }
    inflate_state *state = strm->state;

    int x;
    if (state->mode == COPY) {
        x = state->length;
    } else if (state->mode == MATCH) {
        x = state->was - state->length;
    } else {
        x = 0;
    }
    return (long)(((uint32_t)((long)state->back)) << 16) + x;
}

pub uint32_t inflateCodesUsed(z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return (uint32_t)-1;
    }
    inflate_state *state = strm->state;
    return state->next - state->codes;
}

/*
   Decode literal, length, and distance codes and write out the resulting
   literal and match bytes until either not enough input or output is
   available, an end-of-block is encountered, or a data error is encountered.
   When large enough input and output buffers are supplied to inflate(), for
   example, a 16K input buffer and a 64K output buffer, more than 95% of the
   inflate execution time is spent in this routine.

   Entry assumptions:

        state->mode == LEN
        strm->avail_in >= 6
        strm->avail_out >= 258
        start >= strm->avail_out
        state->bits < 8

   On return, state->mode is one of:

        LEN -- ran out of enough output space or enough available input
        TYPE -- reached end of block code, inflate() to interpret next block
        BAD -- error in block data

   Notes:

    - The maximum input bits used by a length/distance pair is 15 bits for the
      length code, 5 bits for the length extra, 15 bits for the distance code,
      and 13 bits for the distance extra.  This totals 48 bits, or six bytes.
      Therefore if strm->avail_in >= 6, then there is enough input to avoid
      checking for available input while decoding.

    - The maximum bytes that a single length/distance pair can output is 258
      bytes, which is the maximum length that can be coded.  inflate_fast()
      requires strm->avail_out >= 258 for each loop to avoid checking for
      output space.
 */
/*
   inflate_fast() speedups that turned out slower (on a PowerPC G3 750CXe):
   - Using bit fields for code structure
   - Different op definition to avoid & for extra bits (do & for table bits)
   - Three separate decoding do-loops for direct, window, and wnext == 0
   - Special case for distance > 1 copies to do overlapped load and store copy
   - Explicit branch predictions (based on measured branch probabilities)
   - Deferring match copy and interspersed it with decoding subsequent codes
   - Swapping literal/length else
   - Swapping window/direct else
   - Larger unrolled copy loops (three is about right)
   - Moving len -= 3 statement into middle of loop
 */

typedef {
    /* start is inflate()'s starting value for strm->avail_out */
    inflate_state *state;
    uint8_t *in;      /* local strm->next_in */
    uint8_t *last;    /* have enough input while in < last */
    uint8_t *out;     /* local strm->next_out */
    uint8_t *beg;     /* inflate()'s initial strm->next_out */
    uint8_t *end;     /* while out < end, enough space available */
    unsigned wsize;             /* window size or zero if not using window */
    unsigned whave;             /* valid bytes in the window */
    unsigned wnext;             /* window write index */
    uint8_t *window;  /* allocated sliding window, if wsize != 0 */
    uint32_t hold;         /* local strm->hold */
    unsigned bits;              /* local strm->bits */
    code_t *lcode;      /* local strm->lencode */
    code_t *dcode;      /* local strm->distcode */
    unsigned lmask;             /* mask for first level of length codes */
    unsigned dmask;             /* mask for first level of distance codes */
    code_t *here;           /* retrieved table entry */
    unsigned op;                /* code bits, operation, extra bits, or */
                                /*  window position, window bytes to copy */
    unsigned len;               /* match length, unused bytes */
    unsigned dist;              /* match distance */
    uint8_t *from;    /* where to copy match from */
} fastctx_t;

void inflate_fast(z_stream *strm, unsigned start) {
    fastctx_t ctx = {};

    /* copy state to local variables */
    ctx.state = strm->state;
    ctx.in = strm->next_in;
    ctx.last = ctx.in + (strm->avail_in - 5);
    ctx.out = strm->next_out;
    ctx.beg = ctx.out - (start - strm->avail_out);
    ctx.end = ctx.out + (strm->avail_out - 257);
    ctx.wsize = ctx.state->wsize;
    ctx.whave = ctx.state->whave;
    ctx.wnext = ctx.state->wnext;
    ctx.window = ctx.state->window;
    ctx.hold = ctx.state->hold;
    ctx.bits = ctx.state->bits;
    ctx.lcode = ctx.state->lencode;
    ctx.dcode = ctx.state->distcode;
    ctx.lmask = (1U << ctx.state->lenbits) - 1;
    ctx.dmask = (1U << ctx.state->distbits) - 1;

    /* decode literals and length/distances until end-of-block or not enough
       input data or output space */
    while (true) {
        if (ctx->bits < 15) {
            ctx.hold += (uint32_t)(*(ctx->in)++) << ctx.bits;
            ctx.bits += 8;
            ctx.hold += (uint32_t)(*(ctx->in)++) << ctx.bits;
            ctx.bits += 8;
        }
        ctx->here = ctx->lcode + (ctx->hold & ctx->lmask);
        int r = dolen(ctx);
        if (r == BREAK) break;
        if (r == CONTINUE) continue;
        if (ctx->in >= ctx->last || ctx->out >= ctx->end) {
            break;
        }
    }

    /* return unused bytes (on entry, bits < 8, so in won't go too far back) */
    ctx.len = ctx.bits >> 3;
    ctx.in -= ctx.len;
    ctx.bits -= ctx.len << 3;
    ctx.hold &= (1U << ctx.bits) - 1;

    /* update state and return */
    ctx.strm->next_in = ctx.in;
    ctx.strm->next_out = ctx.out;
    if (ctx.in < ctx.last) {
        ctx.strm->avail_in = (unsigned)(5 + (ctx.last - ctx.in));
    } else {
        ctx.strm->avail_in = (unsigned)(5 - (ctx.in - ctx.last));
    }
    if (ctx.out < ctx.end) {
        ctx.strm->avail_out = (unsigned)(257 + (ctx.end - ctx.out));
    } else {
        ctx.strm->avail_out = (unsigned)(257 - (ctx.out - ctx.end));
    }    
    ctx.state->hold = ctx.hold;
    ctx.state->bits = ctx.bits;
    return;
}

int dolen(fastctx_t *ctx) {
    ctx->op = (unsigned)(ctx->here->bits);
    ctx->hold >>= ctx->op;
    ctx->bits -= ctx->op;
    ctx->op = (unsigned)(ctx->here->op);
    if (ctx->op == 0) {                          
        /* literal */
        if (ctx->here->val >= 0x20 && ctx->here->val < 0x7f) {
            trace.Tracevv(stderr,  "inflate:         literal '%c'\n", ctx->here->val);
        } else {
            trace.Tracevv(stderr, "inflate:         literal 0x%02x\n", ctx->here->val);
        }
        *(ctx->out)++ = (uint8_t)(ctx->here->val);
    }
    else if (ctx->op & 16) {                     
        /* length base */
        ctx->len = (unsigned)(ctx->here->val);
        ctx->op &= 15;                           /* number of extra bits */
        if (ctx->op) {
            if (ctx->bits < ctx->op) {
                ctx->hold += (uint32_t)(*(ctx->in)++) << ctx->bits;
                ctx->bits += 8;
            }
            ctx->len += (unsigned)ctx->hold & ((1U << ctx->op) - 1);
            ctx->hold >>= ctx->op;
            ctx->bits -= ctx->op;
        }
        trace.Tracevv(stderr, "inflate:         length %u\n", ctx->len);
        if (ctx->bits < 15) {
            ctx->hold += (uint32_t)(*(ctx->in)++) << ctx->bits;
            ctx->bits += 8;
            ctx->hold += (uint32_t)(*(ctx->in)++) << ctx->bits;
            ctx->bits += 8;
        }
        ctx->here = ctx->dcode + (ctx->hold & ctx->dmask);

        int r = dodist(ctx);
        if (r == BREAK || r == CONTINUE) {
            return r;
        }
    }
    else if ((ctx->op & 64) == 0) {              
        /* 2nd level length code */
        ctx->here = ctx->lcode + ctx->here->val + (ctx->hold & ((1U << ctx->op) - 1));
        return dolen(ctx);
    }
    else if (ctx->op & 32) {                     
        /* end-of-block */
        trace.Tracevv(stderr, "inflate:         end of block\n");
        ctx->state->mode = TYPE;
        return BREAK;
    }
    else {
        ctx->strm->msg = "invalid literal/length code";
        ctx->state->mode = BAD;
        return BREAK;
    }
    return CONTINUE;
}

int dodist(fastctx_t *ctx) {
    ctx->op = (unsigned)(ctx->here->bits);
    ctx->hold >>= ctx->op;
    ctx->bits -= ctx->op;
    ctx->op = (unsigned)(ctx->here->op);
    if (ctx->op & 16) {                      
        /* distance base */
        int r = wtfgoto4(ctx);
        if (r == BREAK || r == CONTINUE) {
            return r;
        }
    }
    else if ((ctx->op & 64) == 0) {          
        /* 2nd level distance code */
        ctx->here = ctx->dcode + ctx->here->val + (ctx->hold & ((1U << ctx->op) - 1));
        return dodist(ctx);
    }
    else {
        ctx->strm->msg = "invalid distance code";
        ctx->state->mode = BAD;
        return BREAK;
    }
}

int wtfgoto4(fastctx_t *ctx) {
    ctx->dist = (unsigned)(ctx->here->val);
    ctx->op &= 15;                       /* number of extra bits */
    if (ctx->bits < ctx->op) {
        ctx->hold += (uint32_t)(*(ctx->in)++) << ctx->bits;
        ctx->bits += 8;
        if (ctx->bits < ctx->op) {
            ctx->hold += (uint32_t)(*(ctx->in)++) << ctx->bits;
            ctx->bits += 8;
        }
    }
    ctx->dist += (unsigned)ctx->hold & ((1U << ctx->op) - 1);
    ctx->hold >>= ctx->op;
    ctx->bits -= ctx->op;
    trace.Tracevv(stderr, "inflate:         distance %u\n", ctx->dist);
    ctx->op = (unsigned)(ctx->out - ctx->beg);     /* max distance in output */
    if (ctx->dist > ctx->op) {                /* see if copy from window */
        int r = wtfgoto3(ctx);
        if (r == BREAK || r == CONTINUE) {
            return r;
        }
        wtfgoto2(ctx);
    }
    else {
        wtfgoto1(ctx);
    }
    return CONTINUE;
}

int wtfgoto3(fastctx_t *ctx) {
    /* distance back in window */
    ctx->op = ctx->dist - ctx->op;             
    if (ctx->op > ctx->whave) {
        if (ctx->state->sane) {
            ctx->strm->msg = "invalid distance too far back";
            ctx->state->mode = BAD;
            return BREAK;
        }
        if (ctx->len <= ctx->op - ctx->whave) {
            while (true) {
                *(ctx->out)++ = 0;
                if (!--ctx->len) {
                    break;
                }
            }
            return CONTINUE;
        }
        ctx->len -= ctx->op - ctx->whave;
        while (true) {
            *(ctx->out)++ = 0;
            ctx->op--;
            bool cont = (ctx->op > ctx->whave);
            if (!cont) {
                break;
            }
        }
        if (ctx->op == 0) {
            ctx->from = ctx->out - ctx->dist;
            while (true) {
                *(ctx->out)++ = *(ctx->from)++;
                if (!--ctx->len) {
                    break;
                }
            }
            return CONTINUE;
        }
    }
    return FALLTHROUGH;
}

void wtfgoto2(fastctx_t *ctx) {
    ctx->from = ctx->window;
    if (ctx->wnext == 0) {           
        /* very common case */
        ctx->from += ctx->wsize - ctx->op;
        if (ctx->op < ctx->len) {         
            /* some from window */
            ctx->len -= ctx->op;
            while (true) {
                *(ctx->out)++ = *(ctx->from)++;
                if (!--ctx->op) {
                    break;
                }
            }
            ctx->from = ctx->out - ctx->dist;  /* rest from output */
        }
    }
    else if (ctx->wnext < ctx->op) {      /* wrap around window */
        ctx->from += ctx->wsize + ctx->wnext - ctx->op;
        ctx->op -= ctx->wnext;
        if (ctx->op < ctx->len) {         /* some from end of window */
            ctx->len -= ctx->op;
            while (true) {
                *(ctx->out)++ = *(ctx->from)++;
                if (!--ctx->op) break;
            }
            ctx->from = ctx->window;
            if (ctx->wnext < ctx->len) {  /* some from start of window */
                ctx->op = ctx->wnext;
                ctx->len -= ctx->op;
                while (true) {
                    *(ctx->out)++ = *(ctx->from)++;
                    if (!--ctx->op) {
                        break;
                    }
                }
                ctx->from = ctx->out - ctx->dist;      /* rest from output */
            }
        }
    }
    else {                      /* contiguous in window */
        ctx->from += ctx->wnext - ctx->op;
        if (ctx->op < ctx->len) {         /* some from window */
            ctx->len -= ctx->op;
            while (true) {
                *(ctx->out)++ = *(ctx->from)++;
                if (!--ctx->op) {
                    break;
                }
            }
            ctx->from = ctx->out - ctx->dist;  /* rest from output */
        }
    }
    while (ctx->len > 2) {
        *(ctx->out)++ = *(ctx->from)++;
        *(ctx->out)++ = *(ctx->from)++;
        *(ctx->out)++ = *(ctx->from)++;
        ctx->len -= 3;
    }
    if (ctx->len) {
        *(ctx->out)++ = *(ctx->from)++;
        if (ctx->len > 1) {
            *(ctx->out)++ = *(ctx->from)++;
        }
    }
}

void wtfgoto1(fastctx_t *ctx) {
    ctx->from = ctx->out - ctx->dist;          /* copy direct from output */
    while (true) {                        /* minimum length is three */
        *(ctx->out)++ = *(ctx->from)++;
        *(ctx->out)++ = *(ctx->from)++;
        *(ctx->out)++ = *(ctx->from)++;
        ctx->len -= 3;
        if (ctx->len <= 2) {
            break;
        }
    }
    if (ctx->len) {
        *(ctx->out)++ = *(ctx->from)++;
        if (ctx->len > 1) {
            *(ctx->out)++ = *(ctx->from)++;
        }
    }
}



uint16_t inftable_lbase[31] = { /* Length codes 257..285 base */
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
uint16_t inftable_lext[31] = { /* Length codes 257..285 extra */
    16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18,
    19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 16, 194, 65};
uint16_t inftable_dbase[32] = { /* Distance codes 0..29 base */
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577, 0, 0};
uint16_t inftable_dext[32] = { /* Distance codes 0..29 extra */
    16, 16, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
    23, 23, 24, 24, 25, 25, 26, 26, 27, 27,
    28, 28, 29, 29, 64, 64};
    
/*
   Build a set of tables to decode the provided canonical Huffman code.
   The code lengths are lens[0..codes-1].  The result starts at *table,
   whose indices are 0..2^bits-1.  work is a writable array of at least
   lens shorts, which is used as a work area.  type is the type of code
   to be generated, CODES, LENS, or DISTS.  On return, zero is success,
   -1 is an invalid code, and +1 means that ENOUGH isn't enough.  table
   on return points to the next available entry's address.  bits is the
   requested root table index bits, and on return it is the actual root
   table index bits.  It will differ if the request is greater than the
   longest code or if it is less than the shortest code.
 */
int inflate_table(
    int type,
    uint16_t *lens,
    unsigned codes,
    code_t **table,
    unsigned *bits,
    uint16_t *work
) {
    unsigned len;               /* a code's length in bits */
    unsigned sym;               /* index of code symbols */
    unsigned min;
    unsigned max;          /* minimum and maximum code lengths */
    unsigned root;              /* number of index bits for root table */
    unsigned curr;              /* number of index bits for current table */
    unsigned drop;              /* code bits to drop for sub-table */
    int left;                   /* number of prefix codes available */
    unsigned used;              /* code entries in table used */
    unsigned huff;              /* Huffman code */
    unsigned incr;              /* for incrementing code, index */
    unsigned fill;              /* index for replicating entries */
    unsigned low;               /* low bits for current root entry */
    unsigned mask;              /* mask for low root bits */
    code_t here;                  /* table entry for duplication */
    code_t *next;             /* next available space in table */
    const uint16_t *base;     /* base value table to use */
    const uint16_t *extra;    /* extra bits table to use */
    unsigned match;             /* use base and extra for symbol >= match */
    uint16_t count[MAXBITS+1];    /* number of codes of each length */
    uint16_t offs[MAXBITS+1];     /* offsets in table for each length */
   

    /*
       Process a set of code lengths to create a canonical Huffman code.  The
       code lengths are lens[0..codes-1].  Each length corresponds to the
       symbols 0..codes-1.  The Huffman code is generated by first sorting the
       symbols by length from short to long, and retaining the symbol order
       for codes with equal lengths.  Then the code starts with all zero bits
       for the first code of the shortest length, and the codes are integer
       increments for the same length, and zeros are appended as the length
       increases.  For the deflate format, these bits are stored backwards
       from their more natural integer increment ordering, and so when the
       decoding tables are built in the large loop below, the integer codes
       are incremented backwards.

       This routine assumes, but does not check, that all of the entries in
       lens[] are in the range 0..MAXBITS.  The caller must assure this.
       1..MAXBITS is interpreted as that code length.  zero means that that
       symbol does not occur in this code.

       The codes are sorted by computing a count of codes for each length,
       creating from that a table of starting indices for each length in the
       sorted table, and then entering the symbols in order in the sorted
       table.  The sorted table is work[], with that space being provided by
       the caller.

       The length counts are used for other purposes as well, i.e. finding
       the minimum and maximum length codes, determining if there are any
       codes at all, checking for a valid set of lengths, and looking ahead
       at length counts to determine sub-table sizes when building the
       decoding tables.
     */

    /* accumulate lengths for codes (assumes lens[] all in 0..MAXBITS) */
    for (len = 0; len <= MAXBITS; len++)
        count[len] = 0;
    for (sym = 0; sym < codes; sym++)
        count[lens[sym]]++;

    /* bound code lengths, force root to be within code lengths */
    root = *bits;
    for (max = MAXBITS; max >= 1; max--)
        if (count[max] != 0) break;
    if (root > max) root = max;
    if (max == 0) {                     /* no symbols to code at all */
        here.op = (uint8_t)64;    /* invalid code marker */
        here.bits = (uint8_t)1;
        here.val = (uint16_t)0;
        *(*table)++ = here;             /* make a table to force an error */
        *(*table)++ = here;
        *bits = 1;
        return 0;     /* no symbols, but wait for decoding to report error */
    }
    for (min = 1; min < max; min++)
        if (count[min] != 0) break;
    if (root < min) root = min;

    /* check for an over-subscribed or incomplete set of lengths */
    left = 1;
    for (len = 1; len <= MAXBITS; len++) {
        left <<= 1;
        left -= count[len];
        if (left < 0) return -1;        /* over-subscribed */
    }
    if (left > 0 && (type == CODES || max != 1))
        return -1;                      /* incomplete set */

    /* generate offsets into symbol table for each length for sorting */
    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + count[len];

    /* sort symbols by length, by symbol order within each length */
    for (sym = 0; sym < codes; sym++)
        if (lens[sym] != 0) work[offs[lens[sym]]++] = (uint16_t)sym;

    /*
       Create and fill in decoding tables.  In this loop, the table being
       filled is at next and has curr index bits.  The code being used is huff
       with length len.  That code is converted to an index by dropping drop
       bits off of the bottom.  For codes where len is less than drop + curr,
       those top drop + curr - len bits are incremented through all values to
       fill the table with replicated entries.

       root is the number of index bits for the root table.  When len exceeds
       root, sub-tables are created pointed to by the root entry with an index
       of the low root bits of huff.  This is saved in low to check for when a
       new sub-table should be started.  drop is zero when the root table is
       being filled, and drop is root when sub-tables are being filled.

       When a new sub-table is needed, it is necessary to look ahead in the
       code lengths to determine what size sub-table is needed.  The length
       counts are used for this, and so count[] is decremented as codes are
       entered in the tables.

       used keeps track of how many table entries have been allocated from the
       provided *table space.  It is checked for LENS and DIST tables against
       the constants ENOUGH_LENS and ENOUGH_DISTS to guard against changes in
       the initial root table size constants.  See the comments in inftrees.h
       for more information.

       sym increments through all symbols, and the loop terminates when
       all codes of length max, i.e. all codes, have been processed.  This
       routine permits incomplete codes, so another loop after this one fills
       in the rest of the decoding tables with invalid code markers.
     */

    /* set up for code type */
    switch (type) {
        case CODES: {
            base = extra = work;    /* dummy value--not used */
            match = 20;
        }
        case LENS: {
            base = inftable_lbase;
            extra = inftable_lext;
            match = 257;
        }
        default:    {
            /* DISTS */
            base = inftable_dbase;
            extra = inftable_dext;
            match = 0;
        }
    }

    /* initialize state for loop */
    huff = 0;                   /* starting code */
    sym = 0;                    /* starting code symbol */
    len = min;                  /* starting code length */
    next = *table;              /* current table to fill in */
    curr = root;                /* current table index bits */
    drop = 0;                   /* current bits to drop from code for index */
    low = (unsigned)(-1);       /* trigger new sub-table when len > root */
    used = 1U << root;          /* use root table entries */
    mask = used - 1;            /* mask for comparing low */

    /* check available table space */
    if ((type == LENS && used > ENOUGH_LENS) ||
        (type == DISTS && used > ENOUGH_DISTS))
        return 1;

    /* process all codes and make table entries */
    while (true) {
        /* create table entry */
        here.bits = (uint8_t)(len - drop);
        if (work[sym] + 1U < match) {
            here.op = (uint8_t)0;
            here.val = work[sym];
        }
        else if (work[sym] >= match) {
            here.op = (uint8_t)(extra[work[sym] - match]);
            here.val = base[work[sym] - match];
        }
        else {
            here.op = (uint8_t)(32 + 64);         /* end of block */
            here.val = 0;
        }

        /* replicate for those indices with low len bits equal to huff */
        incr = 1U << (len - drop);
        fill = 1U << curr;
        min = fill;                 /* save offset to next table */
        while (true) {
            fill -= incr;
            next[(huff >> drop) + fill] = here;
            if (fill == 0) break;
        }

        /* backwards increment the len-bit code huff */
        incr = 1U << (len - 1);
        while (huff & incr) {
            incr >>= 1;
        }
        if (incr != 0) {
            huff &= incr - 1;
            huff += incr;
        }
        else {
            huff = 0;
        }

        /* go to next symbol, update count, len */
        sym++;
        if (--(count[len]) == 0) {
            if (len == max) break;
            len = lens[work[sym]];
        }

        /* create new sub-table if needed */
        if (len > root && (huff & mask) != low) {
            /* if first time, transition to sub-tables */
            if (drop == 0)
                drop = root;

            /* increment past last table */
            next += min;            /* here min is 1 << curr */

            /* determine length of next table */
            curr = len - drop;
            left = (int)(1 << curr);
            while (curr + drop < max) {
                left -= count[curr + drop];
                if (left <= 0) break;
                curr++;
                left <<= 1;
            }

            /* check for enough space */
            used += 1U << curr;
            if ((type == LENS && used > ENOUGH_LENS) ||
                (type == DISTS && used > ENOUGH_DISTS))
                return 1;

            /* point entry in root table to sub-table */
            low = huff & mask;
            (*table)[low].op = (uint8_t)curr;
            (*table)[low].bits = (uint8_t)root;
            (*table)[low].val = (uint16_t)(next - *table);
        }
    }

    /* fill in remaining table entry if code is incomplete (guaranteed to have
       at most one remaining entry, since if the code is incomplete, the
       maximum code length that was allowed to get this far is one bit) */
    if (huff != 0) {
        here.op = (uint8_t)64;            /* invalid code marker */
        here.bits = (uint8_t)(len - drop);
        here.val = (uint16_t)0;
        next[huff] = here;
    }

    /* set return parameters */
    *table += used;
    *bits = root;
    return 0;
}
