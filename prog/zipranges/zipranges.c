int main() {
    int val = 0;
    while (true) {
		int r = scanf("%d\n", &val);
		if (r == EOF) {
			break;
		}
		if (r != 1) {
			fprintf(stderr, "failed to parse a number\n");
			return 1;
		}
		append(val);
	}
    out();
}

typedef { int a, b; } range_t;

range_t cells[4000] = {};
int pos = -1;

void append(int val) {
    if (pos == -1) {
        pos = 0;
        cells[pos].a = val;
        cells[pos].b = val;
    } else if (val == cells[pos].b + 1) {
        cells[pos].b++;
    } else {
        pos++;
        cells[pos].a = val;
        cells[pos].b = val;
    }
}

void out() {
    for (int i = 0; i <= pos; i++) {
        printf("%d-%d ", cells[i].a, cells[i].b);
    }
    puts("");
}
