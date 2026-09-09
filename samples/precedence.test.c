int main() {
	int x = 40;
	int l = 64;
	int r = x - (2*l-1);
	if (r != -87) {
		panic("%d != -87", r);
	}
	return 0;
}
