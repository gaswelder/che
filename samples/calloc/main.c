int main() {
	char *x = calloc!(100, 1);
	puts("ok");
	free(x);
	return 0;
}
