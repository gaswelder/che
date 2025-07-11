#import formats/csv
#import formats/json
#import opt
#import strings

int main(int argc, char *argv[]) {
	bool noheader = false;
	opt.flag("n", "the first line is not header", &noheader);
	opt.parse(argc, argv);

	char *colnames[100] = {};
	for (size_t i = 0; i < 100; i++) {
		colnames[i] = strings.newstr("col%d", i);
	}

	csv.reader_t *r = csv.newreader();

	if (!noheader) {
		size_t i = 0;
		while (csv.readval(r)) {
			strcpy(colnames[i], csv.val(r));
			i++;
		}
		csv.nextline(r);
	}

	while (true) {
		size_t i = 0;
		while (csv.readval(r)) {
			val(colnames[i], csv.val(r));
			i++;
		}
		end();
		if (!csv.nextline(r)) {
			break;
		}
	}

	csv.freereader(r);
	return 0;
}

bool line_started = false;

void val(const char *field, *val) {
	if (!line_started) {
		printf("{");
		write_kv(field, val);
		line_started = true;
	} else {
		printf(",");
		write_kv(field, val);
	}
}
void end() {
	if (!line_started) return;
	printf("}\n");
	line_started = false;
}

void write_kv(const char *k, *v) {
	json.write_string(stdout, k);
	printf(":");
	json.write_string(stdout, v);
}
