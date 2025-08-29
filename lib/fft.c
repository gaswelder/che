/*
 *  Copyright (c) 2003-2010, Mark Borgerding et al. All rights reserved.
 *  Originally KISS FFT: https://github.com/mborgerding/kissfft
KISS FFT is provided under:

  SPDX-License-Identifier: BSD-3-Clause

Being under the terms of the BSD 3-clause "New" or "Revised" License:

Valid-License-Identifier: BSD-3-Clause
SPDX-URL: https://spdx.org/licenses/BSD-3-Clause.html
Usage-Guide:
  To use the BSD 3-clause "New" or "Revised" License put the following SPDX
  tag/value pair into a comment according to the placement guidelines in
  the licensing rules documentation:
    SPDX-License-Identifier: BSD-3-Clause
License-Text:

Copyright (c) <year> <owner> . All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// The basic usage for 1-d complex FFT is:

// ```c
//     kiss_fft_cfg cfg = kiss_fft_alloc( nfft ,is_inverse_fft ,0,0 );
//     while ...
    
//         ... // put kth sample in cx_in[k].r and cx_in[k].i
        
//         kiss_fft( cfg , cx_in , cx_out );
        
//         ... // transformed. DC is in cx_out[0].r and cx_out[0].i 
        
//     free(cfg);
// ```
// NOTE: frequency-domain data is stored from dc up to 2pi.
//     so cx_out[0] is the dc bin of the FFT
//     and cx_out[nfft/2] is the Nyquist bin (if exists)
//
// Kiss FFT uses a time decimation, mixed-radix, out-of-place FFT.
// If you give it an input buffer and output buffer that are the same,
// a temporary buffer will be created to hold the data.
//
// Optimized butterflies are used for factors 2,3,4, and 5. 
//
// The real (i.e. not complex) optimization code only works for even length ffts.  It does two half-length
// FFTs in parallel (packed into real&imag), and then combines them via twiddling.  The result is 
// nfft/2+1 complex frequency bins from DC to Nyquist.  If you don't know what this means, search the web.
//
// The fast convolution filtering uses the overlap-scrap method, slightly 
// modified to put the scrap at the tail.
//
// Reducing code size remove some of the butterflies. There are currently butterflies optimized for radices
// 2,3,4,5.  It is worth mentioning that you can still use FFT sizes that contain 
// other factors, they just won't be quite as fast.  You can decide for yourself 
// whether to keep radix 2 or 4.  If you do some work in this area, let me 
// know what you find.




/* e.g. an fft of length 128 has 4 factors
 as far as kissfft is concerned
 4*4*4*2
 */
#define MAXFACTORS 32

const double PI = 3.141592653589793238462643383279502884197169399375105820974944;

pub typedef { float r, i; } kiss_fft_cpx;

void kf_cexp(kiss_fft_cpx *x, float phase) {
    (x)->r = cos(phase);
    (x)->i = sin(phase);
}

// C_MUL(m,a,b)         : m = a*b
void C_MUL(kiss_fft_cpx *m, a, b) {
    m->r = a.r*b.r - a.i*b.i;
    m->i = a.r*b.i + a.i*b.r;
}

void C_ADD(kiss_fft_cpx *res, a,b) {
    res->r = a.r + b.r;
    res->i = a.i + b.i;
}

// C_SUB( res, a,b)     : res = a - b
void C_SUB(kiss_fft_cpx *res, a,b) {
    res->r = a.r - b.r;
    res->i = a.i - b.i;
}

// C_ADDTO( res , a)    : res += a
void C_ADDTO(kiss_fft_cpx *res, a) {
    res->r += a.r;
    res->i += a.i;
}

void C_MULBYSCALAR(kiss_fft_cpx *c, float s) {
    c->r *= s;
    c->i *= s;
}

pub typedef {
    kiss_fft_state_t *substate;
    kiss_fft_cpx * tmpbuf;
    kiss_fft_cpx * super_twiddles;
} kiss_fftr_state_t;


/*
* For platforms where ROM/code space is more plentiful than RAM,
     consider creating a hardcoded kiss_fft_state. In other words, decide which
     FFT size(s) you want and make a structure with the correct factors and twiddles.
*/
pub typedef {
    int nfft;
    int inverse;
    int factors[2*MAXFACTORS];
    kiss_fft_cpx twiddles[1];
} kiss_fft_state_t;

pub typedef {
    int dimReal;
    int dimOther;
    kiss_fftr_state_t *cfg_r;
    kiss_fftnd_state_t *cfg_nd;
    void * tmpbuf;
} kiss_fftndr_state_t;

