The buffer doesn't make its own copy of the string, it only keeps the
reference. This is done to avoid copying large texts when not needed.
Give it an explicit copy (using strdup, for example) if needed.

Example: parse a list like "[a,b,c,d,e,]"

	// To simplify the example we assume that the separator (comma)
	// follows every element.
	const char *list = "[a,b,c,d,e,]";

	parsebuf *b = buf_new(list);

	// This example just uses asserts for everything. More advanced
	// code could do appropriate recovery.
	assert(b != NULL);

	// The first character must be '['
	assert(buf_get(b) == '[');

	// "while there are more characters and the following
	// one is not ']'"
	while(buf_more(b) && buf_peek(b) != ']') {
		int c = buf_get(b);
		assert(isalpha(c));
		assert(buf_get(b) == ',');
		printf("item: %c\n", c);
	}

	// The list must end with ']'
	assert(buf_get(b) == ']');

	// And nothing must follow the list
	assert(buf_get(b) == EOF);
	buf_free(b);
