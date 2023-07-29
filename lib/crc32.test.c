#import crc32.c
#import test

int main() {
    uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};

    uint32_t r = crc32.init();
    r = crc32.crc32(r, data, sizeof(data));

    test.truth("123456789 -> 0xCBF43926", r == 0xCBF43926);
    return test.fails();
}
