/* deflate.c -- compress data using the deflation algorithm
 * Copyright (C) 1995-2022 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * The idea of lazy evaluation of matches is due to Jan-Mark Wams, and I found it in 'freeze' written by Leonid Broukhis.

 */


/*
Before calling deflate or inflate, make sure that avail_in and avail_out are not zero.
    When setting the parameter flush equal to stream.Z_FINISH, also make sure that
    avail_out is big enough to allow processing all pending input.  Note that a
    stream.Z_BUF_ERROR is not fatal--another call to deflate() or inflate() can be
    made with more input or output space.  A stream.Z_BUF_ERROR may in fact be
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

#import stream.c
#import trace.c
#import crc32
#import adler32
#import assert.c
#import zmalloc.c

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
/* The three kinds of block type */

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
/* default memLevel */
#define DEF_MEM_LEVEL 8

#define BL_CODES  19
/* number of codes used to transfer the bit lengths */

/* Maximum value for windowBits in deflateInit2 and inflateInit2.
 * WARNING: reducing MAX_WBITS makes minigzip unable to extract .gz files
 * created by gzip. (Files created by minigzip can still be extracted by
 * gzip.)
 */
#define MAX_WBITS   15 /* 32K LZ77 window */

// In Windows16, OS_CODE is 0, as in MSDOS [Truta]
// In Cygwin, OS_CODE is 3 (Unix), not 11 (Windows32) [Wilson]
#define OS_CODE  3     /* assume Unix */


#define MAX_BITS 15
/* All codes must not exceed MAX_BITS bits */
/* number of distance codes */
#define D_CODES   30

/* maximum heap size */
#define HEAP_SIZE (2*L_CODES+1)

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

/* compression strategy; see deflateInit2() below for details */
pub enum {
  Z_FILTERED            = 1,
  Z_HUFFMAN_ONLY        = 2,
  Z_RLE                 = 3,
  Z_FIXED               = 4,
  Z_DEFAULT_STRATEGY    = 0,
};

#define ZLIB_VERSION "foo.bar"

#define PRESET_DICT 0x20 /* preset dictionary flag in zlib header */

void *ZALLOC(stream.z_stream *strm, size_t items, size) {
    return (*((strm)->zalloc))((strm)->opaque, items, size);
}


#  define GZIP

/* The minimum and maximum match lengths */
#define MIN_MATCH  3
#define MAX_MATCH  258


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


/* Output a byte on the stream.
 * IN assertion: there is enough room in pending_buf.
 */
void put_byte(stream.deflate_state *s, uint8_t c) {
    s->pending_buf[s->pending++] = c;
}


#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
/* Minimum amount of lookahead, except at the end of the input file.
 * See deflate.c for comments about the MIN_MATCH+1.
 */


/* In order to simplify the code, particularly on 16 bit machines, match
 * distances are limited to MAX_DIST instead of WSIZE.
 */
int MAX_DIST(stream.deflate_state *s)  {
    return s->w_size - MIN_LOOKAHEAD;
}

#define WIN_INIT MAX_MATCH
/* Number of bytes after end of data in window to initialize in order to avoid
   memory checker errors from longest match routines */



void _tr_tally_lit(stream.deflate_state *s, uint8_t c, bool *flush) {
    *flush = _tr_tally(s, 0, c);
}
void _tr_tally_dist(stream.deflate_state *s, uint16_t distance, uint8_t length, bool *flush) {
    *flush = _tr_tally(s, distance, length);
}


const char deflate_copyright[] =
   " deflate 1.2.13 Copyright 1995-2022 Jean-loup Gailly and Mark Adler ";
/*
  If you use the zlib library in a product, an acknowledgment is welcome
  in the documentation of your product. If for some reason you cannot
  include such an acknowledgment, I would appreciate that you keep this
  copyright string in the executable of your product.
 */

enum {
    need_more,      /* block not completed, need more input or more output */
    block_done,     /* block flush performed */
    finish_started, /* finish started, need only more output at next deflate */
    finish_done     /* finish done, accept no more input or output */
};

/* Compression function. Returns the block state after the call. */
typedef int *compress_func(stream.deflate_state *, int);

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
   compress_func func;
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

