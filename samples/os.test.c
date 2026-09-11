#import osmod.c

int main() {
	if (osmod.foo() != 123) panic("!");
	return 0;
}