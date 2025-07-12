#import formats/bencode
#import os/fs

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("arguments: <file>\n");
        return 1;
    }

    size_t size = 0;
    char *data = fs.readfile(argv[1], &size);
    if (!data) {
        fprintf(stderr, "failed to read file: %s\n", strerror(errno));
        return 1;
    }

    bencode.reader_t *r = bencode.newreader(data, size);
    while (bencode.more(r)) {
        printval(r);
    }
    bencode.freereader(r);
    return 0;
}

void printbuf(const uint8_t *buf, size_t buflen) {
    bool allprintable = true;
    for (size_t i = 0; i < buflen; i++) {
        if (!isprint(buf[i])) {
            allprintable = false;
            break;
        }
    }
    if (allprintable) {
        for (size_t i = 0; i < buflen; i++) {
            putchar(buf[i]);
        }
    } else {
        for (size_t i = 0; i < buflen; i++) {
            printf("%x", buf[i]);
            if (i > 20) {
                printf("...%zu more...", buflen - i);
                break;
            }
        }
    }
}

void printval(bencode.reader_t *r) {
    switch (bencode.type(r)) {
        case 'd': {
            printf("{\n");
            _indent++;
            bencode.enter(r);
            while (bencode.more(r)) {
                uint8_t key[100] = {};
                size_t keylen = bencode.key(r, key, 100);
                indent();
                printbuf(key, keylen);
                printf(": ");
                printval(r);
                if (bencode.more(r)) {
                    putchar(',');
                }
                printf("\n");
            }
            _indent--;
            bencode.leave(r);
            indent();
            printf("}");
        }
        case 'i': {
            printf("%d", bencode.readnum(r));
        }
        case 's': {
            int len = bencode.strsize(r);
            uint8_t *buf = calloc!(len+1, 1);
            bencode.readbuf(r, buf, len+1);
            printbuf(buf, (size_t) len);
            free(buf);
        }
        case 'l': {
            printf("[\n");
            _indent++;
            bencode.enter(r);
            int i = 0;
            while (bencode.more(r)) {
                indent();
                printf("%d: ", i++);
                printval(r);
                if (bencode.more(r)) {
                    putchar(',');
                }
                printf("\n");
            }
            _indent--;
            bencode.leave(r);
            indent();
            printf("]");
        }
        default: {
            panic("unknown type %c", bencode.type(r));
        }
    }
}

int _indent = 0;
void indent() {
    for (int i = 0; i < _indent; i++) {
        putchar('_');
		putchar(' ');
    }
}