pub typedef {
    int dimprod; /* dimsum would be mighty tasty right now */
    int ndims;
    int *dims;
    kiss_fft_state_t **states; /* cfg states for each dimension */
    kiss_fft_cpx * tmpbuf; /*buffer capable of hold the entire input */
} kiss_fftnd_state_t;

float S_MUL(float a, b) {
    return a * b;
}

// /* for real ffts, we need an even size */
// int kiss_fftr_next_fast_size_real(int n) {
//     return kiss_fft_next_fast_size( ((n)+1)>>1)<<1;
// }

void kf_bfly2(kiss_fft_cpx *Fout, size_t fstride, kiss_fft_state_t *st, int m) {
    kiss_fft_cpx *tw1 = st->twiddles;
    kiss_fft_cpx *Fout2 = Fout + m;
    kiss_fft_cpx t = {};
    while (true) {
        C_MUL(&t, *Fout2, *tw1);
        tw1 += fstride;
        C_SUB(Fout2, *Fout, t);
        C_ADDTO(Fout, t);
        ++Fout2;
        ++Fout;
        m--;
        if (!m) break;
    }
}

void kf_bfly4(kiss_fft_cpx *Fout, size_t fstride, kiss_fft_state_t *st, size_t m) {
    size_t k = m;
    size_t m2 = 2*m;
    size_t m3 = 3*m;

    kiss_fft_cpx *tw1 = st->twiddles;
    kiss_fft_cpx *tw2 = st->twiddles;
    kiss_fft_cpx *tw3 = st->twiddles;
    kiss_fft_cpx scratch[6];

    while (true) {
        C_MUL(&scratch[0], Fout[m], *tw1 );
        C_MUL(&scratch[1], Fout[m2], *tw2 );
        C_MUL(&scratch[2], Fout[m3], *tw3 );

        C_SUB(&scratch[5] , *Fout, scratch[1] );
        C_ADDTO(Fout, scratch[1]);
        C_ADD(&scratch[3] , scratch[0] , scratch[2] );
        C_SUB(&scratch[4] , scratch[0] , scratch[2] );
        C_SUB(&Fout[m2], *Fout, scratch[3] );
        tw1 += fstride;
        tw2 += fstride*2;
        tw3 += fstride*3;
        C_ADDTO(Fout, scratch[3] );

        if(st->inverse) {
            Fout[m].r = scratch[5].r - scratch[4].i;
            Fout[m].i = scratch[5].i + scratch[4].r;
            Fout[m3].r = scratch[5].r + scratch[4].i;
            Fout[m3].i = scratch[5].i - scratch[4].r;
        }else{
            Fout[m].r = scratch[5].r + scratch[4].i;
            Fout[m].i = scratch[5].i - scratch[4].r;
            Fout[m3].r = scratch[5].r - scratch[4].i;
            Fout[m3].i = scratch[5].i + scratch[4].r;
        }
        ++Fout;
        k--;
        if (!k) break;
    }
}

void kf_bfly3(kiss_fft_cpx * Fout, size_t fstride, kiss_fft_state_t *st, size_t m) {
     size_t k = m;
     size_t m2 = 2*m;
     kiss_fft_cpx scratch[5];
     kiss_fft_cpx epi3 = st->twiddles[fstride*m];
     kiss_fft_cpx *tw1 = st->twiddles;
     kiss_fft_cpx *tw2 = st->twiddles;

     while (true) {
         C_MUL(&scratch[1],Fout[m] , *tw1);
         C_MUL(&scratch[2],Fout[m2] , *tw2);

         C_ADD(&scratch[3],scratch[1],scratch[2]);
         C_SUB(&scratch[0],scratch[1],scratch[2]);
         tw1 += fstride;
         tw2 += fstride*2;

         Fout[m].r = Fout->r - 0.5 * (scratch[3].r);
         Fout[m].i = Fout->i - 0.5 * (scratch[3].i);

         C_MULBYSCALAR(&scratch[0] , epi3.i);

         C_ADDTO(Fout, scratch[3]);

         Fout[m2].r = Fout[m].r + scratch[0].i;
         Fout[m2].i = Fout[m].i - scratch[0].r;

         Fout[m].r -= scratch[0].i;
         Fout[m].i += scratch[0].r;

         ++Fout;
         k--;
         if (!k) break;
     }
}

