#import opt
#import strings

int main(int argc, char *argv[]) {
    char buf[4096] = {};
    size_t size = 2;
    size_t cols = 0;

    opt.opt_size("n", "number of columns", &size);
    opt.opt_parse(argc, argv);

    while (fgets(buf, 4096, stdin)) {
        strings.rtrim(buf, "\n");
        if (cols > 0) {
            putchar('\t');
        }
        printf("%s", buf);
        cols++;
        if (cols == size) {
            putchar('\n');
            cols = 0;
        }
    }
    if (cols > 0 && cols < size) {
        putchar('\n');
    }
}
