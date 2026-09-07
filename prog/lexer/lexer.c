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
    while (true) {
        clex.tok_t *tok = clex.lexer_read(lexer);
		if (!tok) {
			break;
		}
		if (strcmp(tok->name, "error") == 0) {
			fprintf(stderr, "%s at %s\n", tok->content, tok->pos);
			clex.tok_free(tok);
			return 1;
		}
		printf("{\"pos\":\"%s\",\"type\":\"%s\",\"content\":\"%s\"}\n", tok->pos, tok->name, tok->content);
		clex.tok_free(tok);
    }
	clex.lexer_free(lexer);
    return 0;
}