void kf_bfly5(
        kiss_fft_cpx * Fout,
        size_t fstride,
        kiss_fft_state_t *st,
        int m
        )
{
    kiss_fft_cpx *Fout0;
    kiss_fft_cpx *Fout1;
    kiss_fft_cpx *Fout2;
    kiss_fft_cpx *Fout3;
    kiss_fft_cpx *Fout4;
    int u;
    kiss_fft_cpx scratch[13];
    kiss_fft_cpx * twiddles = st->twiddles;
    kiss_fft_cpx *tw;
    kiss_fft_cpx ya;
    kiss_fft_cpx yb;
    ya = twiddles[fstride*m];
    yb = twiddles[fstride*2*m];

    Fout0=Fout;
    Fout1=Fout0+m;
    Fout2=Fout0+2*m;
    Fout3=Fout0+3*m;
    Fout4=Fout0+4*m;

    tw=st->twiddles;
    for ( u=0; u<m; ++u ) {
        scratch[0] = *Fout0;

        C_MUL(&scratch[1] ,*Fout1, tw[u*fstride]);
        C_MUL(&scratch[2] ,*Fout2, tw[2*u*fstride]);
        C_MUL(&scratch[3] ,*Fout3, tw[3*u*fstride]);
        C_MUL(&scratch[4] ,*Fout4, tw[4*u*fstride]);

        C_ADD(&scratch[7],scratch[1],scratch[4]);
        C_SUB(&scratch[10],scratch[1],scratch[4]);
        C_ADD(&scratch[8],scratch[2],scratch[3]);
        C_SUB(&scratch[9],scratch[2],scratch[3]);

        Fout0->r += scratch[7].r + scratch[8].r;
        Fout0->i += scratch[7].i + scratch[8].i;

        scratch[5].r = scratch[0].r + S_MUL(scratch[7].r,ya.r) + S_MUL(scratch[8].r,yb.r);
        scratch[5].i = scratch[0].i + S_MUL(scratch[7].i,ya.r) + S_MUL(scratch[8].i,yb.r);

        scratch[6].r =  S_MUL(scratch[10].i,ya.i) + S_MUL(scratch[9].i,yb.i);
        scratch[6].i = -S_MUL(scratch[10].r,ya.i) - S_MUL(scratch[9].r,yb.i);

        C_SUB(Fout1, scratch[5],scratch[6]);
        C_ADD(Fout4, scratch[5],scratch[6]);

        scratch[11].r = scratch[0].r + S_MUL(scratch[7].r,yb.r) + S_MUL(scratch[8].r,ya.r);
        scratch[11].i = scratch[0].i + S_MUL(scratch[7].i,yb.r) + S_MUL(scratch[8].i,ya.r);
        scratch[12].r = - S_MUL(scratch[10].i,yb.i) + S_MUL(scratch[9].i,ya.i);
        scratch[12].i = S_MUL(scratch[10].r,yb.i) - S_MUL(scratch[9].r,ya.i);

        C_ADD(Fout2, scratch[11],scratch[12]);
        C_SUB(Fout3, scratch[11],scratch[12]);

        ++Fout0;
        ++Fout1;
        ++Fout2;
        ++Fout3;
        ++Fout4;
    }
}

/* perform the butterfly for one stage of a mixed radix FFT */
void kf_bfly_generic(
        kiss_fft_cpx * Fout,
        size_t fstride,
        kiss_fft_state_t *st,
        int m,
        int p
        )
{
    int u;
    int k;
    int q1;
    int q;
    kiss_fft_cpx * twiddles = st->twiddles;
    kiss_fft_cpx t;
    int Norig = st->nfft;

    kiss_fft_cpx *scratch = calloc!(1, sizeof(kiss_fft_cpx)*p);

    for ( u=0; u<m; ++u ) {
        k=u;
        for ( q1=0 ; q1<p ; ++q1 ) {
            scratch[q1] = Fout[ k  ];
            k += m;
        }

        k=u;
        for ( q1=0 ; q1<p ; ++q1 ) {
            int twidx=0;
            Fout[ k ] = scratch[0];
            for (q=1;q<p;++q ) {
                twidx += fstride * k;
                if (twidx>=Norig) twidx-=Norig;
                C_MUL(&t, scratch[q] , twiddles[twidx] );
                C_ADDTO(&Fout[k], t);
            }
            k += m;
        }
    }
    free(scratch);
}

