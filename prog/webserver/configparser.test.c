#import configparser.c
#import config.c

int main() {
    config.LKConfig *cfg = configparser.read_file("prog/webserver/lktest.conf");
    if (!cfg) {
        panic("nope");
    }
    printf("size = %zu\n", cfg->hostconfigs_size);
    config.print(cfg);
    return 0;
}
