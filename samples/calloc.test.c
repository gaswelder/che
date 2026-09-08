int main() {
	char *x = calloc!(100, 1);
	free(x);
	return 0;
}