void kf_work(
        kiss_fft_cpx * Fout,
        kiss_fft_cpx * f,
        size_t fstride,
        int in_stride,
        int * factors,
        kiss_fft_state_t *st)
{
    kiss_fft_cpx * Fout_beg=Fout;
    int p=*factors++; /* the radix  */
    int m=*factors++; /* stage's fft length/p */
    kiss_fft_cpx *Fout_end = Fout + p*m;


    if (m==1) {
        while (true) {
            *Fout = *f;
            f += fstride * in_stride;
            Fout++;
            if (Fout == Fout_end) break;
        }
    }else{
        while (true) {
            // recursive call:
            // DFT of size m*p performed by doing
            // p instances of smaller DFTs of size m,
            // each one takes a decimated version of the input
            kf_work( Fout , f, fstride*p, in_stride, factors,st);
            f += fstride*in_stride;
            Fout += m;
            if (Fout == Fout_end) break;
        }
    }

    Fout=Fout_beg;

    // recombine the p smaller DFTs
    switch (p) {
        case 2: { kf_bfly2(Fout,fstride,st,m); }
        case 3: { kf_bfly3(Fout,fstride,st,m); }
        case 4: { kf_bfly4(Fout,fstride,st,m); }
        case 5: { kf_bfly5(Fout,fstride,st,m); }
        default: { kf_bfly_generic(Fout,fstride,st,m,p); }
    }
}

/*  facbuf is populated by p1,m1,p2,m2, ...
    where
    p[i] * m[i] = m[i-1]
    m0 = n                  */
void kf_factor(int n,int * facbuf) {
    int p=4;
    double floor_sqrt = floor( sqrt((double)n) );

    /*factor out powers of 4, powers of 2, then any remaining primes */
    while (true) {
        while (n % p) {
            switch (p) {
                case 4: { p = 2; }
                case 2: { p = 3; }
                default: { p += 2; }
            }
            if ((double) p > floor_sqrt) {
                /* no more factors, skip to end */
                p = n;
            }
        }
        n /= p;
        *facbuf++ = p;
        *facbuf++ = n;
        if (n <= 1) break;
    }
}

/*
 *  Initialize a FFT (or IFFT) algorithm's cfg/state buffer.
 *
 *  typical usage:      kiss_fft_state_t *mycfg=kiss_fft_alloc(1024,0,NULL,NULL);
 *
 *  The return value from fft_alloc is a cfg buffer used internally
 *  by the fft routine or NULL.
 *
 *  If lenmem is NULL, then kiss_fft_alloc will allocate a cfg buffer using malloc.
 *  The returned value should be free()d when done to avoid memory leaks.
 *
 *  The state can be placed in a user supplied buffer 'mem':
 *  If lenmem is not NULL and mem is not NULL and *lenmem is large enough,
 *      then the function places the cfg in mem and the size used in *lenmem
 *      and returns mem.
 *
 *  If lenmem is not NULL and ( mem is NULL or *lenmem is not large enough),
 *      then the function returns NULL and places the minimum cfg
 *      buffer size in *lenmem.
 *
*
 * The return value is a contiguous block of memory, allocated with malloc,
 it can be freed with free().
 * */
pub kiss_fft_state_t *kiss_fft_alloc(int nfft,int inverse_fft,void * mem,size_t * lenmem ) {
    kiss_fft_state_t *st=NULL;
    size_t memneeded = (sizeof(kiss_fft_state_t) + sizeof(kiss_fft_cpx)*(nfft-1)); /* twiddle factors*/

    if (lenmem == NULL) {
        st = calloc!(memneeded, 1);
    } else{
        if (mem != NULL && *lenmem >= memneeded)
            st = mem;
        *lenmem = memneeded;
    }
    if (st) {
        int i;
        st->nfft=nfft;
        st->inverse = inverse_fft;

        for (i=0;i<nfft;++i) {
            double phase = -2 * PI * i / nfft;
            if (st->inverse)
                phase *= -1;
            kf_cexp(st->twiddles+i, phase );
        }

        kf_factor(nfft,st->factors);
    }
    return st;
}

/*
 A more generic version of the above function. It reads its input from every Nth sample.
 * */
void kiss_fft_stride(kiss_fft_state_t *st, kiss_fft_cpx *fin,kiss_fft_cpx *fout,int in_stride)
{
    if (fin == fout) {
        //NOTE: this is not really an in-place FFT algorithm.
        //It just performs an out-of-place FFT into a temp buffer
        if (fout == NULL) {
            panic("fout buffer NULL.");
        }

        kiss_fft_cpx *tmpbuf = calloc!(1, sizeof(kiss_fft_cpx)*st->nfft);
        kf_work(tmpbuf,fin,1,in_stride, st->factors,st);
        memcpy(fout,tmpbuf,sizeof(kiss_fft_cpx)*st->nfft);
        free(tmpbuf);
    }else{
        kf_work( fout, fin, 1,in_stride, st->factors,st );
    }
}

