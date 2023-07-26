/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2017 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "zutil.h"
#  include "gzguts.h"

const char * const z_errmsg[10] = {
    (const char *)"need dictionary",     /* Z_NEED_DICT       2  */
    (const char *)"stream end",          /* Z_STREAM_END      1  */
    (const char *)"",                    /* Z_OK              0  */
    (const char *)"file error",          /* Z_ERRNO         (-1) */
    (const char *)"stream error",        /* Z_STREAM_ERROR  (-2) */
    (const char *)"data error",          /* Z_DATA_ERROR    (-3) */
    (const char *)"insufficient memory", /* Z_MEM_ERROR     (-4) */
    (const char *)"buffer error",        /* Z_BUF_ERROR     (-5) */
    (const char *)"incompatible version",/* Z_VERSION_ERROR (-6) */
    (const char *)""
};


/* The application can compare zlibVersion and ZLIB_VERSION for consistency.
   If the first character differs, the library code actually used is not
   compatible with the zlib.h header file used by the application.  This check
   is automatically made by deflateInit and inflateInit.
 */
pub const char * zlibVersion()
{
    return ZLIB_VERSION;
}


/* Return flags indicating compile-time options.

    Type sizes, two bits each, 00 = 16 bits, 01 = 32, 10 = 64, 11 = other:
     1.0: size of uInt
     3.2: size of uLong
     5.4: size of void* (pointer)
     7.6: size of z_off_t

    Compiler, assembler, and debug options:
     8: ZLIB_DEBUG
     9: ASMV or ASMINF -- use ASM code
     10: ZLIB_WINAPI -- exported functions use the WINAPI calling convention
     11: 0 (reserved)

    One-time table building (smaller code, but not thread-safe if true):
     12: BUILDFIXED -- build static block decoding tables when needed
     13: DYNAMIC_CRC_TABLE -- build CRC calculation tables when needed
     14,15: 0 (reserved)

    Library content (indicates missing functionality):
     16: NO_GZCOMPRESS -- gz* functions cannot compress (to avoid linking
                          deflate code when not needed)
     17: NO_GZIP -- deflate can't write gzip streams, and inflate can't detect
                    and decode gzip streams (to avoid linking crc code)
     18-19: 0 (reserved)

    Operation variations (changes in library functionality):
     20: PKZIP_BUG_WORKAROUND -- slightly more permissive inflate
     21: FASTEST -- deflate algorithm with only one, lowest compression level
     22,23: 0 (reserved)

    The sprintf variant used by gzprintf (zero is best):
     24: 0 = vs*, 1 = s* -- 1 means limited to 20 arguments after the format
     25: 0 = *nprintf, 1 = *printf -- 1 means gzprintf() not secure!
     26: 0 = returns value, 1 = void -- 1 means inferred string length returned

    Remainder:
     27-31: 0 (reserved)
 */
pub uLong zlibCompileFlags()
{
    uLong flags;

    flags = 0;
    switch ((int)(sizeof(uInt))) {
    case 2:     break;
    case 4:     flags += 1;     break;
    case 8:     flags += 2;     break;
    default:    flags += 3;
    }
    switch ((int)(sizeof(uLong))) {
    case 2:     break;
    case 4:     flags += 1 << 2;        break;
    case 8:     flags += 2 << 2;        break;
    default:    flags += 3 << 2;
    }
    switch ((int)(sizeof(voidpf))) {
    case 2:     break;
    case 4:     flags += 1 << 4;        break;
    case 8:     flags += 2 << 4;        break;
    default:    flags += 3 << 4;
    }
    switch ((int)(sizeof(z_off_t))) {
    case 2:     break;
    case 4:     flags += 1 << 6;        break;
    case 8:     flags += 2 << 6;        break;
    default:    flags += 3 << 6;
    }
    flags += 1 << 8;
    /*
#if defined(ASMV) || defined(ASMINF)
    flags += 1 << 9;
#endif
     */
#ifdef ZLIB_WINAPI
    flags += 1 << 10;
#endif
    flags += 1 << 12;
    flags += 1 << 13;
#ifdef NO_GZCOMPRESS
    flags += 1L << 16;
#endif
if (PKZIP_BUG_WORKAROUND) {
    flags += 1L << 20;
}
#  ifdef NO_vsnprintf
    flags += 1L << 25;
#    ifdef HAS_vsprintf_void
    flags += 1L << 26;
#    endif
#  else
#    ifdef HAS_vsnprintf_void
    flags += 1L << 26;
#    endif
#  endif
    return flags;
}

#include <stdlib.h>
#  ifndef verbose
#    define verbose 0
#  endif
int ZLIB_INTERNAL z_verbose = verbose;

void ZLIB_INTERNAL z_error(m)
    char *m;
{
    fprintf(stderr, "%s\n", m);
    exit(1);
}

/* exported to allow conversion of error code to string for compress() and
 * uncompress()
 */
pub const char * zError(err)
    int err;
{
    return ERR_MSG(err);
}

#if defined(_WIN32_WCE) && _WIN32_WCE < 0x800
    /* The older Microsoft C Run-Time Library for Windows CE doesn't have
     * errno.  We define it as a global variable to simplify porting.
     * Its value is always 0 and should not be used.
     */
    int errno = 0;
#endif





#ifndef MY_ZCALLOC /* Any system without a special alloc function */


voidpf ZLIB_INTERNAL zcalloc(opaque, items, size)
    voidpf opaque;
    unsigned items;
    unsigned size;
{
    (void)opaque;
    return sizeof(uInt) > 2 ? (voidpf)malloc(items * size) :
                              (voidpf)calloc(items, size);
}

void ZLIB_INTERNAL zcfree(opaque, ptr)
    voidpf opaque;
    voidpf ptr;
{
    (void)opaque;
    free(ptr);
}

#endif /* MY_ZCALLOC */

