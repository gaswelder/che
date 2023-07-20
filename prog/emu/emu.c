#import chip8.c
#import lc3.c
#import strings

int main(int argc, char *argv[]) {
    if (argc != 3) {
        usage();
        return 1;
    }
    if (!strcmp(argv[1], "run")) {
        return run(argv[2]);
    }
    if (!strcmp(argv[1], "disas")) {
        return disas(argv[2]);
    }
    usage();
    return 1;
}

void usage() {
    fprintf(stderr, "usage: emu run|disas <rom-file>\n");
}

int run(char *rompath) {
    if (strings.ends_with(rompath, ".lc3")) {
        return lc3.run(rompath);
    }
    if (strings.ends_with(rompath, ".chip8")) {
        return chip8.run(rompath);
    }
    fprintf(stderr, "unrecognized ROM for run\n");
    return 1;
}

int disas(char *rompath) {
    if (strings.ends_with(rompath, ".chip8")) {
        return chip8.disas(rompath);
    }
    fprintf(stderr, "unrecognized ROM for disassembly\n");
    return 1;
}
