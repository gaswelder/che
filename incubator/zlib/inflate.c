/* inflate.c -- zlib decompression
 * Copyright (C) 1995-2022 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#import stream.c
#import trace.c
#import adler32

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
} code;
/* op values as set by inflate_table():
    00000000 - literal
    0000tttt - table link, tttt != 0 is the number of table index bits
    0001eeee - length or distance, eeee is the number of extra bits
    01100000 - end of block
    01000000 - invalid code
 */


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
    code *lencode;    /* starting table for length/literal codes */
    code *distcode;   /* starting table for distance codes */
    unsigned lenbits;           /* index bits for lencode */
    unsigned distbits;          /* index bits for distcode */
        /* dynamic table building */
    unsigned ncode;             /* number of code length code lengths */
    unsigned nlen;              /* number of length code lengths */
    unsigned ndist;             /* number of distance code lengths */
    unsigned have;              /* number of code lengths in lens[] */
    code *next;             /* next available space in codes[] */
    uint16_t lens[320];   /* temporary storage for code lengths */
    uint16_t work[288];   /* work area for code table building */
    code codes[ENOUGH];         /* space for code tables */
    int sane;                   /* if false, allow invalid distance too far */
    int back;                   /* bits back of last unprocessed length/lit */
    unsigned was;               /* initial length of match */
} inflate_state;


int inflateStateCheck(stream.z_stream *strm) {
    if (!strm || !strm->zalloc || !strm->zfree) {
        return 1;
    }
    inflate_state *state = strm->state;
    if (!state || state->strm != strm || state->mode < HEAD || state->mode > SYNC) {
        return 1;
    }
    return 0;
}

