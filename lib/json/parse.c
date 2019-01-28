import "parsebuf"

/*
 * Parses given JSON string and returns a json_node object.
 */
pub json_node *json_parse(const char *s)
{
	parser_t p = {};

	p.buf = buf_new(s);
	if(!p.buf) {
		return NULL;
	}
	p.next.type = EOF;

	json_node *n = NULL;

	if(peek(&p) == EOF) {
		n = json_newerror("No content");
		buf_free(p.buf);
		return n;
	}

	// <node>
	n = read_node(&p);
	if(!n) {
		n = json_newerror(p.err);
		buf_free(p.buf);
		return n;
	}

	// <eof>
	if(peek(&p) != EOF) {
		json_free(n);
		n = json_newerror("End of file expected");
		buf_free(p.buf);
		return n;
	}

	buf_free(p.buf);
	return n;
}

/*
 * Reads one node and returns it.
 * Returns NULL in case of error.
 */
json_node *read_node(parser_t *p)
{
	switch(peek(p)) {
		case EOF:
			return error(p, "Unexpected end of file");
		case '[':
			return read_array(p);
		case '{':
			return read_dict(p);
		case T_STR:
			token_t t = {};
			get(p, &t);
			json_node *n = json_newstr(t.str);
			free(t.str);
			return n;
		case T_NUM:
			token_t t = {};
			get(p, &t);
			return json_newnum(t.num);
		case T_TRUE:
			return json_newbool(true);
		case T_FALSE:
			return json_newbool(false);
		case T_NULL:
			return json_newnull();
	}
	assert(false);
	return NULL;
}

json_node *read_array(parser_t *p)
{
	if(!expect(p, '[')) {
		return NULL;
	}

	json_node *a = json_newarr();

	if(peek(p) != ']') {
		while(peek(p) != EOF) {
			json_node *v = read_node(p);
			if(!v) {
				json_free(a);
				return NULL;
			}

			json_push(a, v);

			if(peek(p) != ',') {
				break;
			}
			get(p, NULL);
		}
	}
	if(!expect(p, ']')) {
		json_free(a);
		return NULL;
	}

	return a;
}

json_node *read_dict(parser_t *p)
{
	if(!expect(p, '{')) {
		return NULL;
	}

	json_node *o = json_newobj();
	if(!o) return NULL;

	if(peek(p) != '}') {
		token_t t = {};

		while(peek(p) != EOF) {

			if(peek(p) != T_STR) {
				json_free(o);
				return error(p, "Key expected");
			}

			get(p, &t);
			char *key = t.str;

			if(!expect(p, ':')) {
				free(key);
				json_free(o);
				return NULL;
			}

			json_node *val = read_node(p);
			if(!val) {
				free(key);
				json_free(o);
				return NULL;
			}

			if(!json_put(o, key, val)) {
				free(key);
				json_free(val);
				json_free(o);
				return NULL;
			}

			if(peek(p) != ',') {
				break;
			}
			get(p, NULL);
		}
	}

	if(!expect(p, '}')) {
		json_free(o);
		return NULL;
	}

	return o;
}

bool expect(parser_t *p, int toktype)
{
	token_t t = {};
	get(p, &t);
	if(t.type != toktype) {
		error(p, "'%c' expected", toktype);
		return false;
	}
	return true;
}

/*
 * Puts a formatted error message to the parser's
 * context and returns NULL.
 */
void *error(parser_t *p, const char *fmt, ...)
{
	va_list args = {};
	va_start(args, fmt);
	vsnprintf(p->err, sizeof(p->err), fmt, args);
	va_end(args);
	printf("error: %s\n", p->err);
	return NULL;
}
