// Copyright © 2021 Plan 9 Foundation

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

/*
 * The authors of this software are Rob Pike and Ken Thompson.
 *              Copyright (c) 2002 by Lucent Technologies.
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHORS NOR LUCENT TECHNOLOGIES MAKE
 * ANY REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 */

// The Plan 9 character set and representation are based on the Unicode Standard
// and on the ISO multibyte UTF-8 encoding (Universal Character Set
// Transformation Format, 8 bits wide). The Unicode Standard represents its
// characters in 16 bits; UTF-8 represents such values in an 8-bit byte stream.

// UTF-8 is designed so the 7-bit ASCII set (values hexadecimal 00 to 7F),
// appear only as themselves in the encoding.
// Runes with values above 7F appear as sequences of two or more
// bytes with values only from 80 to FF.

// The UTF encoding of the Unicode Standard is backward compatible with ASCII:
// programs presented only with ASCII work on Plan 9 even if not written to deal with UTF,
// as do programs that deal with uninterpreted byte streams.
// However, programs that perform semantic processing on ASCII graphic
// characters must convert from UTF to runes in order to work properly with non-ASCII
// input.

// Letting numbers be binary, a rune x is converted to a multibyte UTF sequence
// as follows:

// 01.   x in [00000000.0bbbbbbb] → 0bbbbbbb
// 10.   x in [00000bbb.bbbbbbbb] → 110bbbbb, 10bbbbbb
// 11.   x in [bbbbbbbb.bbbbbbbb] → 1110bbbb, 10bbbbbb, 10bbbbbb

// Conversion 01 provides a one-byte sequence that spans the ASCII
// character set in a compatible way.
// Conversions 10 and 11 represent higher-valued characters
// as sequences of two or three bytes with the high bit set.
// Plan 9 does not support the 4, 5, and 6 byte sequences proposed by X-Open.
// When there are multiple ways to encode a value, for example rune 0,
// the shortest encoding is used.

// In the inverse mapping, any sequence except those described above is
// incorrect and is converted to rune hexadecimal 0080.

/*
 * In Plan 9, a rune is a number representing a Unicode character. Internally,
 * programs may store characters as runes. However, any external manifestation
 * of textual information, in files or at the interface between programs, uses
 * a machine-independent, byte-stream encoding called UTF-8.
 */
pub typedef uint32_t Rune;

pub enum {
	UTFmax		= 4,		/* maximum bytes per rune */
	Runesync	= 0x80,		/* cannot represent part of a UTF sequence (<) */
	Runeself	= 0x80,		/* rune and UTF sequences are the same (<) */
	Runeerror	= 0xFFFD,	/* decoding error in UTF */
	Runemax = 0x10FFFF	/* maximum rune value */
}

pub Rune *runestrcat(Rune *s1, *s2) {
	runestrcpy(runestrchr(s1, 0), s2);
	return s1;
}

pub Rune* runestrchr(Rune *s, Rune c) {
	if ((int) c == 0) {
		while (*s != 0) s++;
		return s;
	}
	while (*s != 0) {
		if (*s == c) return s;
	}
	return 0;
}

pub Rune* runestrecpy(Rune *s1, Rune *es1, Rune *s2) {
	if (s1 >= es1) {
		return s1;
	}
	while (true) {
		*s1 = *s2;
		s1++;
		s2++;
		if (!*s2) break;
		if (s1 == es1) {
			*--s1 = '\0';
			break;
		}
	}
	return s1;
}

/*
 * Return pointer to first occurrence of s2 in s1, NULL if none.
 */
pub Rune* runestrstr(Rune *s1, *s2) {
	if (!*s2) {
		return s1;
	}

	Rune *p = NULL;
	Rune *pa = NULL;
	Rune *pb = NULL;
	uint32_t c = 0;

	int c0 = *s2;
	s2++;
	for(p=runestrchr(s1, c0); p; p=runestrchr(p+1, c0)) {
		pa = p;
		for(pb=s2;true; pb++) {
			c = *pb;
			if(c == 0)
				return p;
			if (c != *++pa) {
				break;
			}
		}
	}
	return NULL;
}

// Allocates and returns a copy of the string s.
pub Rune *runestrdup(Rune *s) {
	int len = runestrchr(s, 0) - s;
	Rune *ns = calloc!(len + 1, sizeof(Rune));
	return runestrcpy(ns, s);
}

pub Rune* runestrncat(Rune *s1, *s2, int32_t n) {
	Rune *os1 = NULL;

	os1 = s1;
	s1 = runestrchr(s1, 0);
	while (true) {
		*s1 = *s2;
		s1++;
		s2++;
		if (!*s2) break;
		if (--n < 0) {
			s1[-1] = 0;
			break;
		}
	}
	return os1;
}


pub int runestrncmp(Rune *s1, *s2, int32_t n) {
	Rune c1 = 0;
	Rune c2 = 0;

	while (n > 0) {
		c1 = *s1++;
		c2 = *s2++;
		n--;
		if(c1 != c2) {
			if(c1 > c2)
				return 1;
			return -1;
		}
		if(c1 == 0)
			break;
	}
	return 0;
}

pub int runestrcmp(Rune *s1, *s2) {
	Rune c1 = 0;
	Rune c2 = 0;

	while (true) {
		c1 = *s1++;
		c2 = *s2++;
		if(c1 != c2) {
			if(c1 > c2)
				return 1;
			return -1;
		}
		if(c1 == 0)
			return 0;
	}
}

pub Rune *runestrcpy(Rune *s1, *s2) {
	Rune *os1 = s1;
	while (true) {
		*s1 = *s2;
		s1++;
		s2++;
		if (!*s2) break;
	}
	return os1;
}

pub Rune* runestrncpy(Rune *s1, *s2, int32_t n) {
	Rune *os1 = s1;
	for (int i = 0; i < n; i++) {
		*s1++ = *s2++;
		if (*s2 == 0) {
			while (++i < n) {
				*s1++ = 0;
			}
			return os1;
		}
	}
	return os1;
}

// Returns a pointer to the last occurence of c in string s.
pub Rune* runestrrchr(Rune *s, Rune c) {
	if (c == 0) {
		return runestrchr(s, 0);
	}
	Rune *r = NULL;
	while (true) {
		s = runestrchr(s, c);
		if (!s) break;
		r = s;
		s++;
	}
	return r;
}

enum {
	Bit1	= 7,
	Bitx	= 6,
	Bit2	= 5,
	Bit3	= 4,
	Bit4	= 3,
	Bit5	= 2,

	// T1	= ((1<<(Bit1+1))-1) ^ 0xFF,	/* 0000 0000 */
	Tx	= ((1<<(Bitx+1))-1) ^ 0xFF,	/* 1000 0000 */
	T2	= ((1<<(Bit2+1))-1) ^ 0xFF,	/* 1100 0000 */
	T3	= ((1<<(Bit3+1))-1) ^ 0xFF,	/* 1110 0000 */
	T4	= ((1<<(Bit4+1))-1) ^ 0xFF,	/* 1111 0000 */
	T5	= ((1<<(Bit5+1))-1) ^ 0xFF,	/* 1111 1000 */

	Rune1	= (1<<(Bit1+0*Bitx))-1,		/* 0000 0000 0000 0000 0111 1111 */
	Rune2	= (1<<(Bit2+1*Bitx))-1,		/* 0000 0000 0000 0111 1111 1111 */
	Rune3	= (1<<(Bit3+2*Bitx))-1,		/* 0000 0000 1111 1111 1111 1111 */
	Rune4	= (1<<(Bit4+3*Bitx))-1,		/* 0011 1111 1111 1111 1111 1111 */

	Maskx	= (1<<Bitx)-1,			/* 0011 1111 */
	Testx	= Maskx ^ 0xFF			/* 1100 0000 */
};

// Copies the first rune from str to rune and returns the rune's size in bytes.
// If the input is not exactly in UTF format, will convert to 0x80 and return 1.
pub int get_rune(Rune *rune, const char *str) {
	int c2 = 0;
	int c3 = 0;
	int32_t l = 0;

	/*
	 * one character sequence
	 *	00000-0007F => T1
	 */
	int c = *(uint8_t*)str;
	if(c < Tx) {
		*rune = c;
		return 1;
	}

	/*
	 * two character sequence
	 *	0080-07FF => T2 Tx
	 */
	int c1 = *(uint8_t*)(str+1) ^ Tx;
	if(c1 & Testx) {
		*rune = Runeerror;
		return 1;
	}
	if(c < T3) {
		if(c < T2) {
			*rune = Runeerror;
			return 1;
		}
		l = ((c << Bitx) | c1) & Rune2;
		if(l <= Rune1) {
			*rune = Runeerror;
			return 1;
		}
		*rune = l;
		return 2;
	}

	/*
	 * three character sequence
	 *	0800-FFFF => T3 Tx Tx
	 */
	c2 = *(uint8_t*)(str+2) ^ Tx;
	if(c2 & Testx) {
		*rune = Runeerror;
		return 1;
	}
	if(c < T4) {
		l = ((((c << Bitx) | c1) << Bitx) | c2) & Rune3;
		if(l <= Rune2) {
			*rune = Runeerror;
			return 1;
		}
		*rune = l;
		return 3;
	}

	/*
	 * four character sequence
	 *	10000-10FFFF => T4 Tx Tx Tx
	 */
	if(UTFmax >= 4) {
		c3 = *(uint8_t*)(str+3) ^ Tx;
		if(c3 & Testx) {
			*rune = Runeerror;
			return 1;
		}
		if(c < T5) {
			l = ((((((c << Bitx) | c1) << Bitx) | c2) << Bitx) | c3) & Rune4;
			if(l <= Rune3 || l > Runemax) {
				*rune = Runeerror;
				return 1;
			}
			*rune = l;
			return 4;
		}
	}

	*rune = Runeerror;
	return 1;
}

