/* adler32.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011, 2016 Mark Adler

  Copyright (C) 1995-2022 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
 */

#define BASE 65521U     /* largest prime smaller than 65536 */
#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

/*
     Update a running Adler-32 checksum with the bytes buf[0..len-1] and
   return the updated checksum. An Adler-32 value is in the range of a 32-bit
   uint32_t integer. If buf is Z_NULL, this function returns the required
   initial value for the checksum.

     An Adler-32 checksum is almost as reliable as a CRC-32 but can be computed
   much faster.

   Usage example:

     uint32_t adler = adler32(0L, Z_NULL, 0);

     while (read_buffer(buffer, length) != EOF) {
       adler = adler32(adler, buffer, length);
     }
     if (adler != original_adler) error();
*/
pub uint32_t adler32(uint32_t adler, const uint8_t *buf, size_t len) {
    /* split Adler-32 into component sums */
    uint32_t sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    /* in case user likes doing a byte at a time, keep it fast */
    if (len == 1) {
        adler += buf[0];
        if (adler >= BASE)
            adler -= BASE;
        sum2 += adler;
        if (sum2 >= BASE)
            sum2 -= BASE;
        return adler | (sum2 << 16);
    }

    /* initial Adler-32 value (deferred check for len == 1 speed) */
    if (buf == NULL)
        return 1L;

    /* in case short lengths are provided, keep it somewhat fast */
    if (len < 16) {
        while (len--) {
            adler += *buf++;
            sum2 += adler;
        }
        if (adler >= BASE) {
            adler -= BASE;
        }
        sum2 %= BASE;            /* only added so many BASE's */
        return adler | (sum2 << 16);
    }

    /* do length NMAX blocks -- requires just one modulo operation */
    uint32_t n;
    while (len >= NMAX) {
        len -= NMAX;
        n = NMAX / 16;          /* NMAX is divisible by 16 */
        while (true) {
            for (int roll = 0; roll < 16; roll++) {
                adler += buf[roll];
                sum2 += adler;
            }
            buf += 16;
            n--;
            if (!n) {
                break;
            }
        }
        adler %= BASE;
        sum2 %= BASE;
    }

    /* do remaining bytes (less than NMAX, still just one modulo) */
    if (len) {                  /* avoid modulos if none remaining */
        while (len >= 16) {
            len -= 16;
            for (int roll = 0; roll < 16; roll++) {
                adler += buf[roll];
                sum2 += adler;
            }
            buf += 16;
        }
        while (len--) {
            adler += *buf++;
            sum2 += adler;
        }
        adler %= BASE;
        sum2 %= BASE;
    }

    /* return recombined sums */
    return adler | (sum2 << 16);
}

/* Combine two Adler-32 checksums into one.  For two sequences of bytes, seq1
   and seq2 with lengths len1 and len2, Adler-32 checksums were calculated for
   each, adler1 and adler2.  adler32_combine() returns the Adler-32 checksum of
   seq1 and seq2 concatenated, requiring only adler1, adler2, and len2.  Note
   that the z_off_t type (like off_t) is a signed integer.  If len2 is
   negative, the result has no meaning or utility.
*/
pub uint32_t adler32_combine(uint32_t adler1, adler2, int64_t len2) {
    uint32_t sum1;
    uint32_t sum2;
    uint32_t rem;

    /* for negative len, return invalid adler32 as a clue for debugging */
    if (len2 < 0)
        return 0xffffffffUL;

    /* the derivation of this formula is left as an exercise for the reader */
    len2 %= BASE; /* assumes len2 >= 0 */
    rem = (uint32_t)len2;
    sum1 = adler1 & 0xffff;
    sum2 = rem * sum1;
    sum2 %= BASE;
    sum1 += (adler2 & 0xffff) + BASE - 1;
    sum2 += ((adler1 >> 16) & 0xffff) + ((adler2 >> 16) & 0xffff) + BASE - rem;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum2 >= ((uint32_t)BASE << 1)) sum2 -= ((uint32_t)BASE << 1);
    if (sum2 >= BASE) sum2 -= BASE;
    return sum1 | (sum2 << 16);
}
