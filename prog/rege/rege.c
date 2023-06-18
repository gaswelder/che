#import strings
#import lib.c

int main() {
    lib.regex_t *re = lib.init("\\s hehehe, how is it going?");

    char *text = "  hehehe, how is it going?";
    char *p = text;
    p += lib.consume(re, p);
    printf("trailing: %s\n", p);
    char *m = strings.newsubstr(text, 0, p-text);
    printf("matched: %s\n", m);
    free(m);

    
    free(re);
    return 0;
}
