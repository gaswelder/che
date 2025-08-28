#import formats/json
#import formats/srt

int main() {
	srt.reader_t r = {};
	srt.block_t b = {};
	while (srt.block(&r, &b)) {
		print_block(&b);
	}
	return 0;
}

void print_block(srt.block_t *b) {
	printf("{");

	json.write_string(stdout, "n");
	printf(":%d,", b->index);

	json.write_string(stdout, "range");
	printf(":");
	json.write_string(stdout, b->range);
	printf(",");

	json.write_string(stdout, "text");
	printf(":");
	json.write_string(stdout, b->text);

	printf("}\n");
}
