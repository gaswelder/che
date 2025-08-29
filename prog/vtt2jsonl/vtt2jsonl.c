#import formats/vtt
#import formats/json

int main() {
	vtt.reader_t *r = vtt.reader(stdin);
	while (vtt.read(r)) {
		print_block(r);
	}
	if (vtt.err(r)) {
		fprintf(stderr, "%s\n", vtt.err(r));
		vtt.freereader(r);
		return 1;
	}
	vtt.freereader(r);
	return 0;
}

void print_block(vtt.reader_t *r) {
	printf("{");

	json.write_string(stdout, "range");
	printf(":");
	json.write_string(stdout, r->timerange);
	printf(",");

	json.write_string(stdout, "text");
	printf(":");
	json.write_string(stdout, r->text);

	printf("}\n");
}
