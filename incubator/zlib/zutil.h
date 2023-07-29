/* zutil.h -- internal interface and configuration of the compression library
 * Copyright (C) 1995-2022 Jean-loup Gailly, Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#  define ZLIB_INTERNAL

typedef unsigned char  uch;
typedef unsigned short ush;
typedef ush FAR ushf;

#if !defined(Z_U8)
#  if (ULONG_MAX == 0xffffffffffffffff)
#    define Z_U8 unsigned long
#  elif (ULLONG_MAX == 0xffffffffffffffff)
#    define Z_U8 unsigned long long
#  elif (UINT_MAX == 0xffffffffffffffff)
#    define Z_U8 unsigned
#  endif
#endif

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
/* The three kinds of block type */


#ifndef F_OPEN
#  define F_OPEN(name, mode) fopen((name), (mode))
#endif
