#import formats/csv
#import formats/json
#import strings

int main() {
	char val[4096];	
	char *header[100] = {};
	size_t headersize = 0;
	bool first_line = true;

	csv.reader_t *r = csv.new_reader();
	while (csv.read_line(r)) {
		if (first_line) {
			first_line = false;
			while (csv.read_val(r, val, sizeof(val))) {
				header[headersize++] = strings.newstr("%s", val);
			}
			continue;
		}

		putc('{', stdout);
		size_t i = 0;
		while (csv.read_val(r, val, sizeof(val))) {
			if (i > 0) putc(',', stdout);
			write_kv(header[i++], val);
		}
		printf("}\n");
	}

	csv.free_reader(r);
	return 0;
}

void write_kv(char *k, *v) {
	json.write_string(stdout, k);
	printf(":");
	json.write_string(stdout, v);
}
