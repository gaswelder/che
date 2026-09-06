#import formats/pdf

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "arguments: <pdf-file>\n");
		return 1;
	}
	pdf.reader_t *p = pdf.open(argv[1]);
	
	while (pdf.more(p)) {
		pdf.any_t val = pdf.next(p);
		printany(val);
	}
	if (pdf.more(p)) {
		panic("trailing input");
	}
	pdf.close(p);
	return 0;
}

void printany(pdf.any_t val) {
	switch (val.type) {
		case pdf.XREFLIST: {
			pdf.xref_list_t list = val.content.xreflist;
			printf("{\"xreflist\": [");
			for (int i = 0; i < list.n; i++) {
				int id = list.items[i].id;
				int pos = list.items[i].pos;
				if (i > 0) putchar(',');
				printf("{\"id\":%d,\"pos\":%d}", id, pos);
			}
			printf("]}");
			putchar('\n');
		}
		case pdf.OBJ: {
			pdf.obj_t o = val.content.obj;
			printf("{\"id\":%d,\"data\":[", o.id);
			for (int i = 0; i < o.size; i++) {
				if (i > 0) printf(",");
				printval(o.items[i]);
			}
			printf("]}");
			putchar('\n');
		}
		case pdf.TRAILER: {
			pdf.trailer_t t = val.content.trailer;
			printf("{\"trailer\":");
			printval(t.val);
			printf("}");
			putchar('\n');
		}
		case pdf.STARTXREF: {
			pdf.startxref_t t = val.content.startxref;
			printf("{\"startxref\":");
			printval(t.val);
			printf("}\n");
		}
		default: {
			printf("************** val %d\n", val.type);
		}
	}
}

void printval(pdf.val_t v) {
	switch (v.type) {
		case pdf.MAP: { printmap(v); }
		case pdf.NAME: { printf("\"%s\"", v.chars); }
		case pdf.NUM: { printf("%d", v.num); }
		case pdf.ARR: {
			printf("[");
			for (int i = 0; i < v.size; i++) {
				if (i > 0) printf(",");
				printval(v.vals[i]);
			}
			printf("]");
		}
		case pdf.REF: { printf("{\"refto\":%d}", v.num); }
		case pdf.HEX, pdf.STR: {
			printf("\"0x%s\"", v.chars);
		}
		case pdf.TFALSE: { printf("false"); }
		default: { panic("{val type %d}", v.type); }
	}
}

void printmap(pdf.val_t v) {
	printf("{");
	for (int i = 0; i < v.size; i++) {
		if (i > 0) printf(",");
		printval(v.keys[i]);
		printf(":");
		printval(v.vals[i]);
	}
	printf("}");
}
