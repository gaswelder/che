int main(int argc, char *argv[]) {
	(void) argv;
	if (argc == 1) {
		int panic = argc;
		printf("%d\n", panic);
	} else {
		panic("!");
	}
	return 0;
}
