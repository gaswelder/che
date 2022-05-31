#import cli

enum {
	V_UNK,
	V_ERR,
	V_INT,
	V_DBL,
	V_STR
};

typedef {
	int type;
	union {
		int i;
		double d;
		char *s;
		char *e;
	} val;
} val_t;

int main()
{
	val_t a = {};
	val_t b = {};

	val_seti(&a, 42);
	val_setd(&b, 0.5);

	val_t c = val_sum(a, b);
	val_print(c);
}

void val_print(val_t v)
{
	switch(v.type) {
		case V_INT:
			printf("int(%d)", v.val.i);
			return;
		case V_DBL:
			printf("dbl(%f)", v.val.d);
			return;
		case V_STR:
			printf("str(%s)", v.val.s);
			return;
		case V_ERR:
			printf("error(%s)", v.val.e);
			return;
		default:
			fatal("oops");
	}
}

void val_seti(val_t *v, int i)
{
	if(v->type == V_STR) {
		free(v->val.s);
	}
	v->type = V_INT;
	v->val.i = i;
}

void val_setd(val_t *v, double d)
{
	if(v->type == V_STR) {
		free(v->val.s);
	}
	v->type = V_DBL;
	v->val.d = d;
}

double val_dbl(val_t v)
{
	switch(v.type) {
		case V_DBL:
			return v.val.d;
		case V_INT:
			return (double) v.val.i;
	}
	return 0;
}

bool is_num(val_t x) {
	return x.type == V_INT || x.type == V_DBL;
}

val_t val_sum(val_t a, b)
{
	val_t s = {};

	if (!is_num(a) || !is_num(b)) {
		s.type = V_ERR;
		s.val.e = "Can't produce sum for the given values";
		return s;
	}

	if(a.type == V_INT && b.type == V_INT) {
		s.type = V_INT;
		s.val.i = a.val.i + b.val.i;
		return s;
	}

	s.type = V_DBL;
	s.val.d = val_dbl(a) + val_dbl(b);
	return s;
}
