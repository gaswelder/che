typedef short nrg;

nrg *neumann_init(short seed) {
	short *c = malloc(sizeof(short));
	if(!c) return NULL;
	*c = seed;
	return c;
}

void neumann_free(nrg *c) {
	free(c);
}

short neumann_next(nrg *c) {
	int tmp = *c * *c;
	tmp /= 100;
	tmp %= 10000;
	*c = tmp;
	return tmp;
}
