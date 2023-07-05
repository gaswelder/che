typedef {
	int inst;
} item_t;

int main() {
    item_t items[3] = {};
    item_t *p = items - 1;
    for (int i = 0; i < 3; i++) {
        (++p)->inst = i+1;
    }
    for (int i = 0; i < 3; i++) {
        printf("%d\n", items[i].inst);
    }
    return 0;
}
