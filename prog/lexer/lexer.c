#import clex.c

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "arguments: <file.c>\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		fprintf(stderr, "failed to open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}
	clex.lexer_t *lexer = clex.newlex(f);
    while (clex.read(lexer)) {
        clex.tok_t tok = clex.tok(lexer);
		if (strcmp(tok.name, "error") == 0) {
			fprintf(stderr, "%s at %s\n", tok.content, tok.pos);
			return 1;
		}
		printf("{\"pos\":\"%s\",\"type\":\"%s\",\"content\":\"%s\"}\n", tok.pos, tok.name, tok.content);
    }
	clex.lexer_free(lexer);
	fclose(f);
    return 0;
}