// returns the number of bytes required to convert the n runes pointed to by
// r into UTF.
pub int runenlen(Rune *r, int nrune) {
	int nb = 0;
	while(nrune--) {
		int c = *r++;
		if(c <= Rune1)
			nb++;
		else if(c <= Rune2)
			nb += 2;
		else if(c <= Rune3 || c > Runemax)
			nb += 3;
		else
			nb += 4;
	}
	return nb;
}

// returns 1 if the string s of length n is long enough to be decoded by get_rune
// and 0 otherwise.
// This does not guarantee that the string
// contains a legal
//  UTF
// encoding.
// This routine is used by programs that
// obtain input a byte at
// a time and need to know when a full rune
// has arrived.
pub int fullrune(char *str, int n)
{
	int c = 0;

	if(n <= 0)
		return 0;
	c = *(uint8_t*)str;
	if(c < Tx)
		return 1;
	if(c < T3)
		return n >= 2;
	if(UTFmax == 3 || c < T4)
		return n >= 3;
	return n >= 4;
}


/*
 * alpha ranges -
 *	only covers ranges not in lower||upper
 */
Rune	__alpha2[] =
{
	0x00d8,	0x00f6,	/* Ø - ö */
	0x00f8,	0x01f5,	/* ø - ǵ */
	0x0250,	0x02a8,	/* ɐ - ʨ */
	0x038e,	0x03a1,	/* Ύ - Ρ */
	0x03a3,	0x03ce,	/* Σ - ώ */
	0x03d0,	0x03d6,	/* ϐ - ϖ */
	0x03e2,	0x03f3,	/* Ϣ - ϳ */
	0x0490,	0x04c4,	/* Ґ - ӄ */
	0x0561,	0x0587,	/* ա - և */
	0x05d0,	0x05ea,	/* א - ת */
	0x05f0,	0x05f2,	/* װ - ײ */
	0x0621,	0x063a,	/* ء - غ */
	0x0640,	0x064a,	/* ـ - ي */
	0x0671,	0x06b7,	/* ٱ - ڷ */
	0x06ba,	0x06be,	/* ں - ھ */
	0x06c0,	0x06ce,	/* ۀ - ێ */
	0x06d0,	0x06d3,	/* ې - ۓ */
	0x0905,	0x0939,	/* अ - ह */
	0x0958,	0x0961,	/* क़ - ॡ */
	0x0985,	0x098c,	/* অ - ঌ */
	0x098f,	0x0990,	/* এ - ঐ */
	0x0993,	0x09a8,	/* ও - ন */
	0x09aa,	0x09b0,	/* প - র */
	0x09b6,	0x09b9,	/* শ - হ */
	0x09dc,	0x09dd,	/* ড় - ঢ় */
	0x09df,	0x09e1,	/* য় - ৡ */
	0x09f0,	0x09f1,	/* ৰ - ৱ */
	0x0a05,	0x0a0a,	/* ਅ - ਊ */
	0x0a0f,	0x0a10,	/* ਏ - ਐ */
	0x0a13,	0x0a28,	/* ਓ - ਨ */
	0x0a2a,	0x0a30,	/* ਪ - ਰ */
	0x0a32,	0x0a33,	/* ਲ - ਲ਼ */
	0x0a35,	0x0a36,	/* ਵ - ਸ਼ */
	0x0a38,	0x0a39,	/* ਸ - ਹ */
	0x0a59,	0x0a5c,	/* ਖ਼ - ੜ */
	0x0a85,	0x0a8b,	/* અ - ઋ */
	0x0a8f,	0x0a91,	/* એ - ઑ */
	0x0a93,	0x0aa8,	/* ઓ - ન */
	0x0aaa,	0x0ab0,	/* પ - ર */
	0x0ab2,	0x0ab3,	/* લ - ળ */
	0x0ab5,	0x0ab9,	/* વ - હ */
	0x0b05,	0x0b0c,	/* ଅ - ଌ */
	0x0b0f,	0x0b10,	/* ଏ - ଐ */
	0x0b13,	0x0b28,	/* ଓ - ନ */
	0x0b2a,	0x0b30,	/* ପ - ର */
	0x0b32,	0x0b33,	/* ଲ - ଳ */
	0x0b36,	0x0b39,	/* ଶ - ହ */
	0x0b5c,	0x0b5d,	/* ଡ଼ - ଢ଼ */
	0x0b5f,	0x0b61,	/* ୟ - ୡ */
	0x0b85,	0x0b8a,	/* அ - ஊ */
	0x0b8e,	0x0b90,	/* எ - ஐ */
	0x0b92,	0x0b95,	/* ஒ - க */
	0x0b99,	0x0b9a,	/* ங - ச */
	0x0b9e,	0x0b9f,	/* ஞ - ட */
	0x0ba3,	0x0ba4,	/* ண - த */
	0x0ba8,	0x0baa,	/* ந - ப */
	0x0bae,	0x0bb5,	/* ம - வ */
	0x0bb7,	0x0bb9,	/* ஷ - ஹ */
	0x0c05,	0x0c0c,	/* అ - ఌ */
	0x0c0e,	0x0c10,	/* ఎ - ఐ */
	0x0c12,	0x0c28,	/* ఒ - న */
	0x0c2a,	0x0c33,	/* ప - ళ */
	0x0c35,	0x0c39,	/* వ - హ */
	0x0c60,	0x0c61,	/* ౠ - ౡ */
	0x0c85,	0x0c8c,	/* ಅ - ಌ */
	0x0c8e,	0x0c90,	/* ಎ - ಐ */
	0x0c92,	0x0ca8,	/* ಒ - ನ */
	0x0caa,	0x0cb3,	/* ಪ - ಳ */
	0x0cb5,	0x0cb9,	/* ವ - ಹ */
	0x0ce0,	0x0ce1,	/* ೠ - ೡ */
	0x0d05,	0x0d0c,	/* അ - ഌ */
	0x0d0e,	0x0d10,	/* എ - ഐ */
	0x0d12,	0x0d28,	/* ഒ - ന */
	0x0d2a,	0x0d39,	/* പ - ഹ */
	0x0d60,	0x0d61,	/* ൠ - ൡ */
	0x0e01,	0x0e30,	/* ก - ะ */
	0x0e32,	0x0e33,	/* า - ำ */
	0x0e40,	0x0e46,	/* เ - ๆ */
	0x0e5a,	0x0e5b,	/* ๚ - ๛ */
	0x0e81,	0x0e82,	/* ກ - ຂ */
	0x0e87,	0x0e88,	/* ງ - ຈ */
	0x0e94,	0x0e97,	/* ດ - ທ */
	0x0e99,	0x0e9f,	/* ນ - ຟ */
	0x0ea1,	0x0ea3,	/* ມ - ຣ */
	0x0eaa,	0x0eab,	/* ສ - ຫ */
	0x0ead,	0x0eae,	/* ອ - ຮ */
	0x0eb2,	0x0eb3,	/* າ - ຳ */
	0x0ec0,	0x0ec4,	/* ເ - ໄ */
	0x0edc,	0x0edd,	/* ໜ - ໝ */
	0x0f18,	0x0f19,	/* ༘ - ༙ */
	0x0f40,	0x0f47,	/* ཀ - ཇ */
	0x0f49,	0x0f69,	/* ཉ - ཀྵ */
	0x10d0,	0x10f6,	/* ა - ჶ */
	0x1100,	0x1159,	/* ᄀ - ᅙ */
	0x115f,	0x11a2,	/* ᅟ - ᆢ */
	0x11a8,	0x11f9,	/* ᆨ - ᇹ */
	0x1e00,	0x1e9b,	/* Ḁ - ẛ */
	0x1f50,	0x1f57,	/* ὐ - ὗ */
	0x1f80,	0x1fb4,	/* ᾀ - ᾴ */
	0x1fb6,	0x1fbc,	/* ᾶ - ᾼ */
	0x1fc2,	0x1fc4,	/* ῂ - ῄ */
	0x1fc6,	0x1fcc,	/* ῆ - ῌ */
	0x1fd0,	0x1fd3,	/* ῐ - ΐ */
	0x1fd6,	0x1fdb,	/* ῖ - Ί */
	0x1fe0,	0x1fec,	/* ῠ - Ῥ */
	0x1ff2,	0x1ff4,	/* ῲ - ῴ */
	0x1ff6,	0x1ffc,	/* ῶ - ῼ */
	0x210a,	0x2113,	/* ℊ - ℓ */
	0x2115,	0x211d,	/* ℕ - ℝ */
	0x2120,	0x2122,	/* ℠ - ™ */
	0x212a,	0x2131,	/* K - ℱ */
	0x2133,	0x2138,	/* ℳ - ℸ */
	0x3041,	0x3094,	/* ぁ - ゔ */
	0x30a1,	0x30fa,	/* ァ - ヺ */
	0x3105,	0x312c,	/* ㄅ - ㄬ */
	0x3131,	0x318e,	/* ㄱ - ㆎ */
	0x3192,	0x319f,	/* ㆒ - ㆟ */
	0x3260,	0x327b,	/* ㉠ - ㉻ */
	0x328a,	0x32b0,	/* ㊊ - ㊰ */
	0x32d0,	0x32fe,	/* ㋐ - ㋾ */
	0x3300,	0x3357,	/* ㌀ - ㍗ */
	0x3371,	0x3376,	/* ㍱ - ㍶ */
	0x337b,	0x3394,	/* ㍻ - ㎔ */
	0x3399,	0x339e,	/* ㎙ - ㎞ */
	0x33a9,	0x33ad,	/* ㎩ - ㎭ */
	0x33b0,	0x33c1,	/* ㎰ - ㏁ */
	0x33c3,	0x33c5,	/* ㏃ - ㏅ */
	0x33c7,	0x33d7,	/* ㏇ - ㏗ */
	0x33d9,	0x33dd,	/* ㏙ - ㏝ */
	0x4e00,	0x9fff,	/* 一 - 鿿 */
	0xac00,	0xd7a3,	/* 가 - 힣 */
	0xf900,	0xfb06,	/* 豈 - ﬆ */
	0xfb13,	0xfb17,	/* ﬓ - ﬗ */
	0xfb1f,	0xfb28,	/* ײַ - ﬨ */
	0xfb2a,	0xfb36,	/* שׁ - זּ */
	0xfb38,	0xfb3c,	/* טּ - לּ */
	0xfb40,	0xfb41,	/* נּ - סּ */
	0xfb43,	0xfb44,	/* ףּ - פּ */
	0xfb46,	0xfbb1,	/* צּ - ﮱ */
	0xfbd3,	0xfd3d,	/* ﯓ - ﴽ */
	0xfd50,	0xfd8f,	/* ﵐ - ﶏ */
	0xfd92,	0xfdc7,	/* ﶒ - ﷇ */
	0xfdf0,	0xfdf9,	/* ﷰ - ﷹ */
	0xfe70,	0xfe72,	/* ﹰ - ﹲ */
	0xfe76,	0xfefc,	/* ﹶ - ﻼ */
	0xff66,	0xff6f,	/* ｦ - ｯ */
	0xff71,	0xff9d,	/* ｱ - ﾝ */
	0xffa0,	0xffbe,	/* ﾠ - ﾾ */
	0xffc2,	0xffc7,	/* ￂ - ￇ */
	0xffca,	0xffcf,	/* ￊ - ￏ */
	0xffd2,	0xffd7,	/* ￒ - ￗ */
	0xffda,	0xffdc,	/* ￚ - ￜ */
};

