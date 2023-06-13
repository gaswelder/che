#import strings

int main() {
    char *sample = newstr("%s", "   123 \r\n ");
    sample = trim(sample);
    printf("'%s'\n", sample);
}
