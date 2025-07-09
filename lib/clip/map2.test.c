#import map2.c
#import test
#import rnd

const char *alpha = "abcdefghijklmnopqrstuvwxyz";

char randchar() {
    return alpha[rnd.intn(strlen(alpha))];
}

int main() {
    char key[10] = {};
    char buf[100] = {};

    map2.map_t *m = map2.new();
    for (size_t i = 0; i < 1000; i++) {
        key[0] = randchar();
        key[1] = randchar();
        int val = rnd.intn(1000);

        map2.set(m, (uint8_t *) key, 2, &val, sizeof(int));
        int *r = map2.get(m, (uint8_t *) key, 2);
        if (r == NULL) panic("!");
        sprintf(buf, "%zu: %s: wanted %d, got %d\n", i, key, val, *r);
        test.truth(buf, val == *r);
    }

    map2.free(m);
    return test.fails();
}