pub int inflateResetKeep(stream.z_stream *strm) {
    inflate_state *state;

    if (inflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
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
    return stream.Z_OK;
}


/*
     This function is equivalent to inflateEnd followed by inflateInit,
   but does not free and reallocate the internal decompression state.  The
   stream will keep attributes that may have been set by inflateInit2.

     inflateReset returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if the source
   stream state was inconsistent (such as zalloc or state being NULL).
*/
pub int inflateReset(stream.z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return stream.Z_STREAM_ERROR;
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

     inflateReset2 returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if the source
   stream state was inconsistent (such as zalloc or state being NULL), or if
   the windowBits parameter is invalid.
*/
pub int inflateReset2(stream.z_stream *strm, int windowBits) {
    /* get the state */
    if (inflateStateCheck(strm)) {
        return stream.Z_STREAM_ERROR;
    }
    inflate_state *state = strm->state;

    int wrap = 0;
    /* extract wrap request from windowBits parameter */
    if (windowBits < 0) {
        if (windowBits < -15)
            return stream.Z_STREAM_ERROR;
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
        return stream.Z_STREAM_ERROR;
    if (state->window != NULL && state->wbits != (unsigned)windowBits) {
        stream.ZFREE(strm, state->window);
        state->window = NULL;
    }

    /* update state and reset the rest of it */
    state->wrap = wrap;
    state->wbits = (unsigned)windowBits;
    return inflateReset(strm);
}


/*
     This is another version of inflateInit with an extra parameter.  The
   fields next_in, avail_in, zalloc, zfree and opaque must be initialized
   before by the caller.

     The windowBits parameter is the base two logarithm of the maximum window
   size (the size of the history buffer).  It should be in the range 8..15 for
   this version of the library.  The default value is 15 if inflateInit is used
   instead.  windowBits must be greater than or equal to the windowBits value
   provided to deflateInit2() while compressing, or it must be equal to 15 if
   deflateInit2() was not used.  If a compressed stream with a larger window
   size is given as input, inflate() will return with the error code
   stream.Z_DATA_ERROR instead of trying to allocate a larger window.

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
   return a stream.Z_DATA_ERROR).  If a gzip stream is being decoded, strm->adler is a
   CRC-32 instead of an Adler-32.  Unlike the gunzip utility and gzread() (see
   below), inflate() will *not* automatically decode concatenated gzip members.
   inflate() will return stream.Z_STREAM_END at the end of the gzip member.  The state
   would need to be reset to continue decoding a subsequent gzip member.  This
   *must* be done if there is more data after a gzip member, in order for the
   decompression to be compliant with the gzip standard (RFC 1952).

     inflateInit2 returns stream.Z_OK if success, stream.Z_MEM_ERROR if there was not enough
   memory, Z_VERSION_ERROR if the zlib library version is incompatible with the
   version assumed by the caller, or stream.Z_STREAM_ERROR if the parameters are
   invalid, such as a null pointer to the structure.  msg is set to null if
   there is no error message.  inflateInit2 does not perform any decompression
   apart from possibly reading the zlib header if present: actual decompression
   will be done by inflate().  (So next_in and avail_in may be modified, but
   next_out and avail_out are unused and unchanged.) The current implementation
   of inflateInit2() does not process any header information -- that is
   deferred until inflate() is called.
*/
pub int inflateInit2_(
    stream.z_stream *strm,
    int windowBits,
    const char *version,
    int stream_size
) {
    int ret;
    inflate_state *state;

    if (version == NULL || version[0] != ZLIB_VERSION[0] ||
        stream_size != (int)(sizeof(stream.z_stream)))
        return Z_VERSION_ERROR;
    if (strm == NULL) return stream.Z_STREAM_ERROR;
    strm->msg = NULL;                 /* in case we return an error */
    if (strm->zalloc == NULL) {
        strm->zalloc = zcalloc;
        strm->opaque = NULL;
    }
    if (strm->zfree == NULL) {
        strm->zfree = zcfree;
    }
    state = stream.ZALLOC(strm, 1, sizeof(inflate_state));
    if (state == NULL) return stream.Z_MEM_ERROR;
    trace.Tracev(stderr, "inflate: allocated\n");
    strm->state = state;
    state->strm = strm;
    state->window = NULL;
    state->mode = HEAD;     /* to pass state test in inflateReset2() */
    ret = inflateReset2(strm, windowBits);
    if (ret != stream.Z_OK) {
        stream.ZFREE(strm, state);
        strm->state = NULL;
    }
    return ret;
}

/*
 * Initializes state for decompression.
 */
pub int inflateInit(stream.z_stream *strm) {
    /*
    The provided stream is not read or consumed, actual decompression will be done by inflate().

    The fields next_in, avail_in, zalloc, zfree and opaque must be initialized before by the caller. 

    The allocation of a sliding window will be deferred to the first call of inflate (if the decompression does not complete on the
    first call).  If zalloc and zfree are set to NULL, inflateInit updates
    them to use default allocation functions.

    inflateInit returns stream.Z_OK if success, stream.Z_MEM_ERROR if there was not enough
    memory, Z_VERSION_ERROR if the zlib library version is incompatible with the
    version assumed by the caller, or stream.Z_STREAM_ERROR if the parameters are
    invalid, such as a null pointer to the structure.  msg is set to null if
    there is no error message.

    So next_in, and avail_in, next_out, and avail_out are unused and unchanged.  The current
    implementation of inflateInit() does not process any header information --
    that is deferred until inflate() is called.
    */
    return inflateInit2_(strm, DEF_WBITS, ZLIB_VERSION, sizeof(stream.z_stream));
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

     inflatePrime returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if the source
   stream state was inconsistent.
*/
pub int inflatePrime(
    stream.z_stream *strm,
    int bits,
    int value    
) {
    inflate_state *state;

    if (inflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    if (bits < 0) {
        state->hold = 0;
        state->bits = 0;
        return stream.Z_OK;
    }
    if (bits > 16 || state->bits + (uint32_t)bits > 32) return stream.Z_STREAM_ERROR;
    value &= (1L << bits) - 1;
    state->hold += (unsigned)value << state->bits;
    state->bits += (uint32_t)bits;
    return stream.Z_OK;
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
code *lenfix = NULL;
code *distfix = NULL;
code fixed[544] = {};
code *fixedtables_next = NULL;
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
    stream.z_stream *strm,
    const uint8_t *end,
    unsigned copy
)

{
    inflate_state *state;
    unsigned dist;

    state = (inflate_state *)strm->state;

    /* if it hasn't been done already, allocate space for the window */
    if (state->window == NULL) {
        state->window = stream.ZALLOC(strm, 1U << state->wbits, sizeof(uint8_t));
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

/* Macros for inflate(): */

/* check function to use adler32.adler32() for zlib or crc32() for gzip */
int UPDATE_CHECK(uint32_t check, char *buf, size_t len) {
    if (state->flags) {
        return crc32(check, buf, len);
    }
    return adler32.adler32(check, buf, len);
}

/* check macros for header crc */
void CRC2(uint32_t check, uint16_t word) {
    hbuf[0] = (uint8_t)(word);
    hbuf[1] = (uint8_t)((word) >> 8);
    check = crc32(check, hbuf, 2);
}

void CRC4(uint32_t check, uint32_t word) {
    hbuf[0] = (uint8_t)(word);
    hbuf[1] = (uint8_t)((word) >> 8);
    hbuf[2] = (uint8_t)((word) >> 16);
    hbuf[3] = (uint8_t)((word) >> 24);
    check = crc32(check, hbuf, 4);
}

/* Load registers with state in inflate() for speed */
void LOAD(ctx_t *ctx) {
    put = strm->next_out;
    left = strm->avail_out;
    next = strm->next_in;
    have = strm->avail_in;
    ctx->hold = state->hold;
    bits = state->bits;
}
/* Restore state from registers in inflate() */
void RESTORE(ctx_t *ctx) {
    strm->next_out = put;
    strm->avail_out = left;
    strm->next_in = next;
    strm->avail_in = have;
    state->hold = ctx->hold;
    state->bits = bits;
}

/* Clear the input bit accumulator */
void INITBITS() {
    ctx->hold = 0;
    bits = 0;
}

/* Get a byte of input into the bit accumulator, or return from inflate()
   if there is no input available. */
void PULLBYTE(ctx_t *ctx) {
    if (ctx->have == 0) {
        return inf_leave();
    }
    ctx->have--;
    uint32_t x = (uint32_t)(*next);
    ctx->next++;
    ctx->hold += x << bits;
    ctx->bits += 8;
}

/* Assure that there are at least n bits in the bit accumulator.  If there is
   not enough available input to do that, then return from inflate(). */
void NEEDBITS(ctx_t *ctx, size_t n) {
    while (ctx->bits < n) {
            PULLBYTE();
    }
}

/* Return the low n bits of the bit accumulator (n < 16) */
int BITS(int n) {
    return ((unsigned) ctx->hold & ((1U << (n)) - 1));
}

/* Remove n bits from the bit accumulator */
void DROPBITS(int n, ctx_t *ctx, int *bits) {
    ctx->hold >>= n;
    *bits -= (unsigned) n;
}


/* permutation of code lengths */
const uint16_t order[19] = 
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/*
    inflate decompresses as much data as possible, and stops when the input
  buffer becomes empty or the output buffer becomes full.  It may introduce
  some output latency (reading input without producing any output) except when
  forced to flush.

  The detailed semantics are as follows.  inflate performs one or both of the
  following actions:

  - Decompress more input starting at next_in and update next_in and avail_in
    accordingly.  If not all input can be processed (because there is not
    enough room in the output buffer), then next_in and avail_in are updated
    accordingly, and processing will resume at this point for the next call of
    inflate().

  - Generate more output starting at next_out and update next_out and avail_out
    accordingly.  inflate() provides as much output as possible, until there is
    no more input data or no more space in the output buffer (see below about
    the flush parameter).

    Before the call of inflate(), the application should ensure that at least
  one of the actions is possible, by providing more input and/or consuming more
  output, and updating the next_* and avail_* values accordingly.  If the
  caller of inflate() does not provide both available input and available
  output space, it is possible that there will be no progress made.  The
  application can consume the uncompressed output when it wants, for example
  when the output buffer is full (avail_out == 0), or after each call of
  inflate().  If inflate returns stream.Z_OK and with zero avail_out, it must be
  called again after making room in the output buffer because there might be
  more output pending.

    The flush parameter of inflate() can be Z_NO_FLUSH, Z_SYNC_FLUSH, stream.Z_FINISH,
  stream.Z_BLOCK, or stream.Z_TREES.  Z_SYNC_FLUSH requests that inflate() flush as much
  output as possible to the output buffer.  stream.Z_BLOCK requests that inflate()
  stop if and when it gets to the next deflate block boundary.  When decoding
  the zlib or gzip format, this will cause inflate() to return immediately
  after the header and before the first block.  When doing a raw inflate,
  inflate() will go ahead and process the first block, and will return when it
  gets to the end of that block, or when it runs out of data.

    The stream.Z_BLOCK option assists in appending to or combining deflate streams.
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

    The stream.Z_TREES option behaves as stream.Z_BLOCK does, but it also returns when the
  end of each deflate block header is reached, before any actual data in that
  block is decoded.  This allows the caller to determine the length of the
  deflate block header for later use in random access within a deflate block.
  256 is added to the value of strm->data_type when inflate() returns
  immediately after reaching the end of the deflate block header.

    inflate() should normally be called until it returns stream.Z_STREAM_END or an
  error.  However if all decompression is to be performed in a single step (a
  single call of inflate), the parameter flush should be set to stream.Z_FINISH.  In
  this case all pending input is processed and all pending output is flushed;
  avail_out must be large enough to hold all of the uncompressed data for the
  operation to complete.  (The size of the uncompressed data may have been
  saved by the compressor for this purpose.)  The use of stream.Z_FINISH is not
  required to perform an inflation in one step.  However it may be used to
  inform inflate that a faster approach can be used for the single inflate()
  call.  stream.Z_FINISH also informs inflate to not maintain a sliding window if the
  stream completes, which reduces inflate's memory footprint.  If the stream
  does not complete, either because not all of the stream is provided or not
  enough output space is provided, then a sliding window will be allocated and
  inflate() can be called again to continue the operation as if Z_NO_FLUSH had
  been used.

     In this implementation, inflate() always flushes as much output as
  possible to the output buffer, and always uses the faster approach on the
  first call.  So the effects of the flush parameter in this implementation are
  on the return value of inflate() as noted below, when inflate() returns early
  when stream.Z_BLOCK or stream.Z_TREES is used, and when inflate() avoids the allocation of
  memory for a sliding window when stream.Z_FINISH is used.

     If a preset dictionary is needed after this call (see inflateSetDictionary
  below), inflate sets strm->adler to the Adler-32 checksum of the dictionary
  chosen by the compressor and returns Z_NEED_DICT; otherwise it sets
  strm->adler to the Adler-32 checksum of all output produced so far (that is,
  total_out bytes) and returns stream.Z_OK, stream.Z_STREAM_END or an error code as described
  below.  At the end of the stream, inflate() checks that its computed Adler-32
  checksum is equal to that saved by the compressor and returns stream.Z_STREAM_END
  only if the checksum is correct.

    inflate() can decompress and check either zlib-wrapped or gzip-wrapped
  deflate data.  The header type is detected automatically, if requested when
  initializing with inflateInit2().  Any information contained in the gzip
  header is not retained unless inflateGetHeader() is used.  When processing
  gzip-wrapped deflate data, strm->adler32.adler32 is set to the CRC-32 of the output
  produced so far.  The CRC-32 is checked against the gzip trailer, as is the
  uncompressed length, modulo 2^32.

    inflate() returns stream.Z_OK if some progress has been made (more input processed
  or more output produced), stream.Z_STREAM_END if the end of the compressed data has
  been reached and all uncompressed output has been produced, Z_NEED_DICT if a
  preset dictionary is needed at this point, stream.Z_DATA_ERROR if the input data was
  corrupted (input stream not conforming to the zlib format or incorrect check
  value, in which case strm->msg points to a string with a more specific
  error), stream.Z_STREAM_ERROR if the stream structure was inconsistent (for example
  next_in or next_out was NULL, or the state was inadvertently written over
  by the application), stream.Z_MEM_ERROR if there was not enough memory, stream.Z_BUF_ERROR
  if no progress was possible or if there was not enough room in the output
  buffer when stream.Z_FINISH is used.  Note that stream.Z_BUF_ERROR is not fatal, and
  inflate() can be called again with more input and more output space to
  continue decompressing.  If stream.Z_DATA_ERROR is returned, the application may
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
   
The NEEDBITS() macro is usually the way the state evaluates whether it can
proceed or should return.

        NEEDBITS(n);
        ... do something with BITS(n) ...
        DROPBITS(n);

   where NEEDBITS(n) either returns from inflate() if there isn't enough
   input left to load n bits into the accumulator, or it continues.  BITS(n)
   gives the low n bits in the accumulator.  When done, DROPBITS(n) drops
   the low n bits off the accumulator.  INITBITS() clears the accumulator
   and sets the number of available bits to zero.  BYTEBITS() discards just
   enough bits to put the accumulator on a byte boundary.  After BYTEBITS()
   and a NEEDBITS(8), then BITS(8) would return the next byte in the stream.

   NEEDBITS(n) uses PULLBYTE to get an available byte of input, or to return
   if there is no input available.  The decoding of variable length codes uses
   PULLBYTE directly in order to pull just enough bytes to decode the next
   code, and no more.

   Some states loop until they get enough input, making sure that enough
   state information is maintained to continue the loop where it left off
   if NEEDBITS() returns in the loop.  For example, want, need, and keep
   would all have to actually be part of the saved state in case NEEDBITS()
   returns:

    case STATEw:
        while (want < need) {
            NEEDBITS(n);
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
   provides the effect documented in zlib.h for stream.Z_FINISH when the entire input
   stream available.  So the only thing the flush parameter actually does is:
   when flush is set to stream.Z_FINISH, inflate() cannot return stream.Z_OK.  Instead it
   will return stream.Z_BUF_ERROR if it has not reached the end of the stream.
 */
typedef {
    uint32_t hold; /* bit buffer */
    unsigned in, out; /* save starting available input and output */
    unsigned have, left; /* available input and output */
    unsigned copy;              /* number of stored or match bytes to copy */
    unsigned len;               /* length to copy for repeats, bits to drop */
    uint8_t *put;     /* next output */
    inflate_state *state;
    stream.z_stream *strm;
} ctx_t;
pub int inflate(stream.z_stream *strm, int flush) {
    const uint8_t *next;    /* next input */
    
    ctx_t ctx = {
        .hold = 0,
        .strm = strm
    };
    unsigned bits;              /* bits in bit buffer */
    
    uint8_t *from;    /* where to copy match bytes from */
    code here;                  /* current decoding table entry */
    code last;                  /* parent table entry */
    
    int ret;                    /* return code */
    uint8_t hbuf[4];      /* buffer for gzip header crc calculation */

    if (inflateStateCheck(strm) || strm->next_out == NULL ||
        (strm->next_in == NULL && strm->avail_in != 0))
    {
        return stream.Z_STREAM_ERROR;
    }

    ctx.state = strm->state;
    if (state->mode == TYPE) state->mode = TYPEDO;      /* skip check */
    LOAD();
    ctx.in = have;
    ctx.out = left;
    ret = stream.Z_OK;
    while (true) {
        int r = 0;
        switch (state->mode) {
            case HEAD:      { r = st_head(); }
            case FLAGS:     { r = st_flags(); }
            case TIME:      { r = st_time(); }
            case OS:        { r = st_os(); }
            case EXLEN:     { r = st_exlen(); }
            case EXTRA:     { r = st_extra(); }
            case NAME:      { r = st_name(); }
            case COMMENT:   { r = st_comment(); }
            case HCRC:      { r = st_hcrc(); }
            case DICTID:    { r = st_dictid(); }
            case DICT:      { r = st_dict(); }
            case TYPE:      { r = st_type(); }
            case TYPEDO:    { r = st_typedo(); }
            case STORED:    { r = st_stored(); }
            case COPY_: {
                state->mode = COPY;
                r = st_copy();
            }
            case COPY:      { r = st_copy(); }
            case TABLE:     { r = st_table(); }
            case LENLENS:   { r = st_lenlens(); }
            case CODELENS:  { r = st_codelens(); }
            case LEN_: {
                state->mode = LEN;
                r = st_len();
            }
            case LEN:       { r = st_len(); }
            case LENEXT:    { r = st_lenext(); }
            case DIST:      { r = st_dist(); }
            case DISTEXT:   { r = st_distext(); }
            case MATCH:     { r = st_match(); }
            case LIT:       { r = st_lit(); }
            case CHECK:     { r = st_check(); }
            case LENGTH:    { r = st_length(); }
            case DONE: {
                ret = stream.Z_STREAM_END;
                r = inf_leave();
            }
            case BAD: {
                ret = stream.Z_DATA_ERROR;
                r = inf_leave();
            }
            case MEM: { r = stream.Z_MEM_ERROR; }
            case SYNC: { r = stream.Z_STREAM_ERROR; }
            default: { r = stream.Z_STREAM_ERROR; }
        }
        if (r != BREAK) {
            return r;
        }
    }
    return inf_leave();
}

enum {
    BREAK = 123456789
};

int st_head(ctx_t *ctx) {
    if (state->wrap == 0) {
        state->mode = TYPEDO;
        break;
    }
    NEEDBITS(ctx, 16);
    if ((state->wrap & 2) && hold == 0x8b1f) {  /* gzip header */
        if (state->wbits == 0)
            state->wbits = 15;
        state->check = crc32(0L, NULL, 0);
        CRC2(state->check, hold);
        INITBITS();
        state->mode = FLAGS;
        break;
    }
    if (state->head != NULL) {
        state->head->done = -1;
    }
    if (!(state->wrap & 1) ||   /* check if zlib header allowed */
        ((BITS(8) << 8) + (hold >> 8)) % 31) {
        strm->msg = (char *)"incorrect header check";
        state->mode = BAD;
        break;
    }
    if (BITS(4) != Z_DEFLATED) {
        strm->msg = (char *)"unknown compression method";
        state->mode = BAD;
        break;
    }
    DROPBITS(4, ctx, &bits);
    len = BITS(4) + 8;
    if (state->wbits == 0)
        state->wbits = len;
    if (len > 15 || len > state->wbits) {
        strm->msg = (char *)"invalid window size";
        state->mode = BAD;
        break;
    }
    state->dmax = 1U << len;
    state->flags = 0;               /* indicate zlib header */
    trace.Tracev(stderr, "inflate:   zlib header ok\n");
    strm->adler = state->check = adler32.adler32(0L, NULL, 0);
    if (hold & 0x200) {
        state->mode = DICTID;
    } else {
        state->mode = TYPE;
    }
    INITBITS();
}

int st_flags(ctx_t *ctx) {
    NEEDBITS(ctx, 16);
    state->flags = (int)(hold);
    if ((state->flags & 0xff) != Z_DEFLATED) {
        strm->msg = (char *)"unknown compression method";
        state->mode = BAD;
        break;
    }
    if (state->flags & 0xe000) {
        strm->msg = (char *)"unknown header flags set";
        state->mode = BAD;
        break;
    }
    if (state->head != NULL)
        state->head->text = (int)((hold >> 8) & 1);
    if ((state->flags & 0x0200) && (state->wrap & 4))
        CRC2(state->check, hold);
    INITBITS();
    state->mode = TIME;
    st_time();
}

int st_time(ctx_t *ctx) {
    NEEDBITS(ctx, 32);
    if (state->head != NULL)
        state->head->time = hold;
    if ((state->flags & 0x0200) && (state->wrap & 4))
        CRC4(state->check, hold);
    INITBITS();
    state->mode = OS;
    st_os();
}

int st_os() {
    NEEDBITS(ctx, 16);
    if (state->head != NULL) {
        state->head->xflags = (int)(hold & 0xff);
        state->head->os = (int)(hold >> 8);
    }
    if ((state->flags & 0x0200) && (state->wrap & 4))
        CRC2(state->check, hold);
    INITBITS();
    state->mode = EXLEN;
    st_exlen();
}

int st_exlen() {
    if (state->flags & 0x0400) {
        NEEDBITS(ctx, 16);
        state->length = (unsigned)(hold);
        if (state->head != NULL)
            state->head->extra_len = (unsigned)hold;
        if ((state->flags & 0x0200) && (state->wrap & 4))
            CRC2(state->check, hold);
        INITBITS();
    }
    else if (state->head != NULL)
        state->head->extra = NULL;
    state->mode = EXTRA;
    st_extra();
}

int st_extra() {
    if (state->flags & 0x0400) {
        copy = state->length;
        if (copy > have) copy = have;
        if (copy) {
            if (state->head != NULL &&
                state->head->extra != NULL &&
                (len = state->head->extra_len - state->length) < state->head->extra_max)
            {
                int x;
                if (len + copy > state->head->extra_max) {
                    x = state->head->extra_max - len;
                } else {
                    x = copy;
                }
                memcpy(state->head->extra + len, next, x);
            }
            if ((state->flags & 0x0200) && (state->wrap & 4))
                state->check = crc32(state->check, next, copy);
            have -= copy;
            next += copy;
            state->length -= copy;
        }
        if (state->length) return inf_leave();
    }
    state->length = 0;
    state->mode = NAME;
    st_name();
}

int st_name() {
    if (state->flags & 0x0800) {
        if (have == 0) {
            return inf_leave();
        }
        copy = 0;
        while (true) {
            len = (unsigned)(next[copy++]);
            if (state->head != NULL &&
                    state->head->name != NULL &&
                    state->length < state->head->name_max)
            {
                state->head->name[state->length++] = (uint8_t)len;
            }
            bool cont = (len && copy < have);
            if (!cont) break;
        }
        if ((state->flags & 0x0200) && (state->wrap & 4)) {
            state->check = crc32(state->check, next, copy);
        }
        have -= copy;
        next += copy;
        if (len) {
            return inf_leave();
        }
    }
    else if (state->head != NULL) {
        state->head->name = NULL;
    }
    state->length = 0;
    state->mode = COMMENT;
    return st_comment();
}

int st_comment(ctx_t *ctx) {
    if (state->flags & 0x1000) {
        if (ctx->have == 0) {
            return inf_leave();
        }
        ctx->copy = 0;
        while (true) {
            ctx->len = (unsigned)(next[copy++]);
            if (state->head != NULL &&
                    state->head->comment != NULL &&
                    state->length < state->head->comm_max)
                state->head->comment[state->length++] = (uint8_t)len;
            bool cont = (len && copy < have);
            if (!cont) break;
        }
        if ((state->flags & 0x0200) && (state->wrap & 4)) {
            state->check = crc32(state->check, next, copy);
        }
        ctx->have -= ctx->copy;
        ctx->next += ctx->copy;
        if (len) {
            return inf_leave();
        }
    }
    else if (state->head != NULL) {
        state->head->comment = NULL;
    }
    state->mode = HCRC;
    return st_hcrc();
}

int st_hcrc() {
    if (state->flags & 0x0200) {
        NEEDBITS(ctx, 16);
        if ((state->wrap & 4) && hold != (state->check & 0xffff)) {
            strm->msg = (char *)"header crc mismatch";
            state->mode = BAD;
            break;
        }
        INITBITS();
    }
    if (state->head != NULL) {
        state->head->hcrc = (int)((state->flags >> 9) & 1);
        state->head->done = 1;
    }
    strm->adler = state->check = crc32(0L, NULL, 0);
    state->mode = TYPE;
}

int st_dictid() {
    NEEDBITS(ctx, 32);
    strm->adler = state->check = ZSWAP32(hold);
    INITBITS();
    state->mode = DICT;
    int r = st_dict();
    if (r == BREAK) break;
    return r;
}

int st_dict() {
    if (state->havedict == 0) {
        RESTORE(ctx);
        return Z_NEED_DICT;
    }
    strm->adler = state->check = adler32.adler32(0L, NULL, 0);
    state->mode = TYPE;
    int r = st_type();
    if (r == BREAK) break;
    return r;
}

int st_type(int flush) {
    if (flush == stream.Z_BLOCK || flush == stream.Z_TREES) {
        return inf_leave();
    }
    return st_typedo();
}

int st_typedo() {
    if (state->last) {
        /* Remove zero to seven bits as needed to go to a byte boundary */
        hold >>= bits & 7;
        bits -= bits & 7;
        state->mode = CHECK;
        break;
    }
    NEEDBITS(ctx, 3);
    state->last = BITS(1);
    DROPBITS(1, ctx, &bits);
    switch (BITS(2)) {
        case 0: {                             /* stored block */
            if (state->last) {
                trace.Tracev(stderr, "inflate:     stored block (last)\n");
            } else {
                trace.Tracev(stderr, "inflate:     stored block\n");
            }
            state->mode = STORED;
        }
        case 1: {                             /* fixed block */
            fixedtables(state);
            if (state->last) {
                trace.Tracev(stderr, "inflate:     fixed codes block (last)\n");
            } else {
                trace.Tracev(stderr, "inflate:     fixed codes block\n");
            }
            state->mode = LEN_;             /* decode codes */
            if (flush == stream.Z_TREES) {
                DROPBITS(2, ctx, &bits);
                return inf_leave();
            }
        }
        case 2: {                             /* dynamic block */
            if (state->last) {
                trace.Tracev(stderr, "inflate:     dynamic codes block (last)\n");
            } else {
                trace.Tracev(stderr, "inflate:     dynamic codes block\n");
            }
            state->mode = TABLE;
        }
        case 3: {
            strm->msg = (char *)"invalid block type";
            state->mode = BAD;
        }
    }
    DROPBITS(2, ctx, &bits);
}

int st_stored(ctx_t *ctx) {
    /* go to byte boundary */
    /* Remove zero to seven bits as needed to go to a byte boundary */
    ctx->hold >>= bits & 7;
    bits -= bits & 7;
    NEEDBITS(ctx, 32);
    if ((ctx->hold & 0xffff) != ((ctx->hold >> 16) ^ 0xffff)) {
        strm->msg = "invalid stored block lengths";
        state->mode = BAD;
        break;
    }
    state->length = (unsigned) ctx->hold & 0xffff;
    trace.Tracev(stderr, "inflate:       stored length %u\n", state->length);
    INITBITS();
    state->mode = COPY_;
    if (flush == stream.Z_TREES) {
        return inf_leave();
    }
    state->mode = COPY;
    return st_copy();
}

int st_copy() {
    copy = state->length;
    if (copy) {
        if (copy > have) copy = have;
        if (copy > left) copy = left;
        if (copy == 0) return inf_leave();
        memcpy(put, next, copy);
        have -= copy;
        next += copy;
        left -= copy;
        put += copy;
        state->length -= copy;
        break;
    }
    trace.Tracev(stderr, "inflate:       stored end\n");
    state->mode = TYPE;
}

int st_table() {
    NEEDBITS(ctx, 14);
    state->nlen = BITS(5) + 257;
    DROPBITS(5, ctx, &bits);
    state->ndist = BITS(5) + 1;
    DROPBITS(5, ctx, &bits);
    state->ncode = BITS(4) + 4;
    DROPBITS(4, ctx, &bits);
    if (!PKZIP_BUG_WORKAROUND) {
        if (state->nlen > 286 || state->ndist > 30) {
            strm->msg = (char *)"too many length or distance symbols";
            state->mode = BAD;
            break;
        }
    }
    trace.Tracev(stderr, "inflate:       table sizes ok\n");
    state->have = 0;
    state->mode = LENLENS;
    return st_lenlens();
}

int st_lenlens() {
    while (state->have < state->ncode) {
        NEEDBITS(ctx, 3);
        state->lens[order[state->have++]] = (uint16_t)BITS(3);
        DROPBITS(3, &ctx->hold, &bits);
    }
    while (state->have < 19)
        state->lens[order[state->have++]] = 0;
    state->next = state->codes;
    state->lencode = state->next;
    state->lenbits = 7;
    ret = inflate_table(CODES, state->lens, 19, &(state->next),
                        &(state->lenbits), state->work);
    if (ret) {
        strm->msg = (char *)"invalid code lengths set";
        state->mode = BAD;
        break;
    }
    trace.Tracev(stderr, "inflate:       code lengths ok\n");
    state->have = 0;
    state->mode = CODELENS;
    return st_codelens();
}

int st_codelens() {
    while (state->have < state->nlen + state->ndist) {
        while (true) {
            here = state->lencode[BITS(state->lenbits)];
            if ((unsigned)(here.bits) <= bits) break;
            PULLBYTE();
        }
        if (here.val < 16) {
            DROPBITS(here.bits, &ctx->hold, &bits);
            state->lens[state->have++] = here.val;
        }
        else {
            if (here.val == 16) {
                NEEDBITS(ctx, here.bits + 2);
                DROPBITS(here.bits, &ctx->hold, &bits);
                if (state->have == 0) {
                    strm->msg = "invalid bit length repeat";
                    state->mode = BAD;
                    break;
                }
                len = state->lens[state->have - 1];
                copy = 3 + BITS(2);
                DROPBITS(2, &ctx->hold, &bits);
            }
            else if (here.val == 17) {
                NEEDBITS(ctx, here.bits + 3);
                DROPBITS(here.bits, ctx, &bits);
                len = 0;
                copy = 3 + BITS(3);
                DROPBITS(3, ctx, &bits);
            }
            else {
                NEEDBITS(ctx, here.bits + 7);
                DROPBITS(here.bits, ctx, &bits);
                len = 0;
                copy = 11 + BITS(7);
                DROPBITS(7, ctx, &bits);
            }
            if (state->have + copy > state->nlen + state->ndist) {
                strm->msg = "invalid bit length repeat";
                state->mode = BAD;
                break;
            }
            while (copy--) {
                state->lens[state->have++] = (uint16_t)len;
            }
        }
    }

    /* handle error breaks in while */
    if (state->mode == BAD) break;

    /* check for end-of-block code (better have one) */
    if (state->lens[256] == 0) {
        strm->msg = (char *)"invalid code -- missing end-of-block";
        state->mode = BAD;
        break;
    }

    /* build code tables -- note: do not change the lenbits or distbits
        values here (9 and 6) without reading the comments in inftrees.h
        concerning the ENOUGH constants, which depend on those values */
    state->next = state->codes;
    state->lencode = state->next;
    state->lenbits = 9;
    ret = inflate_table(LENS, state->lens, state->nlen, &(state->next),
                        &(state->lenbits), state->work);
    if (ret) {
        strm->msg = (char *)"invalid literal/lengths set";
        state->mode = BAD;
        break;
    }
    state->distcode = state->next;
    state->distbits = 6;
    ret = inflate_table(DISTS, state->lens + state->nlen, state->ndist,
                    &(state->next), &(state->distbits), state->work);
    if (ret) {
        strm->msg = (char *)"invalid distances set";
        state->mode = BAD;
        break;
    }
    trace.Tracev(stderr, "inflate:       codes ok\n");
    state->mode = LEN_;
    if (flush == stream.Z_TREES) {
        return inf_leave();
    }
    state->mode = LEN;
    return st_len();
}

int st_len() {
    if (have >= 6 && left >= 258) {
        RESTORE(ctx);
        inflate_fast(strm, out);
        LOAD();
        if (state->mode == TYPE)
            state->back = -1;
        return BREAK;
    }
    state->back = 0;
    while (true) {
        here = state->lencode[BITS(state->lenbits)];
        if ((unsigned)(here.bits) <= bits) break;
        PULLBYTE();
    }
    if (here.op && (here.op & 0xf0) == 0) {
        last = here;
        while (true) {
            here = state->lencode[last.val +
                    (BITS(last.bits + last.op) >> last.bits)];
            if ((unsigned)(last.bits + here.bits) <= bits) break;
            PULLBYTE();
        }
        DROPBITS(last.bits, ctx, &bits);
        state->back += last.bits;
    }
    DROPBITS(here.bits, ctx, &bits);
    state->back += here.bits;
    state->length = (unsigned)here.val;
    if ((int)(here.op) == 0) {
        if (here.val >= 0x20 && here.val < 0x7f) {
            Tracevv(stderr, "inflate:         literal '%c'\n", here.val);
        } else {
            Tracevv(stderr, "inflate:         literal 0x%02x\n", here.val);
        }
        state->mode = LIT;
        break;
    }
    if (here.op & 32) {
        Tracevv(stderr, "inflate:         end of block\n");
        state->back = -1;
        state->mode = TYPE;
        break;
    }
    if (here.op & 64) {
        strm->msg = (char *)"invalid literal/length code";
        state->mode = BAD;
        break;
    }
    state->extra = (unsigned)(here.op) & 15;
    state->mode = LENEXT;
    return st_lenext();
}

int st_lenext() {
    if (state->extra) {
        NEEDBITS(ctx, state->extra);
        state->length += BITS(state->extra);
        DROPBITS(state->extra, ctx, &bits);
        state->back += state->extra;
    }
    Tracevv(stderr, "inflate:         length %u\n", state->length);
    state->was = state->length;
    state->mode = DIST;
    return st_dist();
}

int st_dist() {
    while (true) {
        here = state->distcode[BITS(state->distbits)];
        if ((unsigned)(here.bits) <= bits) break;
        PULLBYTE();
    }
    if ((here.op & 0xf0) == 0) {
        last = here;
        while (true) {
            here = state->distcode[last.val + (BITS(last.bits + last.op) >> last.bits)];
            if ((unsigned)(last.bits + here.bits) <= bits) break;
            PULLBYTE();
        }
        DROPBITS(last.bits, ctx, &bits);
        state->back += last.bits;
    }
    DROPBITS(here.bits, ctx, &bits);
    state->back += here.bits;
    if (here.op & 64) {
        strm->msg = "invalid distance code";
        state->mode = BAD;
        return BREAK;
    }
    state->offset = (unsigned)here.val;
    state->extra = (unsigned)(here.op) & 15;
    state->mode = DISTEXT;
    return st_distext();
}

int st_distext() {
    if (state->extra) {
        NEEDBITS(ctx, state->extra);
        state->offset += BITS(state->extra);
        DROPBITS(state->extra, ctx, &bits);
        state->back += state->extra;
    }
    Tracevv(stderr, "inflate:         distance %u\n", state->offset);
    state->mode = MATCH;
    return st_match();
}

int st_match(ctx_t *ctx) {
    if (ctx->left == 0) {
        return inf_leave();
    }
    ctx->copy = ctx->out - ctx->left;
    if (state->offset > ctx->copy) {         /* copy from window */
        ctx->copy = state->offset - ctx->copy;
        if (ctx->copy > state->whave) {
            if (state->sane) {
                strm->msg = "invalid distance too far back";
                state->mode = BAD;
                return BREAK;
            }
            Trace(stderr, "inflate.c too far\n");
            ctx->copy -= state->whave;
            if (ctx->copy > state->length) {
                ctx->copy = state->length;
            }
            if (ctx->copy > ctx->left) {
                ctx->copy = ctx->left;
            }
            ctx->left -= ctx->copy;
            ctx->state->length -= ctx->copy;
            while (true) {
                *put++ = 0;
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
        bool cont = (--copy);
        if (!cont) break;
    }
    if (ctx->state->length == 0) {
        ctx->state->mode = LEN;
    }
}

int st_lit() {
    if (left == 0) {
        return inf_leave();
    }
    *put++ = (uint8_t)(state->length);
    left--;
    state->mode = LEN;
}

int st_check(ctx_t *ctx) {
    if (ctx->state->wrap) {
        NEEDBITS(ctx, 32);
        ctx->out -= ctx->left;
        ctx->strm->total_out += ctx->out;
        ctx->state->total += ctx->out;
        if ((ctx->state->wrap & 4) && ctx->out) {
            ctx->strm->adler = ctx->state->check = UPDATE_CHECK(ctx->state->check, ctx->put - ctx->out, ctx->out);
        }
        ctx->out = ctx->left;
        int x;
        if (ctx->state->flags) {
            x = ctx->hold;
        } else {
            x = ZSWAP32(ctx->hold);
        }
        if ((ctx->state->wrap & 4) && x != ctx->state->check) {
            ctx->strm->msg = "incorrect data check";
            ctx->state->mode = BAD;
            return BREAK;
        }
        INITBITS();
        trace.Tracev(stderr, "inflate:   check matches trailer\n");
    }
    ctx->state->mode = LENGTH;
    return st_length(ctx);
}

int st_length(ctx_t *ctx) {
    if (ctx->state->wrap && ctx->state->flags) {
        NEEDBITS(ctx, 32);
        if ((ctx->state->wrap & 4) && ctx->hold != (ctx->state->total & 0xffffffff)) {
            ctx->strm->msg = "incorrect length check";
            ctx->state->mode = BAD;
            return BREAK;
        }
        INITBITS();
        trace.Tracev(stderr, "inflate:   length matches trailer\n");
    }
    ctx->state->mode = DONE;
    ctx->ret = stream.Z_STREAM_END;
    return inf_leave();
}

/*
       Return from inflate(), updating the total counts and the check value.
       If there was no progress during the inflate() call, return a buffer
       error.  Call updatewindow() to create and/or update the window state.
       Note: a memory error from inflate() is non-recoverable.
     */
// When returning, a "return inf_leave()" is used to update the total counters,
//    update the check value, and determine whether any progress has been made
//    during that inflate() call in order to return the proper return code.
//    Progress is defined as a change in either strm->avail_in or strm->avail_out.
//    When there is a window, return inf_leave() will update the window with the last
//    output written.  If a return inf_leave() occurs in the middle of decompression
//    and there is no window currently, return inf_leave() will create one and copy
//    output to the window for the next call of inflate().
int inf_leave(ctx_t *ctx, int flush) {
    RESTORE(ctx);
    if (ctx->state->wsize
        || (ctx->out != ctx->strm->avail_out
            && ctx->state->mode < BAD
            && (ctx->state->mode < CHECK || flush != stream.Z_FINISH))
    ) {
        if (updatewindow(ctx->strm, ctx->strm->next_out, ctx->out - ctx->strm->avail_out)) {
            ctx->state->mode = MEM;
            return stream.Z_MEM_ERROR;
        }
    }
    ctx->in -= ctx->strm->avail_in;
    ctx->out -= ctx->strm->avail_out;
    ctx->strm->total_in += ctx->in;
    ctx->strm->total_out += ctx->out;
    ctx->state->total += ctx->out;
    if ((ctx->state->wrap & 4) && ctx->out) {
        ctx->strm->adler = ctx->state->check = UPDATE_CHECK(ctx->state->check, ctx->strm->next_out - ctx->out, ctx->out);
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
    if (((ctx->in == 0 && ctx->out == 0) || flush == stream.Z_FINISH) && ctx->ret == stream.Z_OK) {
        ctx->ret = stream.Z_BUF_ERROR;
    }
    return ctx->ret;
}

/*
 * Frees all dynamically allocated data structures for this stream.
 * Discards any unprocessed input and does not flush any pending output.
 * Returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if the
 * stream state was inconsistent.
 */
pub int inflateEnd(stream.z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return stream.Z_STREAM_ERROR;
    }
    inflate_state *state = strm->state;
    if (state->window != NULL) {
        stream.ZFREE(strm, state->window);
    }
    stream.ZFREE(strm, strm->state);
    strm->state = NULL;
    trace.Tracev(stderr, "inflate: end\n");
    return stream.Z_OK;
}

/*
     Returns the sliding dictionary being maintained by inflate.  dictLength is
   set to the number of bytes in the dictionary, and that many bytes are copied
   to dictionary.  dictionary must have enough space, where 32768 bytes is
   always enough.  If inflateGetDictionary() is called with dictionary equal to
   NULL, then only the dictionary length is returned, and nothing is copied.
   Similarly, if dictLength is NULL, then it is not set.

     inflateGetDictionary returns stream.Z_OK on success, or stream.Z_STREAM_ERROR if the
   stream state is inconsistent.
*/
pub int inflateGetDictionary(
    stream.z_stream *strm,
    uint8_t *dictionary,
    uint32_t *dictLength
) {
    if (inflateStateCheck(strm)) {
        return stream.Z_STREAM_ERROR;
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
    return stream.Z_OK;
}

/*
     Initializes the decompression dictionary from the given uncompressed byte
   sequence.  This function must be called immediately after a call of inflate,
   if that call returned Z_NEED_DICT.  The dictionary chosen by the compressor
   can be determined from the Adler-32 value returned by that call of inflate.
   The compressor and decompressor must use exactly the same dictionary (see
   deflateSetDictionary).  For raw inflate, this function can be called at any
   time to set the dictionary.  If the provided dictionary is smaller than the
   window and there is already data in the window, then the provided dictionary
   will amend what's there.  The application must insure that the dictionary
   that was used for compression is provided.

     inflateSetDictionary returns stream.Z_OK if success, stream.Z_STREAM_ERROR if a
   parameter is invalid (e.g.  dictionary being NULL) or the stream state is
   inconsistent, stream.Z_DATA_ERROR if the given dictionary doesn't match the
   expected one (incorrect Adler-32 value).  inflateSetDictionary does not
   perform any decompression: this will be done by subsequent calls of
   inflate().
*/
pub int inflateSetDictionary(
    stream.z_stream *strm,
    const uint8_t *dictionary,
    uint32_t dictLength
) {
    inflate_state *state;
    uint32_t dictid;
    int ret;

    /* check state */
    if (inflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    if (state->wrap != 0 && state->mode != DICT)
        return stream.Z_STREAM_ERROR;

    /* check for correct dictionary identifier */
    if (state->mode == DICT) {
        dictid = adler32.adler32(0L, NULL, 0);
        dictid = adler32.adler32(dictid, dictionary, dictLength);
        if (dictid != state->check)
            return stream.Z_DATA_ERROR;
    }

    /* copy dictionary to window using updatewindow(), which will amend the
       existing dictionary if appropriate */
    ret = updatewindow(strm, dictionary + dictLength, dictLength);
    if (ret) {
        state->mode = MEM;
        return stream.Z_MEM_ERROR;
    }
    state->havedict = 1;
    trace.Tracev(stderr, "inflate:   dictionary set\n");
    return stream.Z_OK;
}


/*
     inflateGetHeader() requests that gzip header information be stored in the
   provided gz_header structure.  inflateGetHeader() may be called after
   inflateInit2() or inflateReset(), and before the first call of inflate().
   As inflate() processes the gzip stream, head->done is zero until the header
   is completed, at which time head->done is set to one.  If a zlib stream is
   being decoded, then head->done is set to -1 to indicate that there will be
   no gzip header information forthcoming.  Note that stream.Z_BLOCK or stream.Z_TREES can be
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

     inflateGetHeader returns stream.Z_OK if success, or stream.Z_STREAM_ERROR if the source
   stream state was inconsistent.
*/
pub int inflateGetHeader(stream.z_stream *strm, stream.gz_header *head) {
    if (inflateStateCheck(strm)) {
        return stream.Z_STREAM_ERROR;
    }
    inflate_state *state = strm->state;
    if ((state->wrap & 2) == 0) {
        return stream.Z_STREAM_ERROR;
    }

    /* save header structure */
    state->head = head;
    head->done = 0;
    return stream.Z_OK;
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

     inflateSync returns stream.Z_OK if a possible full flush point has been found,
   stream.Z_BUF_ERROR if no more input was provided, stream.Z_DATA_ERROR if no flush point
   has been found, or stream.Z_STREAM_ERROR if the stream structure was inconsistent.
   In the success case, the application may save the current current value of
   total_in which indicates where valid compressed data was found.  In the
   error case, the application may repeatedly call inflateSync, providing more
   input each time, until success or end of the input data.
*/
pub int inflateSync(stream.z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return stream.Z_STREAM_ERROR;
    }
    inflate_state *state = strm->state;
    if (strm->avail_in == 0 && state->bits < 8) {
        return stream.Z_BUF_ERROR;
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
        return stream.Z_DATA_ERROR;
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
    return stream.Z_OK;
}

/*
   Returns true if inflate is currently at the end of a block generated by
   Z_SYNC_FLUSH or Z_FULL_FLUSH. This function is used by one PPP
   implementation to provide an additional safety check. PPP uses
   Z_SYNC_FLUSH but removes the length bytes of the resulting empty stored
   block. When decompressing, PPP checks that at the end of input packet,
   inflate is waiting for these length bytes.
 */
pub int inflateSyncPoint(stream.z_stream *strm) {
    if (inflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    return strm->state->mode == STORED
        && strm->state->bits == 0;
}

/*
     Sets the destination stream as a complete copy of the source stream.

     This function can be useful when randomly accessing a large stream.  The
   first pass through the stream can periodically record the inflate state,
   allowing restarting inflate at those points when randomly accessing the
   stream.

     inflateCopy returns stream.Z_OK if success, stream.Z_MEM_ERROR if there was not
   enough memory, stream.Z_STREAM_ERROR if the source stream state was inconsistent
   (such as zalloc being NULL).  msg is left unchanged in both source and
   destination.
*/
pub int inflateCopy(stream.z_stream *dest, *source) {
    inflate_state *state;
    inflate_state *copy;
    uint8_t *window;
    unsigned wsize;

    /* check input */
    if (inflateStateCheck(source) || dest == NULL) {
        return stream.Z_STREAM_ERROR;
    }
    state = (inflate_state *)source->state;

    /* allocate space */
    copy = stream.ZALLOC(source, 1, sizeof(inflate_state));
    if (copy == NULL) {
        return stream.Z_MEM_ERROR;
    }
    window = NULL;
    if (state->window != NULL) {
        window = stream.ZALLOC(source, 1U << state->wbits, sizeof(uint8_t));
        if (window == NULL) {
            stream.ZFREE(source, copy);
            return stream.Z_MEM_ERROR;
        }
    }

    /* copy state */
    memcpy(dest, source, sizeof(stream.z_stream));
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
    return stream.Z_OK;
}

pub int inflateUndermine(stream.z_stream *strm, int subvert) {
    inflate_state *state;

    if (inflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    state->sane = !subvert;
    return stream.Z_OK;
}

pub int inflateValidate(stream.z_stream *strm, int check) {
    inflate_state *state;
    if (inflateStateCheck(strm)) return stream.Z_STREAM_ERROR;
    state = (inflate_state *)strm->state;
    if (check && state->wrap)
        state->wrap |= 4;
    else
        state->wrap &= ~4;
    return stream.Z_OK;
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
   as noted in the description for the stream.Z_BLOCK flush parameter for inflate.

     inflateMark returns the value noted above, or -65536 if the provided
   source stream state was inconsistent.
*/
pub long inflateMark(stream.z_stream *strm) {
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

pub uint32_t inflateCodesUsed(stream.z_stream *strm) {
    if (inflateStateCheck(strm)) {
        return (uint32_t)-1;
    }
    inflate_state *state = strm->state;
    return state->next - state->codes;
}
