int out[3] = {};
int p = 0;

int main() {
    int i = 3;
    for (;;) {
		out[p++] = i;
        i--;
        if (i == 0) {
            break;
        }
    }
	if (out[0] != 3) panic("!");
	if (out[1] != 2) panic("!");
	if (out[2] != 1) panic("!");
    return 0;
}
