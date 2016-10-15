# `prng/mt`

This is a Mersenne Twister pseudorandom number generator with period
2^19937-1 modified on 2002/1/26 by Takuji Nishimura and Makoto
Matsumoto and published at
http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/MT2002/emt19937ar.html.

mt_seed(seed) initializes the state vector by using one unsigned 32-bit
integer "seed", which may be zero.

The following functions are available:

* `mt_rand32` for unsigned 32-bit integers;
* `mt_randf` for real 32-bit numbers in [0,1].

This generator is not cryptoraphically secure. You need to use a hash
function to obtain a secure random sequence.

Copyright for the original source is:
(C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura; all rights
reserved.