/*
 * alpha singlets -
 *	only covers ranges not in lower||upper
 */
Rune	__alpha1[] =
{
	0x00aa,	/* ª */
	0x00b5,	/* µ */
	0x00ba,	/* º */
	0x03da,	/* Ϛ */
	0x03dc,	/* Ϝ */
	0x03de,	/* Ϟ */
	0x03e0,	/* Ϡ */
	0x06d5,	/* ە */
	0x09b2,	/* ল */
	0x0a5e,	/* ਫ਼ */
	0x0a8d,	/* ઍ */
	0x0ae0,	/* ૠ */
	0x0b9c,	/* ஜ */
	0x0cde,	/* ೞ */
	0x0e4f,	/* ๏ */
	0x0e84,	/* ຄ */
	0x0e8a,	/* ຊ */
	0x0e8d,	/* ຍ */
	0x0ea5,	/* ລ */
	0x0ea7,	/* ວ */
	0x0eb0,	/* ະ */
	0x0ebd,	/* ຽ */
	0x1fbe,	/* ι */
	0x207f,	/* ⁿ */
	0x20a8,	/* ₨ */
	0x2102,	/* ℂ */
	0x2107,	/* ℇ */
	0x2124,	/* ℤ */
	0x2126,	/* Ω */
	0x2128,	/* ℨ */
	0xfb3e,	/* מּ */
	0xfe74,	/* ﹴ */
};

/*
 * space ranges
 */
Rune	__space2[] =
{
	0x0009,	0x000a,	/* tab and newline */
	0x0020,	0x0020,	/* space */
	0x00a0,	0x00a0,	/*   */
	0x2000,	0x200b,	/*   - ​ */
	0x2028,	0x2029,	/*   -   */
	0x3000,	0x3000,	/* 　 */
	0xfeff,	0xfeff,	/* ﻿ */
};

/*
 * lower case ranges
 *	3rd col is conversion excess 500
 */
Rune	__toupper2[] =
{
	0x0061,	0x007a, 468,	/* a-z A-Z */
	0x00e0,	0x00f6, 468,	/* à-ö À-Ö */
	0x00f8,	0x00fe, 468,	/* ø-þ Ø-Þ */
	0x0256,	0x0257, 295,	/* ɖ-ɗ Ɖ-Ɗ */
	0x0258,	0x0259, 298,	/* ɘ-ə Ǝ-Ə */
	0x028a,	0x028b, 283,	/* ʊ-ʋ Ʊ-Ʋ */
	0x03ad,	0x03af, 463,	/* έ-ί Έ-Ί */
	0x03b1,	0x03c1, 468,	/* α-ρ Α-Ρ */
	0x03c3,	0x03cb, 468,	/* σ-ϋ Σ-Ϋ */
	0x03cd,	0x03ce, 437,	/* ύ-ώ Ύ-Ώ */
	0x0430,	0x044f, 468,	/* а-я А-Я */
	0x0451,	0x045c, 420,	/* ё-ќ Ё-Ќ */
	0x045e,	0x045f, 420,	/* ў-џ Ў-Џ */
	0x0561,	0x0586, 452,	/* ա-ֆ Ա-Ֆ */
	0x1f00,	0x1f07, 508,	/* ἀ-ἇ Ἀ-Ἇ */
	0x1f10,	0x1f15, 508,	/* ἐ-ἕ Ἐ-Ἕ */
	0x1f20,	0x1f27, 508,	/* ἠ-ἧ Ἠ-Ἧ */
	0x1f30,	0x1f37, 508,	/* ἰ-ἷ Ἰ-Ἷ */
	0x1f40,	0x1f45, 508,	/* ὀ-ὅ Ὀ-Ὅ */
	0x1f60,	0x1f67, 508,	/* ὠ-ὧ Ὠ-Ὧ */
	0x1f70,	0x1f71, 574,	/* ὰ-ά Ὰ-Ά */
	0x1f72,	0x1f75, 586,	/* ὲ-ή Ὲ-Ή */
	0x1f76,	0x1f77, 600,	/* ὶ-ί Ὶ-Ί */
	0x1f78,	0x1f79, 628,	/* ὸ-ό Ὸ-Ό */
	0x1f7a,	0x1f7b, 612,	/* ὺ-ύ Ὺ-Ύ */
	0x1f7c,	0x1f7d, 626,	/* ὼ-ώ Ὼ-Ώ */
	0x1f80,	0x1f87, 508,	/* ᾀ-ᾇ ᾈ-ᾏ */
	0x1f90,	0x1f97, 508,	/* ᾐ-ᾗ ᾘ-ᾟ */
	0x1fa0,	0x1fa7, 508,	/* ᾠ-ᾧ ᾨ-ᾯ */
	0x1fb0,	0x1fb1, 508,	/* ᾰ-ᾱ Ᾰ-Ᾱ */
	0x1fd0,	0x1fd1, 508,	/* ῐ-ῑ Ῐ-Ῑ */
	0x1fe0,	0x1fe1, 508,	/* ῠ-ῡ Ῠ-Ῡ */
	0x2170,	0x217f, 484,	/* ⅰ-ⅿ Ⅰ-Ⅿ */
	0x24d0,	0x24e9, 474,	/* ⓐ-ⓩ Ⓐ-Ⓩ */
	0xff41,	0xff5a, 468,	/* ａ-ｚ Ａ-Ｚ */
};

/*
 * lower case singlets
 *	2nd col is conversion excess 500
 */
