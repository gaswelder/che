int main() {
    int i = 3;

    // All three for components are optional.
    for (;;) {
        printf("%d\n", i);
        i--;
        if (i == 0) {
            break;
        }
    }
    return 0;
}
