
uint64_t value = 0;

pub void pcg32_seed(uint64_t s) {
    value = s;
}

uint64_t m = 0x9b60933458e17d7d;
uint64_t a = 0xd737232eeccdf7ed;

pub uint32_t pcg32() {
    value = value * m + a;
    int shift = 29 - (value >> 61);
    return value >> shift;
}