Rune	__toupper1[] =
{
	0x00ff, 621,	/* ÿ Ÿ */
	0x0101, 499,	/* ā Ā */
	0x0103, 499,	/* ă Ă */
	0x0105, 499,	/* ą Ą */
	0x0107, 499,	/* ć Ć */
	0x0109, 499,	/* ĉ Ĉ */
	0x010b, 499,	/* ċ Ċ */
	0x010d, 499,	/* č Č */
	0x010f, 499,	/* ď Ď */
	0x0111, 499,	/* đ Đ */
	0x0113, 499,	/* ē Ē */
	0x0115, 499,	/* ĕ Ĕ */
	0x0117, 499,	/* ė Ė */
	0x0119, 499,	/* ę Ę */
	0x011b, 499,	/* ě Ě */
	0x011d, 499,	/* ĝ Ĝ */
	0x011f, 499,	/* ğ Ğ */
	0x0121, 499,	/* ġ Ġ */
	0x0123, 499,	/* ģ Ģ */
	0x0125, 499,	/* ĥ Ĥ */
	0x0127, 499,	/* ħ Ħ */
	0x0129, 499,	/* ĩ Ĩ */
	0x012b, 499,	/* ī Ī */
	0x012d, 499,	/* ĭ Ĭ */
	0x012f, 499,	/* į Į */
	0x0131, 268,	/* ı I */
	0x0133, 499,	/* ĳ Ĳ */
	0x0135, 499,	/* ĵ Ĵ */
	0x0137, 499,	/* ķ Ķ */
	0x013a, 499,	/* ĺ Ĺ */
	0x013c, 499,	/* ļ Ļ */
	0x013e, 499,	/* ľ Ľ */
	0x0140, 499,	/* ŀ Ŀ */
	0x0142, 499,	/* ł Ł */
	0x0144, 499,	/* ń Ń */
	0x0146, 499,	/* ņ Ņ */
	0x0148, 499,	/* ň Ň */
	0x014b, 499,	/* ŋ Ŋ */
	0x014d, 499,	/* ō Ō */
	0x014f, 499,	/* ŏ Ŏ */
	0x0151, 499,	/* ő Ő */
	0x0153, 499,	/* œ Œ */
	0x0155, 499,	/* ŕ Ŕ */
	0x0157, 499,	/* ŗ Ŗ */
	0x0159, 499,	/* ř Ř */
	0x015b, 499,	/* ś Ś */
	0x015d, 499,	/* ŝ Ŝ */
	0x015f, 499,	/* ş Ş */
	0x0161, 499,	/* š Š */
	0x0163, 499,	/* ţ Ţ */
	0x0165, 499,	/* ť Ť */
	0x0167, 499,	/* ŧ Ŧ */
	0x0169, 499,	/* ũ Ũ */
	0x016b, 499,	/* ū Ū */
	0x016d, 499,	/* ŭ Ŭ */
	0x016f, 499,	/* ů Ů */
	0x0171, 499,	/* ű Ű */
	0x0173, 499,	/* ų Ų */
	0x0175, 499,	/* ŵ Ŵ */
	0x0177, 499,	/* ŷ Ŷ */
	0x017a, 499,	/* ź Ź */
	0x017c, 499,	/* ż Ż */
	0x017e, 499,	/* ž Ž */
	0x017f, 200,	/* ſ S */
	0x0183, 499,	/* ƃ Ƃ */
	0x0185, 499,	/* ƅ Ƅ */
	0x0188, 499,	/* ƈ Ƈ */
	0x018c, 499,	/* ƌ Ƌ */
	0x0192, 499,	/* ƒ Ƒ */
	0x0199, 499,	/* ƙ Ƙ */
	0x01a1, 499,	/* ơ Ơ */
	0x01a3, 499,	/* ƣ Ƣ */
	0x01a5, 499,	/* ƥ Ƥ */
	0x01a8, 499,	/* ƨ Ƨ */
	0x01ad, 499,	/* ƭ Ƭ */
	0x01b0, 499,	/* ư Ư */
	0x01b4, 499,	/* ƴ Ƴ */
	0x01b6, 499,	/* ƶ Ƶ */
	0x01b9, 499,	/* ƹ Ƹ */
	0x01bd, 499,	/* ƽ Ƽ */
	0x01c5, 499,	/* ǅ Ǆ */
	0x01c6, 498,	/* ǆ Ǆ */
	0x01c8, 499,	/* ǈ Ǉ */
	0x01c9, 498,	/* ǉ Ǉ */
	0x01cb, 499,	/* ǋ Ǌ */
	0x01cc, 498,	/* ǌ Ǌ */
	0x01ce, 499,	/* ǎ Ǎ */
	0x01d0, 499,	/* ǐ Ǐ */
	0x01d2, 499,	/* ǒ Ǒ */
	0x01d4, 499,	/* ǔ Ǔ */
	0x01d6, 499,	/* ǖ Ǖ */
	0x01d8, 499,	/* ǘ Ǘ */
	0x01da, 499,	/* ǚ Ǚ */
	0x01dc, 499,	/* ǜ Ǜ */
	0x01df, 499,	/* ǟ Ǟ */
	0x01e1, 499,	/* ǡ Ǡ */
	0x01e3, 499,	/* ǣ Ǣ */
	0x01e5, 499,	/* ǥ Ǥ */
	0x01e7, 499,	/* ǧ Ǧ */
	0x01e9, 499,	/* ǩ Ǩ */
	0x01eb, 499,	/* ǫ Ǫ */
	0x01ed, 499,	/* ǭ Ǭ */
	0x01ef, 499,	/* ǯ Ǯ */
	0x01f2, 499,	/* ǲ Ǳ */
	0x01f3, 498,	/* ǳ Ǳ */
	0x01f5, 499,	/* ǵ Ǵ */
	0x01fb, 499,	/* ǻ Ǻ */
	0x01fd, 499,	/* ǽ Ǽ */
	0x01ff, 499,	/* ǿ Ǿ */
	0x0201, 499,	/* ȁ Ȁ */
	0x0203, 499,	/* ȃ Ȃ */
	0x0205, 499,	/* ȅ Ȅ */
	0x0207, 499,	/* ȇ Ȇ */
	0x0209, 499,	/* ȉ Ȉ */
	0x020b, 499,	/* ȋ Ȋ */
	0x020d, 499,	/* ȍ Ȍ */
	0x020f, 499,	/* ȏ Ȏ */
	0x0211, 499,	/* ȑ Ȑ */
	0x0213, 499,	/* ȓ Ȓ */
	0x0215, 499,	/* ȕ Ȕ */
	0x0217, 499,	/* ȗ Ȗ */
	0x0253, 290,	/* ɓ Ɓ */
	0x0254, 294,	/* ɔ Ɔ */
	0x025b, 297,	/* ɛ Ɛ */
	0x0260, 295,	/* ɠ Ɠ */
	0x0263, 293,	/* ɣ Ɣ */
	0x0268, 291,	/* ɨ Ɨ */
	0x0269, 289,	/* ɩ Ɩ */
	0x026f, 289,	/* ɯ Ɯ */
	0x0272, 287,	/* ɲ Ɲ */
	0x0283, 282,	/* ʃ Ʃ */
	0x0288, 282,	/* ʈ Ʈ */
	0x0292, 281,	/* ʒ Ʒ */
	0x03ac, 462,	/* ά Ά */
	0x03cc, 436,	/* ό Ό */
	0x03d0, 438,	/* ϐ Β */
	0x03d1, 443,	/* ϑ Θ */
	0x03d5, 453,	/* ϕ Φ */
	0x03d6, 446,	/* ϖ Π */
	0x03e3, 499,	/* ϣ Ϣ */
	0x03e5, 499,	/* ϥ Ϥ */
	0x03e7, 499,	/* ϧ Ϧ */
	0x03e9, 499,	/* ϩ Ϩ */
	0x03eb, 499,	/* ϫ Ϫ */
	0x03ed, 499,	/* ϭ Ϭ */
	0x03ef, 499,	/* ϯ Ϯ */
	0x03f0, 414,	/* ϰ Κ */
	0x03f1, 420,	/* ϱ Ρ */
	0x0461, 499,	/* ѡ Ѡ */
	0x0463, 499,	/* ѣ Ѣ */
	0x0465, 499,	/* ѥ Ѥ */
	0x0467, 499,	/* ѧ Ѧ */
	0x0469, 499,	/* ѩ Ѩ */
	0x046b, 499,	/* ѫ Ѫ */
	0x046d, 499,	/* ѭ Ѭ */
	0x046f, 499,	/* ѯ Ѯ */
	0x0471, 499,	/* ѱ Ѱ */
	0x0473, 499,	/* ѳ Ѳ */
	0x0475, 499,	/* ѵ Ѵ */
	0x0477, 499,	/* ѷ Ѷ */
	0x0479, 499,	/* ѹ Ѹ */
	0x047b, 499,	/* ѻ Ѻ */
	0x047d, 499,	/* ѽ Ѽ */
	0x047f, 499,	/* ѿ Ѿ */
	0x0481, 499,	/* ҁ Ҁ */
	0x0491, 499,	/* ґ Ґ */
	0x0493, 499,	/* ғ Ғ */
	0x0495, 499,	/* ҕ Ҕ */
	0x0497, 499,	/* җ Җ */
	0x0499, 499,	/* ҙ Ҙ */
	0x049b, 499,	/* қ Қ */
	0x049d, 499,	/* ҝ Ҝ */
	0x049f, 499,	/* ҟ Ҟ */
	0x04a1, 499,	/* ҡ Ҡ */
	0x04a3, 499,	/* ң Ң */
	0x04a5, 499,	/* ҥ Ҥ */
	0x04a7, 499,	/* ҧ Ҧ */
	0x04a9, 499,	/* ҩ Ҩ */
	0x04ab, 499,	/* ҫ Ҫ */
	0x04ad, 499,	/* ҭ Ҭ */
	0x04af, 499,	/* ү Ү */
	0x04b1, 499,	/* ұ Ұ */
	0x04b3, 499,	/* ҳ Ҳ */
	0x04b5, 499,	/* ҵ Ҵ */
	0x04b7, 499,	/* ҷ Ҷ */
	0x04b9, 499,	/* ҹ Ҹ */
	0x04bb, 499,	/* һ Һ */
	0x04bd, 499,	/* ҽ Ҽ */
	0x04bf, 499,	/* ҿ Ҿ */
	0x04c2, 499,	/* ӂ Ӂ */
	0x04c4, 499,	/* ӄ Ӄ */
	0x04c8, 499,	/* ӈ Ӈ */
	0x04cc, 499,	/* ӌ Ӌ */
	0x04d1, 499,	/* ӑ Ӑ */
	0x04d3, 499,	/* ӓ Ӓ */
	0x04d5, 499,	/* ӕ Ӕ */
	0x04d7, 499,	/* ӗ Ӗ */
	0x04d9, 499,	/* ә Ә */
	0x04db, 499,	/* ӛ Ӛ */
	0x04dd, 499,	/* ӝ Ӝ */
	0x04df, 499,	/* ӟ Ӟ */
	0x04e1, 499,	/* ӡ Ӡ */
	0x04e3, 499,	/* ӣ Ӣ */
	0x04e5, 499,	/* ӥ Ӥ */
	0x04e7, 499,	/* ӧ Ӧ */
	0x04e9, 499,	/* ө Ө */
	0x04eb, 499,	/* ӫ Ӫ */
	0x04ef, 499,	/* ӯ Ӯ */
	0x04f1, 499,	/* ӱ Ӱ */
	0x04f3, 499,	/* ӳ Ӳ */
	0x04f5, 499,	/* ӵ Ӵ */
	0x04f9, 499,	/* ӹ Ӹ */
	0x1e01, 499,	/* ḁ Ḁ */
	0x1e03, 499,	/* ḃ Ḃ */
	0x1e05, 499,	/* ḅ Ḅ */
	0x1e07, 499,	/* ḇ Ḇ */
	0x1e09, 499,	/* ḉ Ḉ */
	0x1e0b, 499,	/* ḋ Ḋ */
	0x1e0d, 499,	/* ḍ Ḍ */
	0x1e0f, 499,	/* ḏ Ḏ */
	0x1e11, 499,	/* ḑ Ḑ */
	0x1e13, 499,	/* ḓ Ḓ */
	0x1e15, 499,	/* ḕ Ḕ */
	0x1e17, 499,	/* ḗ Ḗ */
	0x1e19, 499,	/* ḙ Ḙ */
	0x1e1b, 499,	/* ḛ Ḛ */
	0x1e1d, 499,	/* ḝ Ḝ */
	0x1e1f, 499,	/* ḟ Ḟ */
	0x1e21, 499,	/* ḡ Ḡ */
	0x1e23, 499,	/* ḣ Ḣ */
	0x1e25, 499,	/* ḥ Ḥ */
	0x1e27, 499,	/* ḧ Ḧ */
	0x1e29, 499,	/* ḩ Ḩ */
	0x1e2b, 499,	/* ḫ Ḫ */
	0x1e2d, 499,	/* ḭ Ḭ */
	0x1e2f, 499,	/* ḯ Ḯ */
	0x1e31, 499,	/* ḱ Ḱ */
	0x1e33, 499,	/* ḳ Ḳ */
	0x1e35, 499,	/* ḵ Ḵ */
	0x1e37, 499,	/* ḷ Ḷ */
	0x1e39, 499,	/* ḹ Ḹ */
	0x1e3b, 499,	/* ḻ Ḻ */
	0x1e3d, 499,	/* ḽ Ḽ */
	0x1e3f, 499,	/* ḿ Ḿ */
	0x1e41, 499,	/* ṁ Ṁ */
	0x1e43, 499,	/* ṃ Ṃ */
	0x1e45, 499,	/* ṅ Ṅ */
	0x1e47, 499,	/* ṇ Ṇ */
	0x1e49, 499,	/* ṉ Ṉ */
	0x1e4b, 499,	/* ṋ Ṋ */
	0x1e4d, 499,	/* ṍ Ṍ */
	0x1e4f, 499,	/* ṏ Ṏ */
	0x1e51, 499,	/* ṑ Ṑ */
	0x1e53, 499,	/* ṓ Ṓ */
	0x1e55, 499,	/* ṕ Ṕ */
	0x1e57, 499,	/* ṗ Ṗ */
	0x1e59, 499,	/* ṙ Ṙ */
	0x1e5b, 499,	/* ṛ Ṛ */
	0x1e5d, 499,	/* ṝ Ṝ */
	0x1e5f, 499,	/* ṟ Ṟ */
	0x1e61, 499,	/* ṡ Ṡ */
	0x1e63, 499,	/* ṣ Ṣ */
	0x1e65, 499,	/* ṥ Ṥ */
	0x1e67, 499,	/* ṧ Ṧ */
	0x1e69, 499,	/* ṩ Ṩ */
	0x1e6b, 499,	/* ṫ Ṫ */
	0x1e6d, 499,	/* ṭ Ṭ */
	0x1e6f, 499,	/* ṯ Ṯ */
	0x1e71, 499,	/* ṱ Ṱ */
	0x1e73, 499,	/* ṳ Ṳ */
	0x1e75, 499,	/* ṵ Ṵ */
	0x1e77, 499,	/* ṷ Ṷ */
	0x1e79, 499,	/* ṹ Ṹ */
	0x1e7b, 499,	/* ṻ Ṻ */
	0x1e7d, 499,	/* ṽ Ṽ */
	0x1e7f, 499,	/* ṿ Ṿ */
	0x1e81, 499,	/* ẁ Ẁ */
	0x1e83, 499,	/* ẃ Ẃ */
	0x1e85, 499,	/* ẅ Ẅ */
	0x1e87, 499,	/* ẇ Ẇ */
	0x1e89, 499,	/* ẉ Ẉ */
	0x1e8b, 499,	/* ẋ Ẋ */
	0x1e8d, 499,	/* ẍ Ẍ */
	0x1e8f, 499,	/* ẏ Ẏ */
	0x1e91, 499,	/* ẑ Ẑ */
	0x1e93, 499,	/* ẓ Ẓ */
	0x1e95, 499,	/* ẕ Ẕ */
	0x1ea1, 499,	/* ạ Ạ */
	0x1ea3, 499,	/* ả Ả */
	0x1ea5, 499,	/* ấ Ấ */
	0x1ea7, 499,	/* ầ Ầ */
	0x1ea9, 499,	/* ẩ Ẩ */
	0x1eab, 499,	/* ẫ Ẫ */
	0x1ead, 499,	/* ậ Ậ */
	0x1eaf, 499,	/* ắ Ắ */
	0x1eb1, 499,	/* ằ Ằ */
	0x1eb3, 499,	/* ẳ Ẳ */
	0x1eb5, 499,	/* ẵ Ẵ */
	0x1eb7, 499,	/* ặ Ặ */
	0x1eb9, 499,	/* ẹ Ẹ */
	0x1ebb, 499,	/* ẻ Ẻ */
	0x1ebd, 499,	/* ẽ Ẽ */
	0x1ebf, 499,	/* ế Ế */
	0x1ec1, 499,	/* ề Ề */
	0x1ec3, 499,	/* ể Ể */
	0x1ec5, 499,	/* ễ Ễ */
	0x1ec7, 499,	/* ệ Ệ */
	0x1ec9, 499,	/* ỉ Ỉ */
	0x1ecb, 499,	/* ị Ị */
	0x1ecd, 499,	/* ọ Ọ */
	0x1ecf, 499,	/* ỏ Ỏ */
	0x1ed1, 499,	/* ố Ố */
	0x1ed3, 499,	/* ồ Ồ */
	0x1ed5, 499,	/* ổ Ổ */
	0x1ed7, 499,	/* ỗ Ỗ */
	0x1ed9, 499,	/* ộ Ộ */
	0x1edb, 499,	/* ớ Ớ */
	0x1edd, 499,	/* ờ Ờ */
	0x1edf, 499,	/* ở Ở */
	0x1ee1, 499,	/* ỡ Ỡ */
	0x1ee3, 499,	/* ợ Ợ */
	0x1ee5, 499,	/* ụ Ụ */
	0x1ee7, 499,	/* ủ Ủ */
	0x1ee9, 499,	/* ứ Ứ */
	0x1eeb, 499,	/* ừ Ừ */
	0x1eed, 499,	/* ử Ử */
	0x1eef, 499,	/* ữ Ữ */
	0x1ef1, 499,	/* ự Ự */
	0x1ef3, 499,	/* ỳ Ỳ */
	0x1ef5, 499,	/* ỵ Ỵ */
	0x1ef7, 499,	/* ỷ Ỷ */
	0x1ef9, 499,	/* ỹ Ỹ */
	0x1f51, 508,	/* ὑ Ὑ */
	0x1f53, 508,	/* ὓ Ὓ */
	0x1f55, 508,	/* ὕ Ὕ */
	0x1f57, 508,	/* ὗ Ὗ */
	0x1fb3, 509,	/* ᾳ ᾼ */
	0x1fc3, 509,	/* ῃ ῌ */
	0x1fe5, 507,	/* ῥ Ῥ */
	0x1ff3, 509,	/* ῳ ῼ */
};