/*
 * kiss_fft(cfg,in_out_buf)
 *
 * Perform an FFT on a complex input buffer.
 * for a forward FFT,
 * fin should be  f[0] , f[1] , ... ,f[nfft-1]
 * fout will be   F[0] , F[1] , ... ,F[nfft-1]
 * Note that each element is complex and can be accessed like
    f[k].r and f[k].i
 * */
pub void kiss_fft(kiss_fft_state_t *cfg, kiss_fft_cpx *fin, kiss_fft_cpx *fout) {
    kiss_fft_stride(cfg,fin,fout,1);
}

// /*
//  * Returns the smallest integer k, such that k>=n and k has only "fast" factors (2,3,5)
//  */
// int kiss_fft_next_fast_size(int n) {
//     while (true) {
//         int m=n;
//         while ( (m%2) == 0 ) m/=2;
//         while ( (m%3) == 0 ) m/=3;
//         while ( (m%5) == 0 ) m/=5;
//         if (m<=1)
//             break; /* n is completely factorable by twos, threes, and fives */
//         n++;
//     }
//     return n;
// }

/*
 nfft must be even

 If you don't care to allocate space, use mem = lenmem = NULL
*/
kiss_fftr_state_t *kiss_fftr_alloc(int nfft,int inverse_fft,void * mem,size_t * lenmem)
{
    if (nfft & 1) {
        panic("Real FFT optimization must be even.");
        return NULL;
    }

    int i;
    kiss_fftr_state_t *st = NULL;
    size_t subsize = 0;
    
    nfft = nfft >> 1;

    kiss_fft_alloc (nfft, inverse_fft, NULL, &subsize);
    size_t memneeded = sizeof(kiss_fftr_state_t) + subsize + sizeof(kiss_fft_cpx) * ( nfft * 3 / 2);

    if (lenmem == NULL) {
        st = calloc!(memneeded, 1);
    } else {
        if (*lenmem >= memneeded) {
            st = mem;
        }
        *lenmem = memneeded;
    }
    if (!st)
        return NULL;

    st->substate = (kiss_fft_state_t *) (st + 1); /*just beyond kiss_fftr_state_t */
    st->tmpbuf = (kiss_fft_cpx *) (((char *) st->substate) + subsize);
    st->super_twiddles = st->tmpbuf + nfft;
    kiss_fft_alloc(nfft, inverse_fft, st->substate, &subsize);

    for (i = 0; i < nfft/2; ++i) {
        double phase = -PI * ((double) (i+1) / nfft + 0.5);
        if (inverse_fft) phase *= -1;
        kf_cexp (st->super_twiddles+i,phase);
    }
    return st;
}

/*
 input timedata has nfft scalar points
 output freqdata has nfft/2+1 complex points
*/
void kiss_fftr(kiss_fftr_state_t *st, kiss_fft_cpx *timedata, kiss_fft_cpx *freqdata)
{
    /* input buffer timedata is stored row-wise */
    int k;
    int ncfft;
    kiss_fft_cpx fpnk;
    kiss_fft_cpx fpk;
    kiss_fft_cpx f1k;
    kiss_fft_cpx f2k;
    kiss_fft_cpx tw;
    kiss_fft_cpx tdc;

    if ( st->substate->inverse) {
        panic("kiss fft usage error: improper alloc");
        return;/* The caller did not call the correct function */
    }

    ncfft = st->substate->nfft;

    /*perform the parallel fft of two real signals packed in real,imag*/
    kiss_fft(st->substate, timedata, st->tmpbuf );
    /* The real part of the DC element of the frequency spectrum in st->tmpbuf
     * contains the sum of the even-numbered elements of the input time sequence
     * The imag part is the sum of the odd-numbered elements
     *
     * The sum of tdc.r and tdc.i is the sum of the input time sequence.
     *      yielding DC of input time sequence
     * The difference of tdc.r - tdc.i is the sum of the input (dot product) [1,-1,1,-1...
     *      yielding Nyquist bin of input time sequence
     */

    tdc.r = st->tmpbuf[0].r;
    tdc.i = st->tmpbuf[0].i;
    freqdata[0].r = tdc.r + tdc.i;
    freqdata[ncfft].r = tdc.r - tdc.i;
    freqdata[ncfft].i = freqdata[0].i = 0;

    for ( k=1;k <= ncfft/2 ; ++k ) {
        fpk    = st->tmpbuf[k];
        fpnk.r =   st->tmpbuf[ncfft-k].r;
        fpnk.i = - st->tmpbuf[ncfft-k].i;

        C_ADD(&f1k, fpk , fpnk );
        C_SUB(&f2k, fpk , fpnk );
        C_MUL(&tw , f2k , st->super_twiddles[k-1]);

        freqdata[k].r = 0.5 * (f1k.r + tw.r);
        freqdata[k].i = 0.5 * (f1k.i + tw.i);
        freqdata[ncfft-k].r = 0.5 * (f1k.r - tw.r);
        freqdata[ncfft-k].i = 0.5 * (tw.i - f1k.i);
    }
}

