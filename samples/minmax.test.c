int main() {
	size_t x = 10;
	if (max(1, x) != 10) panic("!");
	if (min(1, x) != 1) panic("!");

	size_t y = 20;
	if (min(x, y) != 10) panic("!");
	if (max(x, y) != 20) panic("!");

	int32_t i1 = 10;
	int32_t i2 = 20;
	if (min(i1, i2) != 10) panic("!");
	if (max(i1, i2) != 20) panic("!");

	uint32_t u1 = 10;
	uint32_t u2 = 20;
	if (min(u1, u2) != 10) panic("!");
	if (max(u1, u2) != 20) panic("!");

	
	return 0;
}