/* rank stream.Z_BLOCK between stream.Z_NO_FLUSH and stream.Z_PARTIAL_FLUSH */
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
void UPDATE_HASH(stream.deflate_state *s, uint32_t c) {
    s->ins_h = ((s->ins_h << s->hash_shift) ^ (c)) & s->hash_mask;
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
void INSERT_STRING(stream.deflate_state *s, int str, int *match_head) {
    UPDATE_HASH(s, s->window[(str) + (MIN_MATCH-1)]);
    *match_head = s->prev[(str) & s->w_mask] = s->head[s->ins_h];
    s->head[s->ins_h] = (uint16_t)(str);
}


void zmemzero(void *dest, size_t len) {
    memset(dest, 0, len);
}


/* ===========================================================================
 * Initialize the hash table (avoiding 64K overflow for 16 bit systems).
 * prev[] will be initialized on the fly.
 */
void CLEAR_HASH(stream.deflate_state *s) {
    s->head[s->hash_size - 1] = NIL;
    zmemzero((uint8_t *)s->head, (unsigned)(s->hash_size - 1)*sizeof(*s->head));
}

/* ===========================================================================
 * Slide the hash table when sliding the window down (could be avoided with 32
 * bit values at the expense of memory usage). We slide even when level == 0 to
 * keep the hash table consistent if we switch back to level > 0 later.
 */
void slide_hash(stream.deflate_state *s) {
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
 
 The fields
   zalloc, zfree and opaque must be initialized before by the caller.
   
If zalloc and zfree are set to NULL, deflateInit updates them to use default allocation functions.

The compression level must be Z_DEFAULT_COMPRESSION, or between 0 and 9:
   1 gives best speed, 9 gives best compression, 0 gives no compression at all
   (the input data is simply copied a block at a time).  Z_DEFAULT_COMPRESSION
   requests a default compromise between speed and compression (currently
   equivalent to level 6).

     deflateInit returns stream.Z_OK if success, stream.Z_MEM_ERROR if there was not enough
   memory, stream.Z_STREAM_ERROR if level is not a valid compression level, or
   Z_VERSION_ERROR if the zlib library version (zlib_version) is incompatible
   with the version assumed by the caller (ZLIB_VERSION).  msg is set to null
   if there is no error message.  deflateInit does not perform any compression:
   this will be done by deflate().
*/
pub int deflateInit(stream.z_stream *strm, int level) {
  return deflateInit2_(strm, level, Z_DEFLATED, MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    /* To do: ignore strm->next_in if we use it as window */
}

pub int deflateInit2(stream.z_stream *strm, int level, int method, int windowBits, int memLevel, int strategy) {
    return deflateInit2_(strm, level, method, windowBits, memLevel, strategy);
}

const char my_version[] = ZLIB_VERSION;

/*
pub int deflateInit2 OF((stream.z_stream *strm,
                                     int  level,
                                     int  method,
                                     int  windowBits,
                                     int  memLevel,
                                     int  strategy));

     This is another version of deflateInit with more compression options.  The
   fields zalloc, zfree and opaque must be initialized before by the caller.

     The method parameter is the compression method.  It must be Z_DEFLATED in
   this version of the library.

     The windowBits parameter is the base two logarithm of the window size
   (the size of the history buffer).  It should be in the range 8..15 for this
   version of the library.  Larger values of this parameter result in better
   compression at the expense of memory usage.  The default value is 15 if
   deflateInit is used instead.

     For the current implementation of deflate(), a windowBits value of 8 (a
   window size of 256 bytes) is not supported.  As a result, a request for 8
   will result in 9 (a 512-byte window).  In that case, providing 8 to
   inflateInit2() will result in an error when the zlib header with 9 is
   checked against the initialization of inflate().  The remedy is to not use 8
   with deflateInit2() with this initialization, or at least in that case use 9
   with inflateInit2().

     windowBits can also be -8..-15 for raw deflate.  In this case, -windowBits
   determines the window size.  deflate() will then generate raw deflate data
   with no zlib header or trailer, and will not compute a check value.

     windowBits can also be greater than 15 for optional gzip encoding.  Add
   16 to windowBits to write a simple gzip header and trailer around the
   compressed data instead of a zlib wrapper.  The gzip header will have no
   file name, no extra data, no comment, no modification time (set to zero), no
   header crc, and the operating system will be set to the appropriate value,
   if the operating system was determined at compile time.  If a gzip stream is
   being written, strm->adler is a CRC-32 instead of an Adler-32.

     For raw deflate or gzip encoding, a request for a 256-byte window is
   rejected as invalid, since only the zlib header provides a means of
   transmitting the window size to the decompressor.

     The memLevel parameter specifies how much memory should be allocated
   for the internal compression state.  memLevel=1 uses minimum memory but is
   slow and reduces compression ratio; memLevel=9 uses maximum memory for
   optimal speed.  The default value is 8.  See zconf.h for total memory usage
   as a function of windowBits and memLevel.

The strategy (filter) parameter:
Z_DEFAULT_STRATEGY for normal data
Z_FILTERED for data produced by a filter (or predictor)
Z_HUFFMAN_ONLY to force Huffman encoding only (no string match)
Z_RLE to limit match distances to one (run-length encoding).

Filtered data consists mostly of small values with a somewhat random distribution.
In this case, the compression algorithm is tuned to compress them better.
The effect of Z_FILTERED is to force more Huffman coding and less string matching.
It is somewhat intermediate between Z_DEFAULT_STRATEGY and Z_HUFFMAN_ONLY.

Z_RLE is designed to be almost as fast as Z_HUFFMAN_ONLY, but give better compression for PNG image data.

The strategy parameter only affects the compression ratio but not the correctness of the compressed output even if it is not set appropriately.

Z_FIXED prevents the use of dynamic Huffman codes, allowing for a simpler decoder for special applications.

     deflateInit2 returns stream.Z_OK if success, stream.Z_MEM_ERROR if there was not enough
   memory, stream.Z_STREAM_ERROR if any parameter is invalid (such as an invalid
   method), or Z_VERSION_ERROR if the zlib library version (zlib_version) is
   incompatible with the version assumed by the caller (ZLIB_VERSION).  msg is
   set to null if there is no error message.  deflateInit2 does not perform any
   compression: this will be done by deflate().
*/
pub int deflateInit2_(
    stream.z_stream *strm,
    int level,
    int method,
    int windowBits,
    int memLevel,
    int strategy
) {
    stream.deflate_state *s;
    int wrap = 1;

    // if (version == NULL || version[0] != my_version[0] ||
    //     stream_size != sizeof(stream.z_stream)) {
    //     return Z_VERSION_ERROR;
    // }
    if (strm == NULL) return stream.Z_STREAM_ERROR;

    strm->msg = NULL;
    if (strm->zalloc == NULL) {
        strm->zalloc = zmalloc.zcalloc;
        strm->opaque = NULL;
    }
    if (strm->zfree == NULL)
        strm->zfree = zmalloc.zcfree;

    if (level == Z_DEFAULT_COMPRESSION) level = 6;

    if (windowBits < 0) { /* suppress zlib wrapper */
        wrap = 0;
        if (windowBits < -15)
            return stream.Z_STREAM_ERROR;
        windowBits = -windowBits;
    }
    else if (windowBits > 15) {
        wrap = 2;       /* write gzip wrapper instead */
        windowBits -= 16;
    }
    if (memLevel < 1 || memLevel > MAX_MEM_LEVEL || method != Z_DEFLATED ||
        windowBits < 8 || windowBits > 15 || level < 0 || level > 9 ||
        strategy < 0 || strategy > Z_FIXED || (windowBits == 8 && wrap != 1)) {
        return stream.Z_STREAM_ERROR;
    }
    if (windowBits == 8) windowBits = 9;  /* until 256-byte window bug fixed */
    s = ZALLOC(strm, 1, sizeof(stream.deflate_state));
    if (s == NULL) return stream.Z_MEM_ERROR;
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

    s->window = (uint8_t *) ZALLOC(strm, s->w_size, 2*sizeof(uint8_t));
    s->prev   = (uint16_t *)  ZALLOC(strm, s->w_size, sizeof(uint16_t));
    s->head   = (uint16_t *)  ZALLOC(strm, s->hash_size, sizeof(uint16_t));

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

    s->pending_buf = ZALLOC(strm, s->lit_bufsize, 4);
    s->pending_buf_size = (uint32_t)s->lit_bufsize * 4;

    if (s->window == NULL || s->prev == NULL || s->head == NULL ||
        s->pending_buf == NULL) {
        s->status = FINISH_STATE;
        strm->msg = ERR_MSG(stream.Z_MEM_ERROR);
        deflateEnd (strm);
        return stream.Z_MEM_ERROR;
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
int deflateStateCheck(stream.z_stream *strm) {
    if (strm == NULL || strm->zalloc == NULL || strm->zfree == NULL) {
        return 1;
    }
    stream.deflate_state *s = strm->state;
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
   options stream.Z_BLOCK, stream.Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, or stream.Z_FULL_FLUSH.  The
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

     deflateSetDictionary returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if a
   parameter is invalid (e.g.  dictionary being NULL) or the stream state is
   inconsistent (for example if deflate has already been called for this stream
   or if not at a block boundary for raw deflate).  deflateSetDictionary does
   not perform any compression: this will be done by deflate().
*/
pub int deflateSetDictionary(
    stream.z_stream *strm,
    const uint8_t *dictionary,
    uint32_t  dictLength
) {
    stream.deflate_state *s;
    uint32_t str;
    uint32_t n;
    int wrap;
    unsigned avail;
    const uint8_t *next;

    if (deflateStateCheck(strm) || dictionary == NULL)
        return stream.Z_STREAM_ERROR;
    s = strm->state;
    wrap = s->wrap;
    if (wrap == 2 || (wrap == 1 && s->status != INIT_STATE) || s->lookahead)
        return stream.Z_STREAM_ERROR;

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
    return stream.Z_OK;
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

     deflateGetDictionary returns stream.Z_OK on success, or stream.Z_STREAM_ERROR if the
   stream state is inconsistent.
*/
pub int deflateGetDictionary(
    stream.z_stream *strm,
    uint8_t *dictionary,
    uint32_t  *dictLength
) {
    stream.deflate_state *s;
    uint32_t len;

    if (deflateStateCheck(strm))
        return stream.Z_STREAM_ERROR;
    s = strm->state;
    len = s->strstart + s->lookahead;
    if (len > s->w_size)
        len = s->w_size;
    if (dictionary != NULL && len)
        memcpy(dictionary, s->window + s->strstart + s->lookahead - len, len);
    if (dictLength != NULL)
        *dictLength = len;
    return stream.Z_OK;
}

/* ========================================================================= */
pub int deflateResetKeep(stream.z_stream *strm) {
    stream.deflate_state *s;

    if (deflateStateCheck(strm)) {
        return stream.Z_STREAM_ERROR;
    }

    strm->total_in = strm->total_out = 0;
    strm->msg = NULL; /* use zfree if we ever allocate msg dynamically */
    strm->data_type = stream.Z_UNKNOWN;

    s = strm->state;
    s->pending = 0;
    s->pending_out = s->pending_buf;

    if (s->wrap < 0) {
        s->wrap = -s->wrap; /* was made negative by deflate(..., stream.Z_FINISH); */
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

    return stream.Z_OK;
}

/*
     This function is equivalent to deflateEnd followed by deflateInit, but
   does not free and reallocate the internal compression state.  The stream
   will leave the compression level and any other attributes that may have been
   set unchanged.

     deflateReset returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if the source
   stream state was inconsistent (such as zalloc or state being NULL).
*/
pub int deflateReset(stream.z_stream *strm) {
    int ret = deflateResetKeep(strm);
    if (ret == stream.Z_OK) {
        lm_init(strm->state);
    }
    return ret;
}

/*
     deflateSetHeader() provides gzip header information for when a gzip
   stream is requested by deflateInit2().  deflateSetHeader() may be called
   after deflateInit2() or deflateReset() and before the first call of
   deflate().  The text, time, os, extra field, name, and comment information
   in the provided stream.gz_header structure are written to the gzip header (xflag is
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

     deflateSetHeader returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if the source
   stream state was inconsistent.
*/
pub int deflateSetHeader(stream.z_stream *strm, stream.gz_header *head) {
    if (deflateStateCheck(strm) || strm->state->wrap != 2) {
        return stream.Z_STREAM_ERROR;
    }
    strm->state->gzhead = head;
    return stream.Z_OK;
}

/*
     deflatePending() returns the number of bytes and bits of output that have
   been generated, but not yet provided in the available output.  The bytes not
   provided would be due to the available output space having being consumed.
   The number of bits of output not provided are between 0 and 7, where they
   await more bits to join them in order to fill out a full byte.  If pending
   or bits are NULL, then those values are not set.

     deflatePending returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if the source
   stream state was inconsistent.
 */
pub int deflatePending(stream.z_stream *strm, unsigned *pending, int *bits) {
    if (deflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    if (pending != NULL) {
        *pending = strm->state->pending;
    }
    if (bits != NULL) {
        *bits = strm->state->bi_valid;
    }
    return stream.Z_OK;
}

/*
     deflatePrime() inserts bits in the deflate output stream.  The intent
   is that this function is used to start off the deflate output with the bits
   leftover from a previous deflate stream when appending to it.  As such, this
   function can only be used for raw deflate, and must be used before the first
   deflate() call after a deflateInit2() or deflateReset().  bits must be less
   than or equal to 16, and that many of the least significant bits of value
   will be inserted in the output.

     deflatePrime returns stream.Z_OK if success, stream.Z_BUF_ERROR if there was not enough
   room in the internal buffer to insert the bits, or stream.Z_STREAM_ERROR if the
   source stream state was inconsistent.
*/
pub int deflatePrime(stream.z_stream *strm, int bits, int value) {
    if (deflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    stream.deflate_state *s = strm->state;
    int put;
    if (bits < 0 || bits > 16
        || s->sym_buf < s->pending_out + ((Buf_size + 7) >> 3)) {
        return stream.Z_BUF_ERROR;
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
    return stream.Z_OK;
}

/*
     Dynamically update the compression level and compression strategy.  The
   interpretation of level and strategy is as in deflateInit2().  This can be
   used to switch between compression and straight copy of the input data, or
   to switch to a different kind of input data requiring a different strategy.
   If the compression approach (which is a function of the level) or the
   strategy is changed, and if there have been any deflate() calls since the
   state was initialized or reset, then the input available so far is
   compressed with the old level and strategy using deflate(strm, stream.Z_BLOCK).
   There are three approaches for the compression levels 0, 1..3, and 4..9
   respectively.  The new level and strategy will take effect at the next call
   of deflate().

     If a deflate(strm, stream.Z_BLOCK) is performed by deflateParams(), and it does
   not have enough output space to complete, then the parameter change will not
   take effect.  In this case, deflateParams() can be called again with the
   same parameters and more output space to try again.

     In order to assure a change in the parameters on the first try, the
   deflate stream should be flushed using deflate() with stream.Z_BLOCK or other flush
   request until strm.avail_out is not zero, before calling deflateParams().
   Then no more input data should be provided before the deflateParams() call.
   If this is done, the old level and strategy will be applied to the data
   compressed before deflateParams(), and the new level and strategy will be
   applied to the the data compressed after deflateParams().

     deflateParams returns stream.Z_OK on success, stream.Z_STREAM_ERROR if the source stream
   state was inconsistent or if a parameter was invalid, or stream.Z_BUF_ERROR if
   there was not enough output space to complete the compression of the
   available input data before a change in the strategy or approach.  Note that
   in the case of a stream.Z_BUF_ERROR, the parameters are not changed.  A return
   value of stream.Z_BUF_ERROR is not fatal, in which case deflateParams() can be
   retried with more output space.
*/
pub int deflateParams(
    stream.z_stream *strm,
    int level,
    int strategy
) {
    stream.deflate_state *s;
    compress_func func;

    if (deflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    s = strm->state;

    if (level == Z_DEFAULT_COMPRESSION) level = 6;
    if (level < 0 || level > 9 || strategy < 0 || strategy > Z_FIXED) {
        return stream.Z_STREAM_ERROR;
    }
    func = configuration_table[s->level].func;

    if ((strategy != s->strategy || func != configuration_table[level].func) &&
        s->last_flush != -2) {
        /* Flush the last buffer: */
        int err = deflate(strm, stream.Z_BLOCK);
        if (err == stream.Z_STREAM_ERROR)
            return err;
        if (strm->avail_in || (s->strstart - s->block_start) + s->lookahead)
            return stream.Z_BUF_ERROR;
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
    return stream.Z_OK;
}

/*
     Fine tune deflate's internal compression parameters.  This should only be
   used by someone who understands the algorithm used by zlib's deflate for
   searching for the best matching string, and even then only by the most
   fanatic optimizer trying to squeeze out the last compressed bit for their
   specific input data.  Read the deflate.c source code for the meaning of the
   max_lazy, good_length, nice_length, and max_chain parameters.

     deflateTune() can be called after deflateInit() or deflateInit2(), and
   returns stream.Z_OK on success, or stream.Z_STREAM_ERROR for an invalid deflate stream.
 */
pub int deflateTune(
    stream.z_stream *strm,
    int good_length,
    int max_lazy,
    int nice_length,
    int max_chain
) {
    stream.deflate_state *s;
    if (deflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    s = strm->state;
    s->good_match = (uint32_t)good_length;
    s->max_lazy_match = (uint32_t)max_lazy;
    s->nice_match = nice_length;
    s->max_chain_length = (uint32_t)max_chain;
    return stream.Z_OK;
}

/*
     deflateBound() returns an upper bound on the compressed size after
   deflation of sourceLen bytes.  It must be called after deflateInit() or
   deflateInit2(), and after deflateSetHeader(), if used.  This would be used
   to allocate an output buffer for deflation in a single pass, and so would be
   called before deflate().  If that first deflate() call is provided the
   sourceLen input bytes, an output buffer allocated to the size returned by
   deflateBound(), and the flush value stream.Z_FINISH, then deflate() is guaranteed
   to return stream.Z_STREAM_END.  Note that it is possible for the compressed size to
   be larger than the value returned by deflateBound() if flush options other
   than stream.Z_FINISH or stream.Z_NO_FLUSH are used.
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
pub uint32_t deflateBound(stream.z_stream *strm, uint32_t sourceLen) {
    stream.deflate_state *s;
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
void putShortMSB(stream.deflate_state *s, uint32_t b) {
    put_byte(s, (uint8_t)(b >> 8));
    put_byte(s, (uint8_t)(b & 0xff));
}

/* =========================================================================
 * Flush as much pending output as possible. All deflate() output, except for
 * some deflate_stored() output, goes through this function so some
 * applications may wish to modify it to avoid allocating a large
 * strm->next_out buffer and copying into it. (See also read_buf()).
 */
void flush_pending(stream.z_stream *strm) {
    unsigned len;
    stream.deflate_state *s = strm->state;

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
void HCRC_UPDATE(stream.z_stream *strm, uint32_t beg) {
    stream.deflate_state *s = strm->state;
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
  == 0), or after each call of deflate().  If deflate returns stream.Z_OK and with
  zero avail_out, it must be called again after making room in the output
  buffer because there might be more output pending. See deflatePending(),
  which can be used if desired to determine whether or not there is more output
  in that case.

    Normally the parameter flush is set to stream.Z_NO_FLUSH, which allows deflate to
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

    If flush is set to stream.Z_BLOCK, a deflate block is completed and emitted, as
  for Z_SYNC_FLUSH, but the output is not aligned on a byte boundary, and up to
  seven bits of the current block are held to be written as the next byte after
  the next deflate block is completed.  In this case, the decompressor may not
  be provided enough bits at this point in order to complete decompression of
  the data provided so far to the compressor.  It may need to wait for the next
  block to be emitted.  This is for advanced applications that need to control
  the emission of deflate blocks.

    If flush is set to stream.Z_FULL_FLUSH, all output is flushed as with
  Z_SYNC_FLUSH, and the compression state is reset so that decompression can
  restart from this point if previous compressed data has been damaged or if
  random access is desired.  Using stream.Z_FULL_FLUSH too often can seriously degrade
  compression.

    If deflate returns with avail_out == 0, this function must be called again
  with the same value of the flush parameter and more output space (updated
  avail_out), until the flush is complete (deflate returns with non-zero
  avail_out).  In the case of a stream.Z_FULL_FLUSH or Z_SYNC_FLUSH, make sure that
  avail_out is greater than six to avoid repeated flush markers due to
  avail_out == 0 on return.

    If the parameter flush is set to stream.Z_FINISH, pending input is processed,
  pending output is flushed and deflate returns with stream.Z_STREAM_END if there was
  enough output space.  If deflate returns with stream.Z_OK or stream.Z_BUF_ERROR, this
  function must be called again with stream.Z_FINISH and more output space (updated
  avail_out) but no more input data, until it returns with stream.Z_STREAM_END or an
  error.  After deflate has returned stream.Z_STREAM_END, the only possible operations
  on the stream are deflateReset or deflateEnd.

    stream.Z_FINISH can be used in the first deflate call after deflateInit if all the
  compression is to be done in a single step.  In order to complete in one
  call, avail_out must be at least the value returned by deflateBound (see
  below).  Then deflate is guaranteed to return stream.Z_STREAM_END.  If not enough
  output space is provided, deflate will not return stream.Z_STREAM_END, and it must
  be called again as described above.

    deflate() sets strm->adler to the Adler-32 checksum of all input read
  so far (that is, total_in bytes).  If a gzip stream is being generated, then
  strm->adler will be the CRC-32 checksum of the input read so far.  (See
  deflateInit2 below.)

    deflate() may update strm->data_type if it can make a good guess about
  the input data type (stream.Z_BINARY or stream.Z_TEXT).  If in doubt, the data is
  considered binary.  This field is only for information purposes and does not
  affect the compression algorithm in any manner.

    deflate() returns stream.Z_OK if some progress has been made (more input
  processed or more output produced), stream.Z_STREAM_END if all input has been
  consumed and all output has been produced (only when flush is set to
  stream.Z_FINISH), stream.Z_STREAM_ERROR if the stream state was inconsistent (for example
  if next_in or next_out was NULL or the state was inadvertently written over
  by the application), or stream.Z_BUF_ERROR if no progress is possible (for example
  avail_in or avail_out was zero).  Note that stream.Z_BUF_ERROR is not fatal, and
  deflate() can be called again with more input and more output space to
  continue compressing.
*/
pub int deflate(stream.z_stream *strm, int flush) {
    if (deflateStateCheck(strm) || flush > stream.Z_BLOCK || flush < 0) {
        return stream.Z_STREAM_ERROR;
    }

    stream.deflate_state *s = strm->state;

    if (strm->next_out == NULL ||
        (strm->avail_in != 0 && strm->next_in == NULL) ||
        (s->status == FINISH_STATE && flush != stream.Z_FINISH)) {
        return ERR_RETURN(strm, stream.Z_STREAM_ERROR);
    }
    if (strm->avail_out == 0) {
        return ERR_RETURN(strm, stream.Z_BUF_ERROR);
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
            return stream.Z_OK;
        }
    }
    else if (strm->avail_in == 0 && RANK(flush) <= RANK(old_flush) && flush != stream.Z_FINISH) {
        /* Make sure there is something to do and avoid duplicate consecutive
        flushes. For repeated and useless calls with stream.Z_FINISH, we keep
        returning stream.Z_STREAM_END instead of stream.Z_BUF_ERROR. */
        return ERR_RETURN(strm, stream.Z_BUF_ERROR);
    }

    /* User must not provide more input after the first FINISH: */
    if (s->status == FINISH_STATE && strm->avail_in != 0) {
        return ERR_RETURN(strm, stream.Z_BUF_ERROR);
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
            return stream.Z_OK;
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
                return stream.Z_OK;
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
                    return stream.Z_OK;
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
                        return stream.Z_OK;
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
                        return stream.Z_OK;
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
                    return stream.Z_OK;
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
            return stream.Z_OK;
        }
    }

    /* Start a new block or continue the current one.
     */
    if (strm->avail_in != 0 || s->lookahead != 0 ||
        (flush != stream.Z_NO_FLUSH && s->status != FINISH_STATE)) {
        int bstate;

        if (s->level == 0) {
            bstate = deflate_stored(s, flush);
        } else if (s->strategy == Z_HUFFMAN_ONLY) {
            bstate = deflate_huff(s, flush);
        } else if (s->strategy == Z_RLE) {
            bstate = s->strategy == deflate_rle(s, flush);
        } else {
            bstate = (*(configuration_table[s->level].func))(s, flush);
        }

        if (bstate == finish_started || bstate == finish_done) {
            s->status = FINISH_STATE;
        }
        if (bstate == need_more || bstate == finish_started) {
            if (strm->avail_out == 0) {
                s->last_flush = -1; /* avoid BUF_ERROR next call, see above */
            }
            return stream.Z_OK;
            /* If flush != stream.Z_NO_FLUSH && avail_out == 0, the next call
             * of deflate should use the same flush parameter to make sure
             * that the flush is complete. So we don't have to output an
             * empty block here, this will be done at next call. This also
             * ensures that for a very small output buffer, we emit at most
             * one empty block.
             */
        }
        if (bstate == block_done) {
            if (flush == stream.Z_PARTIAL_FLUSH) {
                _tr_align(s);
            } else if (flush != stream.Z_BLOCK) { /* FULL_FLUSH or SYNC_FLUSH */
                _tr_stored_block(s, (char*)0, 0L, 0);
                /* For a full flush, this empty block will be recognized
                 * as a special marker by inflate_sync().
                 */
                if (flush == stream.Z_FULL_FLUSH) {
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
              return stream.Z_OK;
            }
        }
    }

    if (flush != stream.Z_FINISH) return stream.Z_OK;
    if (s->wrap <= 0) return stream.Z_STREAM_END;

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
        return stream.Z_OK;
    }
    return stream.Z_STREAM_END;
}

int get_dunno_what1(stream.deflate_state *s) {
    if (s->level == 9) { return 2; }
    if (s->strategy >= Z_HUFFMAN_ONLY || s->level < 2) {
        return 4;
    }
    return 0;
}

/*
 * Frees all dynamically allocated data structures for this stream.
 * Discards any unprocessed input and does not flush any pending output.
 * Returns stream.Z_OK if success,
 * stream.Z_STREAM_ERROR if the stream state was inconsistent,
 * stream.Z_DATA_ERROR if the stream was freed prematurely (some input or output was discarded).
 * In the error case, msg may be set to a static string (which must not be deallocated).
 */
pub int deflateEnd(stream.z_stream *strm) {
    if (deflateStateCheck(strm)) {
        return stream.Z_STREAM_ERROR;
    }
    int status = strm->state->status;

    /* Deallocate in reverse order of allocations: */
    if (strm->state->pending_buf) {
        stream.ZFREE(strm, strm->state->pending_buf);
    }
    if (strm->state->head) {
        stream.ZFREE(strm, strm->state->head);
    }
    if (strm->state->prev) {
        stream.ZFREE(strm, strm->state->prev);
    }
    if (strm->state->window) {
        stream.ZFREE(strm, strm->state->window);
    }

    stream.ZFREE(strm, strm->state);
    strm->state = NULL;

    if (status == BUSY_STATE) {
        return stream.Z_DATA_ERROR;
    }
    return stream.Z_OK;
}

/*
     Sets the destination stream as a complete copy of the source stream.

     This function can be useful when several compression strategies will be
   tried, for example when there are several ways of pre-processing the input
   data with a filter.  The streams that will be discarded should then be freed
   by calling deflateEnd.  Note that deflateCopy duplicates the internal
   compression state which can be quite large, so this strategy is slow and can
   consume lots of memory.

     deflateCopy returns stream.Z_OK if success, stream.Z_MEM_ERROR if there was not
   enough memory, stream.Z_STREAM_ERROR if the source stream state was inconsistent
   (such as zalloc being NULL).  msg is left unchanged in both source and
   destination.
*/
/* =========================================================================
 * Copy the source state to the destination state.
 * To simplify the source, this is not supported for 16-bit MSDOS (which
 * doesn't have enough memory anyway to duplicate compression states).
 */
pub int deflateCopy(stream.z_stream * dest, source) {
    stream.deflate_state *ds;
    stream.deflate_state *ss;


    if (deflateStateCheck(source) || dest == NULL) {
        return stream.Z_STREAM_ERROR;
    }

    ss = source->state;

    memcpy((void *)dest, (void *)source, sizeof(stream.z_stream));

    ds = ZALLOC(dest, 1, sizeof(stream.deflate_state));
    if (ds == NULL) return stream.Z_MEM_ERROR;
    dest->state = ds;
    memcpy((void *)ds, (void *)ss, sizeof(stream.deflate_state));
    ds->strm = dest;

    ds->window = (uint8_t *) ZALLOC(dest, ds->w_size, 2*sizeof(uint8_t));
    ds->prev   = (uint16_t *)  ZALLOC(dest, ds->w_size, sizeof(uint16_t));
    ds->head   = (uint16_t *)  ZALLOC(dest, ds->hash_size, sizeof(uint16_t));
    ds->pending_buf = (uint8_t *) ZALLOC(dest, ds->lit_bufsize, 4);

    if (ds->window == NULL || ds->prev == NULL || ds->head == NULL ||
        ds->pending_buf == NULL) {
        deflateEnd (dest);
        return stream.Z_MEM_ERROR;
    }
    /* following memcpy do not work for 16-bit MSDOS */
    memcpy(ds->window, ss->window, ds->w_size * 2 * sizeof(uint8_t));
    memcpy((void *)ds->prev, (void *)ss->prev, ds->w_size * sizeof(uint16_t));
    memcpy((void *)ds->head, (void *)ss->head, ds->hash_size * sizeof(uint16_t));
    memcpy(ds->pending_buf, ss->pending_buf, (uint32_t)ds->pending_buf_size);

    ds->pending_out = ds->pending_buf + (ss->pending_out - ss->pending_buf);
    ds->sym_buf = ds->pending_buf + ds->lit_bufsize;

    ds->l_desc.dyn_tree = ds->dyn_ltree;
    ds->d_desc.dyn_tree = ds->dyn_dtree;
    ds->bl_desc.dyn_tree = ds->bl_tree;

    return stream.Z_OK;
}

/* ===========================================================================
 * Read a new buffer from the current input stream, update the adler32
 * and total number of bytes read.  All deflate() input goes through
 * this function so some applications may wish to modify it to avoid
 * allocating a large strm->next_in buffer and copying from it.
 * (See also flush_pending()).
 */
unsigned read_buf(
    stream.z_stream *strm,
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
void lm_init(stream.deflate_state *s) {
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
    stream.deflate_state *s,
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
void check_match(stream.deflate_state *s, uint32_t start, match, int length) {
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
void fill_window(stream.deflate_state *s) {
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
void FLUSH_BLOCK_ONLY(stream.deflate_state *s, int last) {
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
void FLUSH_BLOCK(stream.deflate_state *s, int last) {
    FLUSH_BLOCK_ONLY(s, last);
    if (s->strm->avail_out == 0) {
        if (last) {
            return finish_started;
        }
        return need_more;
    }
    return NO_RETURN;
}

/* Maximum stored block length in deflate format (not including header). */
#define MAX_STORED 65535


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
int deflate_stored(stream.deflate_state *s, int flush) {
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
        if (len < min_block && ((len == 0 && flush != stream.Z_FINISH) ||
                                flush == stream.Z_NO_FLUSH ||
                                len != left + s->strm->avail_in))
            break;

        /* Make a dummy stored block in pending to get the header bytes,
         * including any pending bits. This also updates the debugging counts.
         */
        if (s->strm->avail_in) {
            last = flush == stream.Z_FINISH && len == left + 1;
        } else {
            last = flush == stream.Z_FINISH && len == left;
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
    if (flush != stream.Z_NO_FLUSH && flush != stream.Z_FINISH &&
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
        ((left || flush == stream.Z_FINISH) && flush != stream.Z_NO_FLUSH &&
         s->strm->avail_in == 0 && left <= have)) {
        len = min(left, have);
        last = 0;
        if (flush == stream.Z_FINISH && s->strm->avail_in == 0 && len == left) {
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
int deflate_fast(stream.deflate_state *s, bool flush) {
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
            if (s->lookahead < MIN_LOOKAHEAD && flush == stream.Z_NO_FLUSH) {
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

    if (flush == stream.Z_FINISH) {
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
int deflate_slow(stream.deflate_state *s, int flush) {
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
            if (s->lookahead < MIN_LOOKAHEAD && flush == stream.Z_NO_FLUSH) {
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
    assert.Assert(flush != stream.Z_NO_FLUSH, "no flush");
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
    if (flush == stream.Z_FINISH) {
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
 * For Z_RLE, simply look for runs of bytes, generate matches only of distance
 * one.  Do not maintain a hash table.  (It will be regenerated if this run of
 * deflate switches away from Z_RLE.)
 */
int deflate_rle(stream.deflate_state *s, int flush) {
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
            if (s->lookahead <= MAX_MATCH && flush == stream.Z_NO_FLUSH) {
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
    if (flush == stream.Z_FINISH) {
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
int deflate_huff(stream.deflate_state *s, int flush) {
    int bflush;             /* set if current block must be flushed */

    while (true) {
        /* Make sure that we have a literal to write. */
        if (s->lookahead == 0) {
            fill_window(s);
            if (s->lookahead == 0) {
                if (flush == stream.Z_NO_FLUSH)
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
    if (flush == stream.Z_FINISH) {
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
int ERR_RETURN(stream.z_stream *strm, int err) {
    strm->msg = ERR_MSG(err);
    return err;
}

const char *ERR_MSG(int err) {
    return z_errmsg[stream.Z_NEED_DICT-(err)];
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

#define LITERALS  256 /* number of literal bytes 0..255 */
#define LENGTH_CODES 29 /* number of length codes, not counting the special END_BLOCK code */
#define L_CODES (LITERALS+1+LENGTH_CODES)
/* number of Literal or Length codes, including the END_BLOCK code */

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
const int extra_lbits[LENGTH_CODES] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};

/* extra bits for each distance code */
const int extra_dbits[D_CODES] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

/* extra bits for each bit length code */
const int extra_blbits[BL_CODES] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7};

/* The lengths of the bit length codes are sent in order of decreasing
 * probability, to avoid transmitting the lengths for unused bit length codes.
 */
const uint8_t bl_order[BL_CODES] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

#define DIST_CODE_LEN  512 /* see definition of array dist_code below */

bool GEN_TREES_H = false;

/* The static literal tree. Since the bit lengths are imposed, there is no
 * need for the L_CODES extra codes used during heap construction. However
 * The codes 286 and 287 are needed to build a canonical tree (see _tr_init
 * below).
 */
stream.ct_data static_ltree[L_CODES+2] = {
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
stream.ct_data static_dtree[D_CODES] = {
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

const stream.static_tree_desc static_l_desc = {static_ltree, extra_lbits, LITERALS+1, L_CODES, MAX_BITS};
const stream.static_tree_desc static_d_desc = {static_dtree, extra_dbits, 0,          D_CODES, MAX_BITS};
const stream.static_tree_desc static_bl_desc = {NULL, extra_blbits, 0,   BL_CODES, MAX_BL_BITS};

void send_code(stream.deflate_state *s, int c, void *tree) {
    trace.Tracevv(stderr, "\ncd %3d ",c);
    send_bits(s, tree[c].fc.code, tree[c].dl.len);
}

/* ===========================================================================
 * Output a short LSB first on the stream.
 * IN assertion: there is enough room in pendingBuf.
 */
void put_short(stream.deflate_state *s, uint16_t w) {
    put_byte(s, (uint8_t)((w) & 0xff));
    put_byte(s, (uint8_t)((uint16_t)(w) >> 8));
}

/* ===========================================================================
 * Send a value on a given number of bits.
 * IN assertion: length <= 16 and value fits in length bits.
 */
void send_bits(stream.deflate_state *s, int value, int length) {
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
    for (code = 0; code < LENGTH_CODES-1; code++) {
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
    for (code = 0 ; code < 16; code++) {
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
void _tr_init(stream.deflate_state *s) {
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
void init_block(stream.deflate_state *s) {
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
bool smaller(stream.ct_data tree, int n, m, depth) {
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
void pqdownheap(stream.deflate_state *s, stream.ct_data *tree, int k) {
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
void gen_bitlen(stream.deflate_state *s, stream.tree_desc *desc) {
    stream.ct_data *tree        = desc->dyn_tree;
    int max_code         = desc->max_code;
    const stream.ct_data *stree = desc->stat_desc->static_tree;
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
    stream.ct_data *tree,
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
void build_tree(stream.deflate_state *s, stream.tree_desc *desc) {
    stream.ct_data *tree         = desc->dyn_tree;
    stream.ct_data *stree  = desc->stat_desc->static_tree;
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
// stream.deflate_state *s;
//     ct_data *tree;   /* the tree to be scanned */
//     int max_code;    /* and its largest code of non zero frequency */
void scan_tree(
    stream.deflate_state *s,
    stream.ct_data *tree,
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
//  stream.deflate_state *s;
//     ct_data *tree; /* the tree to be scanned */
//     int max_code;       /* and its largest code of non zero frequency */
void send_tree(
    stream.deflate_state *s,
    stream.ct_data *tree,
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
int build_bl_tree(stream.deflate_state *s) {
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
//  stream.deflate_state *s;
//     int lcodes, dcodes, blcodes; /* number of codes for each tree */
void send_all_trees(
    stream.deflate_state *s,
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
//  stream.deflate_state *s;
//   *buf;       /* input block */
//     uint32_t stored_len;   /* length of input block */
//     int last;         /* one if this is the last block for a file */
void  _tr_stored_block(
    stream.deflate_state *s,
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
void  _tr_flush_bits(stream.deflate_state *s) {
    bi_flush(s);
}

/* ===========================================================================
 * Send one empty static block to give enough lookahead for inflate.
 * This takes 10 bits, of which 7 may remain in the bit buffer.
 */
void  _tr_align(stream.deflate_state *s) {
    send_bits(s, STATIC_TREES<<1, 3);
    send_code(s, END_BLOCK, static_ltree);
    s->compressed_len += 10L; /* 3 for block type, 7 for EOB */
    bi_flush(s);
}

/* ===========================================================================
 * Determine the best encoding for the current block: dynamic trees, static
 * trees or store, and write out the encoded block.
 */
// stream.deflate_state *s;
// *buf;       /* input block, or NULL if too old */
// uint32_t stored_len;   /* length of input block */
// int last;         /* one if this is the last block for a file */
void  _tr_flush_block(
    stream.deflate_state *s,
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
        if (s->strm->data_type == stream.Z_UNKNOWN) {
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
// stream.deflate_state *s;
// unsigned dist;  /* distance of matched string */
// unsigned lc;    /* match length - MIN_MATCH or unmatched char (dist==0) */
int  _tr_tally(
    stream.deflate_state *s,
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
//  stream.deflate_state *s;
//     const ct_data *ltree; /* literal tree */
//     const ct_data *dtree; /* distance tree */
void compress_block(
    stream.deflate_state *s,
    const stream.ct_data *ltree,
    const stream.ct_data *dtree
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
int detect_data_type(stream.deflate_state *s) {
    /* block_mask is the bit mask of block-listed bytes
     * set bits 0..6, 14..25, and 28..31
     * 0xf3ffc07f = binary 11110011111111111100000001111111
     */
    uint32_t block_mask = 0xf3ffc07fUL;

    /* Check for non-textual ("block-listed") bytes. */
    for (int n = 0; n <= 31; n++) {
        if ((block_mask & 1) && (s->dyn_ltree[n].fc.freq != 0)) {
            return stream.Z_BINARY;
        }
        block_mask >>= 1;
    }

    /* Check for textual ("allow-listed") bytes. */
    if (s->dyn_ltree[9].fc.freq != 0 || s->dyn_ltree[10].fc.freq != 0
            || s->dyn_ltree[13].fc.freq != 0) {
        return stream.Z_TEXT;
    }
    for (int n = 32; n < LITERALS; n++) {
        if (s->dyn_ltree[n].fc.freq != 0) {
            return stream.Z_TEXT;
        }
    }

    /* There are no "block-listed" or "allow-listed" bytes:
     * this stream either is empty or has tolerated ("gray-listed") bytes only.
     */
    return stream.Z_BINARY;
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
void bi_flush(stream.deflate_state *s) {
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
void bi_windup(stream.deflate_state *s) {
    if (s->bi_valid > 8) {
        put_short(s, s->bi_buf);
    } else if (s->bi_valid > 0) {
        put_byte(s, (uint8_t)s->bi_buf);
    }
    s->bi_buf = 0;
    s->bi_valid = 0;
    s->bits_sent = (s->bits_sent + 7) & ~7;
}
