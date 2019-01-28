int main() {
    int t = 2;
    switch(t) {
		case 2:
			t = 1;
			break;
		case 3:
			t = 2;
			break;
		default:
			printf("Unknown zio type: %s\n", type);
			return 1;
	}
    return 0;
}