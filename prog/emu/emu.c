#import chip8.c
#import lc3.c

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }
    if (!strcmp(argv[1], "chip8")) {
        return chip8.main(argc - 1, argv + 1);
    }
    if (!strcmp(argv[1], "lc3")) {
        return lc3.main(argc - 1, argv + 1);
    }
    usage();
    return 1;
}

void usage() {
    fprintf(stderr, "usage: emu chip8|lc3 <...>\n");
}
