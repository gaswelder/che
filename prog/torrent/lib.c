pub size_t maxsz(size_t a, b) {
    if (a < b) return b;
    return a;
}
pub size_t minsz(size_t a, b) {
    if (a < b) return a;
    return b;
}

pub void gen_id(uint8_t *peer_id) {
	for (int i = 0; i < 20; i++) {
		// Random printable character.
		peer_id[i] = 32 + (rand() % (126 - 32));
	}
}
