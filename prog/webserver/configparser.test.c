#import configparser.c
#import server.c

int main() {
    server.LKHostConfig *configs[100] = {NULL};
    size_t n = configparser.read_file("prog/webserver/lktest.conf", configs);
    if (!n) {
        panic("nope");
    }
    printf("size = %zu\n", n);
    for (size_t i = 0; i < n; i++) {
        server.print_config(configs[i]);
    }
    printf("\n");
    return 0;
}