/*
 input freqdata has  nfft/2+1 complex points
 output timedata has nfft scalar points
*/
void kiss_fftri(kiss_fftr_state_t *st, kiss_fft_cpx *freqdata, float *timedata)
{
    if (st->substate->inverse == 0) {
        /* The caller did not call the correct function */
        panic("kiss fft usage error: improper alloc");
    }

    /* input buffer timedata is stored row-wise */
    int ncfft = st->substate->nfft;

    st->tmpbuf[0].r = freqdata[0].r + freqdata[ncfft].r;
    st->tmpbuf[0].i = freqdata[0].r - freqdata[ncfft].r;

    int k;
    for (k = 1; k <= ncfft / 2; ++k) {
        kiss_fft_cpx fk;
        kiss_fft_cpx fnkc;
        kiss_fft_cpx fek;
        kiss_fft_cpx fok;
        kiss_fft_cpx tmp;
        fk = freqdata[k];
        fnkc.r = freqdata[ncfft - k].r;
        fnkc.i = -freqdata[ncfft - k].i;

        C_ADD(&fek, fk, fnkc);
        C_SUB(&tmp, fk, fnkc);
        C_MUL(&fok, tmp, st->super_twiddles[k-1]);
        C_ADD(&st->tmpbuf[k],     fek, fok);
        C_SUB(&st->tmpbuf[ncfft - k], fek, fok);
        st->tmpbuf[ncfft - k].i *= -1;
    }
    kiss_fft (st->substate, st->tmpbuf, (kiss_fft_cpx *) timedata);
}







int prod(int *dims, int n) {
    int x = 1;
    while (n--) {
        x *= *dims++;
    }
    return x;
}

size_t MAX(size_t a, b) {
    if (a > b) return a;
    return b;
}

pub kiss_fftndr_state_t *kiss_fftndr_alloc(int *dims, int ndims, int inverse_fft, void*mem, size_t*lenmem) {
    kiss_fftndr_state_t * st = NULL;
    size_t nr=0;
    size_t nd=0;
    size_t ntmp=0;
    int dimReal = dims[ndims-1];
    int dimOther = prod(dims,ndims-1);
    char * ptr = NULL;

    kiss_fftr_alloc(dimReal,inverse_fft,NULL,&nr);
    (void)kiss_fftnd_alloc(dims,ndims-1,inverse_fft,NULL,&nd);
    ntmp =
        MAX( 2*dimOther , dimReal+2) * sizeof(float)  // freq buffer for one pass
        + dimOther*(dimReal+2) * sizeof(float);  // large enough to hold entire input in case of in-place

    size_t memneeded = sizeof(kiss_fftndr_state_t) + nr + nd + ntmp;

    if (lenmem==NULL) {
        ptr = calloc!(memneeded, 1);
    }else{
        if (*lenmem >= memneeded)
            ptr = mem;
        *lenmem = memneeded;
    }
    if (ptr==NULL) return NULL;

    st = (void *) ptr;
    memset( st , 0 , memneeded);
    ptr += sizeof(kiss_fftndr_state_t);

    st->dimReal = dimReal;
    st->dimOther = dimOther;
    st->cfg_r = kiss_fftr_alloc( dimReal,inverse_fft,ptr,&nr);
    ptr += nr;
    st->cfg_nd = kiss_fftnd_alloc(dims,ndims-1,inverse_fft, ptr,&nd);
    ptr += nd;
    st->tmpbuf = ptr;

    return st;
}

