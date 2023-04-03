#import opt
#import strutil

int main(int argc, char *argv[]) {
    char buf[4096] = {};
    size_t size = 2;
    size_t cols = 0;

    opt.size("n", "number of columns", &size);
    opt.parse(argc, argv);

    while (fgets(buf, 4096, stdin)) {
        rtrim(buf, "\n");
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
