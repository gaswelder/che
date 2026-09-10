typedef {
	int inst;
} item_t;

int main() {
    item_t items[3] = {};
    item_t *p = items - 1;
    for (int i = 0; i < 3; i++) {
        (++p)->inst = i+1;
    }
    if (items[0].inst != 1) panic("!");
	if (items[1].inst != 2) panic("!");
	if (items[2].inst != 3) panic("!");
    return 0;
}