/*
 input timedata has dims[0] X dims[1] X ... X  dims[ndims-1] scalar points
 output freqdata has dims[0] X dims[1] X ... X  dims[ndims-1]/2+1 complex points
*/
pub void kiss_fftndr(kiss_fftndr_state_t * st, kiss_fft_cpx *timedata, kiss_fft_cpx *freqdata)
{
    int k1;
    int k2;
    int dimReal = st->dimReal;
    int dimOther = st->dimOther;
    int nrbins = dimReal/2+1;

    kiss_fft_cpx * tmp1 = (kiss_fft_cpx*)st->tmpbuf;
    kiss_fft_cpx * tmp2 = tmp1 + MAX(nrbins,dimOther);

    // timedata is N0 x N1 x ... x Nk real

    // take a real chunk of data, fft it and place the output at correct intervals
    for (k1=0;k1<dimOther;++k1) {
        kiss_fftr( st->cfg_r, timedata + k1*dimReal , tmp1 ); // tmp1 now holds nrbins complex points
        for (k2=0;k2<nrbins;++k2)
           tmp2[ k2*dimOther+k1 ] = tmp1[k2];
    }

    for (k2=0;k2<nrbins;++k2) {
        kiss_fftnd(st->cfg_nd, tmp2+k2*dimOther, tmp1);  // tmp1 now holds dimOther complex points
        for (k1=0;k1<dimOther;++k1)
            freqdata[ k1*(nrbins) + k2] = tmp1[k1];
    }
}

/*
 input and output dimensions are the exact opposite of kiss_fftndr
*/
pub void kiss_fftndri(kiss_fftndr_state_t * st, kiss_fft_cpx *freqdata,float *timedata)
{
    int k1;
    int k2;
    int dimReal = st->dimReal;
    int dimOther = st->dimOther;
    int nrbins = dimReal/2+1;
    kiss_fft_cpx * tmp1 = (kiss_fft_cpx*)st->tmpbuf;
    kiss_fft_cpx * tmp2 = tmp1 + MAX(nrbins,dimOther);

    for (k2=0;k2<nrbins;++k2) {
        for (k1=0;k1<dimOther;++k1)
            tmp1[k1] = freqdata[ k1*(nrbins) + k2 ];
        kiss_fftnd(st->cfg_nd, tmp1, tmp2+k2*dimOther);
    }

    for (k1=0;k1<dimOther;++k1) {
        for (k2=0;k2<nrbins;++k2)
            tmp1[k2] = tmp2[ k2*dimOther+k1 ];
        kiss_fftri( st->cfg_r,tmp1,timedata + k1*dimReal);
    }
}

kiss_fftnd_state_t *kiss_fftnd_alloc(int *dims, int ndims, int inverse_fft, void *mem, size_t*lenmem) {
    kiss_fftnd_state_t *st = NULL;
    int i;
    int dimprod=1;
    size_t memneeded = sizeof(kiss_fftnd_state_t);
    char * ptr = NULL;

    for (i=0;i<ndims;++i) {
        size_t sublen=0;
        kiss_fft_alloc (dims[i], inverse_fft, NULL, &sublen);
        memneeded += sublen;   /* st->states[i] */
        dimprod *= dims[i];
    }
    memneeded += (sizeof(int) * ndims);/*  st->dims */
    memneeded += (sizeof(void*) * ndims);/* st->states  */
    memneeded += (sizeof(kiss_fft_cpx) * dimprod); /* st->tmpbuf */

    if (lenmem == NULL) {/* allocate for the caller*/
        ptr = calloc!(memneeded, 1);
    } else { /* initialize supplied buffer if big enough */
        if (*lenmem >= memneeded)
            ptr = (char *) mem;
        *lenmem = memneeded; /*tell caller how big struct is (or would be) */
    }
    if (!ptr)
        return NULL; /*malloc failed or buffer too small */

    st = (void *) ptr;
    st->dimprod = dimprod;
    st->ndims = ndims;
    ptr += sizeof(kiss_fftnd_state_t);

    st->states = (kiss_fft_state_t **)ptr;
    ptr += sizeof(void*) * ndims;

    st->dims = (void *) ptr;
    ptr += sizeof(int) * ndims;

    st->tmpbuf = (kiss_fft_cpx*)ptr;
    ptr += sizeof(kiss_fft_cpx) * dimprod;

    for (i=0;i<ndims;++i) {
        size_t len;
        st->dims[i] = dims[i];
        kiss_fft_alloc (st->dims[i], inverse_fft, NULL, &len);
        st->states[i] = kiss_fft_alloc (st->dims[i], inverse_fft, ptr,&len);
        ptr += len;
    }
    /*
Hi there!

If you're looking at this particular code, it probably means you've got a brain-dead bounds checker
that thinks the above code overwrites the end of the array.

It doesn't.

-- Mark

P.S.
The below code might give you some warm fuzzies and help convince you.
       */
    if ( ptr - (char*)st != (int)memneeded ) {
        fprintf(stderr,
                "################################################################################\n"
                "Internal error! Memory allocation miscalculation\n"
                "################################################################################\n"
               );
    }
    return st;
}

