int main() {
	if (min(1, 2) != 1) panic("!");
	if (min(2, 1) != 1) panic("!");
	if (max(1, 2) != 2) panic("!");
	if (max(2, 1) != 2) panic("!");

	size_t x = 10;
	if (max(1, x) != 10) panic("!");
	if (min(1, x) != 1) panic("!");
	return 0;
}
