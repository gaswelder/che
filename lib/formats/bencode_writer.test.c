#import test
#import bencode_writer.c

int main() {
    uint8_t buf[100];

    bencode_writer.t *bw = bencode_writer.tobuf(buf, sizeof(buf));
    if (!bw) panic("!bw");

    test.truth("1", bencode_writer.begin(bw, 'd'));
    test.truth("2", bencode_writer.buf(bw, (uint8_t*) "foo", 3));
    test.truth("3", bencode_writer.num(bw, 12345678));
    test.truth("4", bencode_writer.end(bw));
    test.truth("memcmp", memcmp(buf, "d3:fooi12345678ee", bencode_writer.pos(bw)) == 0);

    bencode_writer.freewriter(bw);

    return test.fails();
}