/*
 This works by tackling one dimension at a time.

 In effect,
 Each stage starts out by reshaping the matrix into a DixSi 2d matrix.
 A Di-sized fft is taken of each column, transposing the matrix as it goes.

Here's a 3-d example:
Take a 2x3x4 matrix, laid out in memory as a contiguous buffer
 [ [ [ a b c d ] [ e f g h ] [ i j k l ] ]
   [ [ m n o p ] [ q r s t ] [ u v w x ] ] ]

Stage 0 ( D=2): treat the buffer as a 2x12 matrix
   [ [a b ... k l]
     [m n ... w x] ]

   FFT each column with size 2.
   Transpose the matrix at the same time using kiss_fft_stride.

   [ [ a+m a-m ]
     [ b+n b-n]
     ...
     [ k+w k-w ]
     [ l+x l-x ] ]

   Note fft([x y]) == [x+y x-y]

Stage 1 ( D=3) treats the buffer (the output of stage D=2) as an 3x8 matrix,
   [ [ a+m a-m b+n b-n c+o c-o d+p d-p ]
     [ e+q e-q f+r f-r g+s g-s h+t h-t ]
     [ i+u i-u j+v j-v k+w k-w l+x l-x ] ]

   And perform FFTs (size=3) on each of the columns as above, transposing
   the matrix as it goes.  The output of stage 1 is
       (Legend: ap = [ a+m e+q i+u ]
                am = [ a-m e-q i-u ] )

   [ [ sum(ap) fft(ap)[0] fft(ap)[1] ]
     [ sum(am) fft(am)[0] fft(am)[1] ]
     [ sum(bp) fft(bp)[0] fft(bp)[1] ]
     [ sum(bm) fft(bm)[0] fft(bm)[1] ]
     [ sum(cp) fft(cp)[0] fft(cp)[1] ]
     [ sum(cm) fft(cm)[0] fft(cm)[1] ]
     [ sum(dp) fft(dp)[0] fft(dp)[1] ]
     [ sum(dm) fft(dm)[0] fft(dm)[1] ]  ]

Stage 2 ( D=4) treats this buffer as a 4*6 matrix,
   [ [ sum(ap) fft(ap)[0] fft(ap)[1] sum(am) fft(am)[0] fft(am)[1] ]
     [ sum(bp) fft(bp)[0] fft(bp)[1] sum(bm) fft(bm)[0] fft(bm)[1] ]
     [ sum(cp) fft(cp)[0] fft(cp)[1] sum(cm) fft(cm)[0] fft(cm)[1] ]
     [ sum(dp) fft(dp)[0] fft(dp)[1] sum(dm) fft(dm)[0] fft(dm)[1] ]  ]

   Then FFTs each column, transposing as it goes.

   The resulting matrix is the 3d FFT of the 2x3x4 input matrix.

   Note as a sanity check that the first element of the final
   stage's output (DC term) is
   sum( [ sum(ap) sum(bp) sum(cp) sum(dp) ] )
   , i.e. the summation of all 24 input elements.
*/
void kiss_fftnd(kiss_fftnd_state_t *st, kiss_fft_cpx *fin,kiss_fft_cpx *fout)
{
    int i;
    int k;
    kiss_fft_cpx *bufin = fin;
    kiss_fft_cpx *bufout;

    /*arrange it so the last bufout == fout*/
    if ( st->ndims & 1 ) {
        bufout = fout;
        if (fin==fout) {
            memcpy( st->tmpbuf, fin, sizeof(kiss_fft_cpx) * st->dimprod );
            bufin = st->tmpbuf;
        }
    } else {
        bufout = st->tmpbuf;
    }

    for ( k=0; k < st->ndims; ++k) {
        int curdim = st->dims[k];
        int stride = st->dimprod / curdim;

        for ( i=0 ; i<stride ; ++i )
            kiss_fft_stride( st->states[k], bufin+i , bufout+i*curdim, stride );

        /*toggle back and forth between the two buffers*/
        if (bufout == st->tmpbuf){
            bufout = fout;
            bufin = st->tmpbuf;
        }else{
            bufout = st->tmpbuf;
            bufin = fout;
        }
    }
}
