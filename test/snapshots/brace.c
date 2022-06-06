
int main() {
    if (t && (a || b)) {}
}

uint32_t rotate(uint32_t value, int bits) {
	return (value << bits) | (value >> (32-bits));
}