/*
 * upper case ranges
 *	3rd col is conversion excess 500
 */
Rune	__tolower2[] =
{
	0x0041,	0x005a, 532,	/* A-Z a-z */
	0x00c0,	0x00d6, 532,	/* À-Ö à-ö */
	0x00d8,	0x00de, 532,	/* Ø-Þ ø-þ */
	0x0189,	0x018a, 705,	/* Ɖ-Ɗ ɖ-ɗ */
	0x018e,	0x018f, 702,	/* Ǝ-Ə ɘ-ə */
	0x01b1,	0x01b2, 717,	/* Ʊ-Ʋ ʊ-ʋ */
	0x0388,	0x038a, 537,	/* Έ-Ί έ-ί */
	0x038e,	0x038f, 563,	/* Ύ-Ώ ύ-ώ */
	0x0391,	0x03a1, 532,	/* Α-Ρ α-ρ */
	0x03a3,	0x03ab, 532,	/* Σ-Ϋ σ-ϋ */
	0x0401,	0x040c, 580,	/* Ё-Ќ ё-ќ */
	0x040e,	0x040f, 580,	/* Ў-Џ ў-џ */
	0x0410,	0x042f, 532,	/* А-Я а-я */
	0x0531,	0x0556, 548,	/* Ա-Ֆ ա-ֆ */
	0x10a0,	0x10c5, 548,	/* Ⴀ-Ⴥ ა-ჵ */
	0x1f08,	0x1f0f, 492,	/* Ἀ-Ἇ ἀ-ἇ */
	0x1f18,	0x1f1d, 492,	/* Ἐ-Ἕ ἐ-ἕ */
	0x1f28,	0x1f2f, 492,	/* Ἠ-Ἧ ἠ-ἧ */
	0x1f38,	0x1f3f, 492,	/* Ἰ-Ἷ ἰ-ἷ */
	0x1f48,	0x1f4d, 492,	/* Ὀ-Ὅ ὀ-ὅ */
	0x1f68,	0x1f6f, 492,	/* Ὠ-Ὧ ὠ-ὧ */
	0x1f88,	0x1f8f, 492,	/* ᾈ-ᾏ ᾀ-ᾇ */
	0x1f98,	0x1f9f, 492,	/* ᾘ-ᾟ ᾐ-ᾗ */
	0x1fa8,	0x1faf, 492,	/* ᾨ-ᾯ ᾠ-ᾧ */
	0x1fb8,	0x1fb9, 492,	/* Ᾰ-Ᾱ ᾰ-ᾱ */
	0x1fba,	0x1fbb, 426,	/* Ὰ-Ά ὰ-ά */
	0x1fc8,	0x1fcb, 414,	/* Ὲ-Ή ὲ-ή */
	0x1fd8,	0x1fd9, 492,	/* Ῐ-Ῑ ῐ-ῑ */
	0x1fda,	0x1fdb, 400,	/* Ὶ-Ί ὶ-ί */
	0x1fe8,	0x1fe9, 492,	/* Ῠ-Ῡ ῠ-ῡ */
	0x1fea,	0x1feb, 388,	/* Ὺ-Ύ ὺ-ύ */
	0x1ff8,	0x1ff9, 372,	/* Ὸ-Ό ὸ-ό */
	0x1ffa,	0x1ffb, 374,	/* Ὼ-Ώ ὼ-ώ */
	0x2160,	0x216f, 516,	/* Ⅰ-Ⅿ ⅰ-ⅿ */
	0x24b6,	0x24cf, 526,	/* Ⓐ-Ⓩ ⓐ-ⓩ */
	0xff21,	0xff3a, 532,	/* Ａ-Ｚ ａ-ｚ */
};

