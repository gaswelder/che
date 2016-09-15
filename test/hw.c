import "neumann_rand"

int main() {
	nrg *c = neumann_init(time(NULL));

	int i;
	for(i = 0; i < 100; i++) {
		printf("%d\n", neumann_next(c));
	}

	neumann_free(c);
}
