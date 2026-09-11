int main(int argc, char *argv[]) {
	(void) argv;
	if (argc == 1) {
		int panic = argc;
		if (panic != 1) panic("!");
	} else {
		panic("!");
	}
	return 0;
}