/*
 * upper case singlets
 *	2nd col is conversion excess 500
 */
Rune	__tolower1[] =
{
	0x0100, 501,	/* Ā ā */
	0x0102, 501,	/* Ă ă */
	0x0104, 501,	/* Ą ą */
	0x0106, 501,	/* Ć ć */
	0x0108, 501,	/* Ĉ ĉ */
	0x010a, 501,	/* Ċ ċ */
	0x010c, 501,	/* Č č */
	0x010e, 501,	/* Ď ď */
	0x0110, 501,	/* Đ đ */
	0x0112, 501,	/* Ē ē */
	0x0114, 501,	/* Ĕ ĕ */
	0x0116, 501,	/* Ė ė */
	0x0118, 501,	/* Ę ę */
	0x011a, 501,	/* Ě ě */
	0x011c, 501,	/* Ĝ ĝ */
	0x011e, 501,	/* Ğ ğ */
	0x0120, 501,	/* Ġ ġ */
	0x0122, 501,	/* Ģ ģ */
	0x0124, 501,	/* Ĥ ĥ */
	0x0126, 501,	/* Ħ ħ */
	0x0128, 501,	/* Ĩ ĩ */
	0x012a, 501,	/* Ī ī */
	0x012c, 501,	/* Ĭ ĭ */
	0x012e, 501,	/* Į į */
	0x0130, 301,	/* İ i */
	0x0132, 501,	/* Ĳ ĳ */
	0x0134, 501,	/* Ĵ ĵ */
	0x0136, 501,	/* Ķ ķ */
	0x0139, 501,	/* Ĺ ĺ */
	0x013b, 501,	/* Ļ ļ */
	0x013d, 501,	/* Ľ ľ */
	0x013f, 501,	/* Ŀ ŀ */
	0x0141, 501,	/* Ł ł */
	0x0143, 501,	/* Ń ń */
	0x0145, 501,	/* Ņ ņ */
	0x0147, 501,	/* Ň ň */
	0x014a, 501,	/* Ŋ ŋ */
	0x014c, 501,	/* Ō ō */
	0x014e, 501,	/* Ŏ ŏ */
	0x0150, 501,	/* Ő ő */
	0x0152, 501,	/* Œ œ */
	0x0154, 501,	/* Ŕ ŕ */
	0x0156, 501,	/* Ŗ ŗ */
	0x0158, 501,	/* Ř ř */
	0x015a, 501,	/* Ś ś */
	0x015c, 501,	/* Ŝ ŝ */
	0x015e, 501,	/* Ş ş */
	0x0160, 501,	/* Š š */
	0x0162, 501,	/* Ţ ţ */
	0x0164, 501,	/* Ť ť */
	0x0166, 501,	/* Ŧ ŧ */
	0x0168, 501,	/* Ũ ũ */
	0x016a, 501,	/* Ū ū */
	0x016c, 501,	/* Ŭ ŭ */
	0x016e, 501,	/* Ů ů */
	0x0170, 501,	/* Ű ű */
	0x0172, 501,	/* Ų ų */
	0x0174, 501,	/* Ŵ ŵ */
	0x0176, 501,	/* Ŷ ŷ */
	0x0178, 379,	/* Ÿ ÿ */
	0x0179, 501,	/* Ź ź */
	0x017b, 501,	/* Ż ż */
	0x017d, 501,	/* Ž ž */
	0x0181, 710,	/* Ɓ ɓ */
	0x0182, 501,	/* Ƃ ƃ */
	0x0184, 501,	/* Ƅ ƅ */
	0x0186, 706,	/* Ɔ ɔ */
	0x0187, 501,	/* Ƈ ƈ */
	0x018b, 501,	/* Ƌ ƌ */
	0x0190, 703,	/* Ɛ ɛ */
	0x0191, 501,	/* Ƒ ƒ */
	0x0193, 705,	/* Ɠ ɠ */
	0x0194, 707,	/* Ɣ ɣ */
	0x0196, 711,	/* Ɩ ɩ */
	0x0197, 709,	/* Ɨ ɨ */
	0x0198, 501,	/* Ƙ ƙ */
	0x019c, 711,	/* Ɯ ɯ */
	0x019d, 713,	/* Ɲ ɲ */
	0x01a0, 501,	/* Ơ ơ */
	0x01a2, 501,	/* Ƣ ƣ */
	0x01a4, 501,	/* Ƥ ƥ */
	0x01a7, 501,	/* Ƨ ƨ */
	0x01a9, 718,	/* Ʃ ʃ */
	0x01ac, 501,	/* Ƭ ƭ */
	0x01ae, 718,	/* Ʈ ʈ */
	0x01af, 501,	/* Ư ư */
	0x01b3, 501,	/* Ƴ ƴ */
	0x01b5, 501,	/* Ƶ ƶ */
	0x01b7, 719,	/* Ʒ ʒ */
	0x01b8, 501,	/* Ƹ ƹ */
	0x01bc, 501,	/* Ƽ ƽ */
	0x01c4, 502,	/* Ǆ ǆ */
	0x01c5, 501,	/* ǅ ǆ */
	0x01c7, 502,	/* Ǉ ǉ */
	0x01c8, 501,	/* ǈ ǉ */
	0x01ca, 502,	/* Ǌ ǌ */
	0x01cb, 501,	/* ǋ ǌ */
	0x01cd, 501,	/* Ǎ ǎ */
	0x01cf, 501,	/* Ǐ ǐ */
	0x01d1, 501,	/* Ǒ ǒ */
	0x01d3, 501,	/* Ǔ ǔ */
	0x01d5, 501,	/* Ǖ ǖ */
	0x01d7, 501,	/* Ǘ ǘ */
	0x01d9, 501,	/* Ǚ ǚ */
	0x01db, 501,	/* Ǜ ǜ */
	0x01de, 501,	/* Ǟ ǟ */
	0x01e0, 501,	/* Ǡ ǡ */
	0x01e2, 501,	/* Ǣ ǣ */
	0x01e4, 501,	/* Ǥ ǥ */
	0x01e6, 501,	/* Ǧ ǧ */
	0x01e8, 501,	/* Ǩ ǩ */
	0x01ea, 501,	/* Ǫ ǫ */
	0x01ec, 501,	/* Ǭ ǭ */
	0x01ee, 501,	/* Ǯ ǯ */
	0x01f1, 502,	/* Ǳ ǳ */
	0x01f2, 501,	/* ǲ ǳ */
	0x01f4, 501,	/* Ǵ ǵ */
	0x01fa, 501,	/* Ǻ ǻ */
	0x01fc, 501,	/* Ǽ ǽ */
	0x01fe, 501,	/* Ǿ ǿ */
	0x0200, 501,	/* Ȁ ȁ */
	0x0202, 501,	/* Ȃ ȃ */
	0x0204, 501,	/* Ȅ ȅ */
	0x0206, 501,	/* Ȇ ȇ */
	0x0208, 501,	/* Ȉ ȉ */
	0x020a, 501,	/* Ȋ ȋ */
	0x020c, 501,	/* Ȍ ȍ */
	0x020e, 501,	/* Ȏ ȏ */
	0x0210, 501,	/* Ȑ ȑ */
	0x0212, 501,	/* Ȓ ȓ */
	0x0214, 501,	/* Ȕ ȕ */
	0x0216, 501,	/* Ȗ ȗ */
	0x0386, 538,	/* Ά ά */
	0x038c, 564,	/* Ό ό */
	0x03e2, 501,	/* Ϣ ϣ */
	0x03e4, 501,	/* Ϥ ϥ */
	0x03e6, 501,	/* Ϧ ϧ */
	0x03e8, 501,	/* Ϩ ϩ */
	0x03ea, 501,	/* Ϫ ϫ */
	0x03ec, 501,	/* Ϭ ϭ */
	0x03ee, 501,	/* Ϯ ϯ */
	0x0460, 501,	/* Ѡ ѡ */
	0x0462, 501,	/* Ѣ ѣ */
	0x0464, 501,	/* Ѥ ѥ */
	0x0466, 501,	/* Ѧ ѧ */
	0x0468, 501,	/* Ѩ ѩ */
	0x046a, 501,	/* Ѫ ѫ */
	0x046c, 501,	/* Ѭ ѭ */
	0x046e, 501,	/* Ѯ ѯ */
	0x0470, 501,	/* Ѱ ѱ */
	0x0472, 501,	/* Ѳ ѳ */
	0x0474, 501,	/* Ѵ ѵ */
	0x0476, 501,	/* Ѷ ѷ */
	0x0478, 501,	/* Ѹ ѹ */
	0x047a, 501,	/* Ѻ ѻ */
	0x047c, 501,	/* Ѽ ѽ */
	0x047e, 501,	/* Ѿ ѿ */
	0x0480, 501,	/* Ҁ ҁ */
	0x0490, 501,	/* Ґ ґ */
	0x0492, 501,	/* Ғ ғ */
	0x0494, 501,	/* Ҕ ҕ */
	0x0496, 501,	/* Җ җ */
	0x0498, 501,	/* Ҙ ҙ */
	0x049a, 501,	/* Қ қ */
	0x049c, 501,	/* Ҝ ҝ */
	0x049e, 501,	/* Ҟ ҟ */
	0x04a0, 501,	/* Ҡ ҡ */
	0x04a2, 501,	/* Ң ң */
	0x04a4, 501,	/* Ҥ ҥ */
	0x04a6, 501,	/* Ҧ ҧ */
	0x04a8, 501,	/* Ҩ ҩ */
	0x04aa, 501,	/* Ҫ ҫ */
	0x04ac, 501,	/* Ҭ ҭ */
	0x04ae, 501,	/* Ү ү */
	0x04b0, 501,	/* Ұ ұ */
	0x04b2, 501,	/* Ҳ ҳ */
	0x04b4, 501,	/* Ҵ ҵ */
	0x04b6, 501,	/* Ҷ ҷ */
	0x04b8, 501,	/* Ҹ ҹ */
	0x04ba, 501,	/* Һ һ */
	0x04bc, 501,	/* Ҽ ҽ */
	0x04be, 501,	/* Ҿ ҿ */
	0x04c1, 501,	/* Ӂ ӂ */
	0x04c3, 501,	/* Ӄ ӄ */
	0x04c7, 501,	/* Ӈ ӈ */
	0x04cb, 501,	/* Ӌ ӌ */
	0x04d0, 501,	/* Ӑ ӑ */
	0x04d2, 501,	/* Ӓ ӓ */
	0x04d4, 501,	/* Ӕ ӕ */
	0x04d6, 501,	/* Ӗ ӗ */
	0x04d8, 501,	/* Ә ә */
	0x04da, 501,	/* Ӛ ӛ */
	0x04dc, 501,	/* Ӝ ӝ */
	0x04de, 501,	/* Ӟ ӟ */
	0x04e0, 501,	/* Ӡ ӡ */
	0x04e2, 501,	/* Ӣ ӣ */
	0x04e4, 501,	/* Ӥ ӥ */
	0x04e6, 501,	/* Ӧ ӧ */
	0x04e8, 501,	/* Ө ө */
	0x04ea, 501,	/* Ӫ ӫ */
	0x04ee, 501,	/* Ӯ ӯ */
	0x04f0, 501,	/* Ӱ ӱ */
	0x04f2, 501,	/* Ӳ ӳ */
	0x04f4, 501,	/* Ӵ ӵ */
	0x04f8, 501,	/* Ӹ ӹ */
	0x1e00, 501,	/* Ḁ ḁ */
	0x1e02, 501,	/* Ḃ ḃ */
	0x1e04, 501,	/* Ḅ ḅ */
	0x1e06, 501,	/* Ḇ ḇ */
	0x1e08, 501,	/* Ḉ ḉ */
	0x1e0a, 501,	/* Ḋ ḋ */
	0x1e0c, 501,	/* Ḍ ḍ */
	0x1e0e, 501,	/* Ḏ ḏ */
	0x1e10, 501,	/* Ḑ ḑ */
	0x1e12, 501,	/* Ḓ ḓ */
	0x1e14, 501,	/* Ḕ ḕ */
	0x1e16, 501,	/* Ḗ ḗ */
	0x1e18, 501,	/* Ḙ ḙ */
	0x1e1a, 501,	/* Ḛ ḛ */
	0x1e1c, 501,	/* Ḝ ḝ */
	0x1e1e, 501,	/* Ḟ ḟ */
	0x1e20, 501,	/* Ḡ ḡ */
	0x1e22, 501,	/* Ḣ ḣ */
	0x1e24, 501,	/* Ḥ ḥ */
	0x1e26, 501,	/* Ḧ ḧ */
	0x1e28, 501,	/* Ḩ ḩ */
	0x1e2a, 501,	/* Ḫ ḫ */
	0x1e2c, 501,	/* Ḭ ḭ */
	0x1e2e, 501,	/* Ḯ ḯ */
	0x1e30, 501,	/* Ḱ ḱ */
	0x1e32, 501,	/* Ḳ ḳ */
	0x1e34, 501,	/* Ḵ ḵ */
	0x1e36, 501,	/* Ḷ ḷ */
	0x1e38, 501,	/* Ḹ ḹ */
	0x1e3a, 501,	/* Ḻ ḻ */
	0x1e3c, 501,	/* Ḽ ḽ */
	0x1e3e, 501,	/* Ḿ ḿ */
	0x1e40, 501,	/* Ṁ ṁ */
	0x1e42, 501,	/* Ṃ ṃ */
	0x1e44, 501,	/* Ṅ ṅ */
	0x1e46, 501,	/* Ṇ ṇ */
	0x1e48, 501,	/* Ṉ ṉ */
	0x1e4a, 501,	/* Ṋ ṋ */
	0x1e4c, 501,	/* Ṍ ṍ */
	0x1e4e, 501,	/* Ṏ ṏ */
	0x1e50, 501,	/* Ṑ ṑ */
	0x1e52, 501,	/* Ṓ ṓ */
	0x1e54, 501,	/* Ṕ ṕ */
	0x1e56, 501,	/* Ṗ ṗ */
	0x1e58, 501,	/* Ṙ ṙ */
	0x1e5a, 501,	/* Ṛ ṛ */
	0x1e5c, 501,	/* Ṝ ṝ */
	0x1e5e, 501,	/* Ṟ ṟ */
	0x1e60, 501,	/* Ṡ ṡ */
	0x1e62, 501,	/* Ṣ ṣ */
	0x1e64, 501,	/* Ṥ ṥ */
	0x1e66, 501,	/* Ṧ ṧ */
	0x1e68, 501,	/* Ṩ ṩ */
	0x1e6a, 501,	/* Ṫ ṫ */
	0x1e6c, 501,	/* Ṭ ṭ */
	0x1e6e, 501,	/* Ṯ ṯ */
	0x1e70, 501,	/* Ṱ ṱ */
	0x1e72, 501,	/* Ṳ ṳ */
	0x1e74, 501,	/* Ṵ ṵ */
	0x1e76, 501,	/* Ṷ ṷ */
	0x1e78, 501,	/* Ṹ ṹ */
	0x1e7a, 501,	/* Ṻ ṻ */
	0x1e7c, 501,	/* Ṽ ṽ */
	0x1e7e, 501,	/* Ṿ ṿ */
	0x1e80, 501,	/* Ẁ ẁ */
	0x1e82, 501,	/* Ẃ ẃ */
	0x1e84, 501,	/* Ẅ ẅ */
	0x1e86, 501,	/* Ẇ ẇ */
	0x1e88, 501,	/* Ẉ ẉ */
	0x1e8a, 501,	/* Ẋ ẋ */
	0x1e8c, 501,	/* Ẍ ẍ */
	0x1e8e, 501,	/* Ẏ ẏ */
	0x1e90, 501,	/* Ẑ ẑ */
	0x1e92, 501,	/* Ẓ ẓ */
	0x1e94, 501,	/* Ẕ ẕ */
	0x1ea0, 501,	/* Ạ ạ */
	0x1ea2, 501,	/* Ả ả */
	0x1ea4, 501,	/* Ấ ấ */
	0x1ea6, 501,	/* Ầ ầ */
	0x1ea8, 501,	/* Ẩ ẩ */
	0x1eaa, 501,	/* Ẫ ẫ */
	0x1eac, 501,	/* Ậ ậ */
	0x1eae, 501,	/* Ắ ắ */
	0x1eb0, 501,	/* Ằ ằ */
	0x1eb2, 501,	/* Ẳ ẳ */
	0x1eb4, 501,	/* Ẵ ẵ */
	0x1eb6, 501,	/* Ặ ặ */
	0x1eb8, 501,	/* Ẹ ẹ */
	0x1eba, 501,	/* Ẻ ẻ */
	0x1ebc, 501,	/* Ẽ ẽ */
	0x1ebe, 501,	/* Ế ế */
	0x1ec0, 501,	/* Ề ề */
	0x1ec2, 501,	/* Ể ể */
	0x1ec4, 501,	/* Ễ ễ */
	0x1ec6, 501,	/* Ệ ệ */
	0x1ec8, 501,	/* Ỉ ỉ */
	0x1eca, 501,	/* Ị ị */
	0x1ecc, 501,	/* Ọ ọ */
	0x1ece, 501,	/* Ỏ ỏ */
	0x1ed0, 501,	/* Ố ố */
	0x1ed2, 501,	/* Ồ ồ */
	0x1ed4, 501,	/* Ổ ổ */
	0x1ed6, 501,	/* Ỗ ỗ */
	0x1ed8, 501,	/* Ộ ộ */
	0x1eda, 501,	/* Ớ ớ */
	0x1edc, 501,	/* Ờ ờ */
	0x1ede, 501,	/* Ở ở */
	0x1ee0, 501,	/* Ỡ ỡ */
	0x1ee2, 501,	/* Ợ ợ */
	0x1ee4, 501,	/* Ụ ụ */
	0x1ee6, 501,	/* Ủ ủ */
	0x1ee8, 501,	/* Ứ ứ */
	0x1eea, 501,	/* Ừ ừ */
	0x1eec, 501,	/* Ử ử */
	0x1eee, 501,	/* Ữ ữ */
	0x1ef0, 501,	/* Ự ự */
	0x1ef2, 501,	/* Ỳ ỳ */
	0x1ef4, 501,	/* Ỵ ỵ */
	0x1ef6, 501,	/* Ỷ ỷ */
	0x1ef8, 501,	/* Ỹ ỹ */
	0x1f59, 492,	/* Ὑ ὑ */
	0x1f5b, 492,	/* Ὓ ὓ */
	0x1f5d, 492,	/* Ὕ ὕ */
	0x1f5f, 492,	/* Ὗ ὗ */
	0x1fbc, 491,	/* ᾼ ᾳ */
	0x1fcc, 491,	/* ῌ ῃ */
	0x1fec, 493,	/* Ῥ ῥ */
	0x1ffc, 491,	/* ῼ ῳ */
};

