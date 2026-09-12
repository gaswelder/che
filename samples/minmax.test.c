int main() {
	if (min(1, 2) != 1) panic("!");
	if (min(2, 1) != 1) panic("!");
	if (max(1, 2) != 2) panic("!");
	if (max(2, 1) != 2) panic("!");
	return 0;
}
