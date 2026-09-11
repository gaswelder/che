
int x[] = {1,2,3};

int main() {
	if (nelem(x) != 3) panic("!");
	foo();
	return 0;
}

void foo() {
	int nelem = 1;
	if (nelem != 1) panic("!");
}