/*
 * title characters are those between
 * upper and lower case. ie DZ Dz dz
 */
Rune	__totitle1[] =
{
	0x01c4, 501,	/* Ǆ ǅ */
	0x01c6, 499,	/* ǆ ǅ */
	0x01c7, 501,	/* Ǉ ǈ */
	0x01c9, 499,	/* ǉ ǈ */
	0x01ca, 501,	/* Ǌ ǋ */
	0x01cc, 499,	/* ǌ ǋ */
	0x01f1, 501,	/* Ǳ ǲ */
	0x01f3, 499,	/* ǳ ǲ */
};

Rune*
rune_bsearch(Rune c, Rune *t, int n, int ne)
{
	Rune *p = NULL;
	int m = 0;

	while(n > 1) {
		m = n/2;
		p = t + m*ne;
		if(c >= p[0]) {
			t = p;
			n = n-m;
		} else
			n = m;
	}
	if(n && c >= t[0])
		return t;
	return 0;
}

pub Rune tolowerrune(Rune c) {
	Rune *p = rune_bsearch(c, __tolower2, nelem(__tolower2)/3, 3);
	if(p && c >= p[0] && c <= p[1])
		return c + p[2] - 500;
	p = rune_bsearch(c, __tolower1, nelem(__tolower1)/2, 2);
	if(p && c == p[0])
		return c + p[1] - 500;
	return c;
}

pub Rune toupperrune(Rune c) {
	Rune *p = rune_bsearch(c, __toupper2, nelem(__toupper2)/3, 3);
	if(p && c >= p[0] && c <= p[1])
		return c + p[2] - 500;
	p = rune_bsearch(c, __toupper1, nelem(__toupper1)/2, 2);
	if(p && c == p[0])
		return c + p[1] - 500;
	return c;
}

pub Rune totitlerune(Rune c) {
	Rune *p = rune_bsearch(c, __totitle1, nelem(__totitle1)/2, 2);
	if(p && c == p[0])
		return c + p[1] - 500;
	return c;
}

pub bool islowerrune(Rune c) {
	Rune *p = rune_bsearch(c, __toupper2, nelem(__toupper2)/3, 3);
	if(p && c >= p[0] && c <= p[1])
		return 1;
	p = rune_bsearch(c, __toupper1, nelem(__toupper1)/2, 2);
	if(p && c == p[0])
		return 1;
	return 0;
}

bool isupperrune(Rune c) {
	Rune *p = rune_bsearch(c, __tolower2, nelem(__tolower2)/3, 3);
	if (p && c >= p[0] && c <= p[1]) {
		return true;
	}
	p = rune_bsearch(c, __tolower1, nelem(__tolower1)/2, 2);
	if (p && c == p[0]) {
		return true;
	}
	return false;
}

pub bool isalpharune(Rune c) {
	Rune *p = NULL;

	if(isupperrune(c) || islowerrune(c))
		return 1;
	p = rune_bsearch(c, __alpha2, nelem(__alpha2)/2, 2);
	if(p && c >= p[0] && c <= p[1])
		return 1;
	p = rune_bsearch(c, __alpha1, nelem(__alpha1), 1);
	if(p && c == p[0])
		return 1;
	return 0;
}

pub bool istitlerune(Rune c) {
	return isupperrune(c) && islowerrune(c);
}

pub bool isspacerune(Rune c) {
	Rune *p = NULL;
	p = rune_bsearch(c, __space2, nelem(__space2)/2, 2);
	if(p && c >= p[0] && c <= p[1])
		return 1;
	return 0;
}

// Copies the rune to at most UTFmax bytes starting at str and returns
// the number of bytes copied.
pub int runetochar(char *str, Rune *rune) {
	int32_t c = *rune;

	// one-character mapping: 00000-0007F => 00-7F
	if (c <= Rune1) {
		str[0] = c;
		return 1;
	}

	/*
	 * two character sequence
	 *	00080-007FF => T2 Tx
	 */
	if(c <= Rune2) {
		str[0] = T2 | (c >> 1*Bitx);
		str[1] = Tx | (c & Maskx);
		return 2;
	}

	/*
	 * three character sequence
	 *	00800-0FFFF => T3 Tx Tx
	 */
	if(c > Runemax)
		c = Runeerror;
	if(c <= Rune3) {
		str[0] = T3 |  (c >> 2*Bitx);
		str[1] = Tx | ((c >> 1*Bitx) & Maskx);
		str[2] = Tx |  (c & Maskx);
		return 3;
	}

	/*
	 * four character sequence
	 *	010000-1FFFFF => T4 Tx Tx Tx
	 */
	str[0] = T4 |  (c >> 3*Bitx);
	str[1] = Tx | ((c >> 2*Bitx) & Maskx);
	str[2] = Tx | ((c >> 1*Bitx) & Maskx);
	str[3] = Tx |  (c & Maskx);
	return 4;
}

// Returns the number of bytes required to convert r into UTF.
pub int runelen(int32_t c) {
	Rune rune = c;
	char str[10] = {};
	return runetochar(str, &rune);
}

// Unicode character classes and cases

// These routines examine and operate on Unicode characters,
// in particular a subset of their properties as defined in the Unicode standard.
// Unicode defines some characters as alphabetic and specifies three cases:
// upper, lower, and title.

// Analogously to isalpha for, these routines test types and modify cases for Unicode characters.
// The names are self-explanatory.

// The case-conversion routines return the character unchanged if it has no case.

// Returns the number of runes that are represented by the UTF string s.
pub size_t runecount(const char *s) {
	int c = 0;
	Rune rune = 0;
	size_t n = 0;
	while (true) {
		c = *(uint8_t*)s;
		if (c < Runeself) {
			if (c == 0) {
				break;
			}
			s++;
		} else {
			s += get_rune(&rune, s);
		}
		n++;
	}
	return n;
}


// copies UTF sequences until a null sequence has been copied, but writes no 
// sequences beyond es1.
// If any sequences are copied, s1
// is terminated by a null sequence, and a pointer to that sequence is returned.
// Otherwise, the original s1
// is returned.
pub char *utfecpy(char *to, char *e, char *from) {
	char *end = NULL;

	if(to >= e)
		return to;
	end = nih_memcpy(to, from, '\0', e - to);
	if (end == NULL) {
		end = e-1;
		while(end>to && (*--end&0xC0)==0x80) {}
		*end = '\0';
	}else{
		end--;
	}
	return end;
}

void *nih_memcpy(void *dest, *src, int c, size_t n) {
	uint8_t *from = (uint8_t *) src;
	uint8_t *to = (uint8_t *) dest;
	while (n > 0) {
		*to = *from;
		if ((int)*to == c) {
			return (void *) (to + 1);
		}
		n--;
	}
	return NULL;
}

/*
 * Returns the number of complete runes that are represented by the first n
 * bytes of UTF string s. If the last few bytes of the string contain an
 * incompletely coded rune, utfnlen will not count them; in this way, it differs
 * from runecount, which includes every byte of the string.
 */
pub int utfnlen(char *s, int32_t n) {
	int c = 0;
	int32_t count = 0;
	Rune rune = 0;

	char *es = s + n;
	for(count = 0; s < es; count++) {
		c = *(uint8_t*)s;
		if(c < Runeself){
			if(c == '\0')
				break;
			s++;
			continue;
		}
		if(!fullrune(s, es-s))
			break;
		s += get_rune(&rune, s);
	}
	return count;
}

// Returns a pointer to the first occurrence of rune c in the UTF-8 string s.
// Returns NULL if c does not occur in the string.
pub char *utfrune(char *s, Rune c) {
	if (c < Runesync) {
		return strchr(s, c);
	}

	Rune c1 = 0;
	Rune r = 0;
	int n = 0;

	while (true) {
		c1 = *(uint8_t*)s;
		if(c1 < Runeself) {	/* one byte rune */
			if(c1 == 0)
				return 0;
			if(c1 == c)
				return s;
			s++;
			continue;
		}
		n = get_rune(&r, s);
		if (r == c)
			return s;
		s += n;
	}
}

// Same as utfrune, but finds the last occurrence instead of first.
pub char *utfrrune(char *s, Rune c) {
	if (c < Runesync) {
		return strrchr(s, c);
	}

	Rune c1 = 0;
	Rune r = 0;
	char *s1 = NULL;
	while (true) {
		c1 = *(uint8_t*)s;
		if(c1 < Runeself) {	/* one byte rune */
			if(c1 == 0)
				return s1;
			if(c1 == c)
				s1 = s;
			s++;
			continue;
		}
		c1 = get_rune(&r, s);
		if(r == c)
			s1 = s;
		s += c1;
	}
}

// Returns a pointer to the first occurrence of s2 in s1.
// Returns NULL if s2 is null or does not contain s1.
pub char *utfutf(char *s1, char *s2) {
	Rune r = 0;
	int n1 = get_rune(&r, s2);
	if (r <= Runesync) {
		// represents self
		return strstr(s1, s2);
	}

	int f = r;
	int n2 = strlen(s2);
	char *p = s1;
	for (;;) {
		p = utfrune(p, f);
		if (!p) break;
		if (strncmp(p, s2, n2) == 0) {
			return p;
		}
		p += n1;
	}
	return NULL;
}
