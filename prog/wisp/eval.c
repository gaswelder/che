#import eater.c
#import hashtab.c
#import mem.c
#import mp.c
#import os/self
#import strings
#import util.c

enum { INT, FLOAT, STRING, SYMBOL, CONS, VECTOR, CFUNC, SPECIAL, DETACH }

pub typedef {
	int type;
	uint32_t refs;
	void *val; // value pointer, for non-function values.
	cfunc_t *fval; // function pointer, for function value.
} val_t;

enum { ADD, SUB, MUL, DIV }
enum { EQ, LT, LTE, GT, GTE }

////////

pub typedef {
	char *name;
	val_t **vals;
	val_t **stack;
	uint32_t cnt;
} symbol_t;

pub typedef { val_t *car, *cdr; } cons_t;
pub typedef val_t *cfunc_t(val_t *);

typedef {
	val_t **v;
	size_t len;
} vector_t;

pub typedef {
	char *raw;
	char *print;
	size_t len;
} str_t;

typedef {
	int in, out;
	int proc;
	reader_t *read;
} detach_t;

// #define REQ(lst, n, so) if (req_length (lst, so, n) == err_symbol)	return err_symbol;
// #define REQX(lst, n, so) if (reqx_length (lst, so, n) == err_symbol)	return err_symbol;
// #define REQPROP(lst) if (properlistp(lst) == NIL) return THROW(err_improper_list, lst);

hashtab.hashtab_t *symbol_table = NULL;

mem.mmanager_t *mm_obj = NULL;
mem.mmanager_t *mm_cons = NULL;
mem.mmanager_t *mm_str = NULL;
mem.mmanager_t *mm_vec = NULL;

val_t *out_of_bounds = NULL;
val_t *NIL = NULL;
val_t *T = NULL;
val_t *lambda = NULL;
val_t *macro = NULL;
val_t *quote = NULL;
val_t *err_symbol = NULL;
val_t *err_thrown = NULL;
val_t *err_attach = NULL;
val_t *rest = NULL;
val_t *optional = NULL;
val_t *doc_string = NULL;
val_t *err_void_function = NULL;
val_t *err_wrong_number_of_arguments = NULL;
val_t *err_wrong_type = NULL;
val_t *err_improper_list = NULL;
val_t *err_improper_list_ending = NULL;
val_t *err_interrupt = NULL;

pub val_t *nil() {
	return NIL;
}

void DOC (const char *s) {
	(void) s;
}

bool issymbol(val_t *o) { return o->type == SYMBOL; }
bool iscons(val_t *o) { return o->type == CONS; }
bool isint(val_t *o) { return o->type == INT; }
bool isfloat(val_t *o) { return (o->type == FLOAT); }
bool isvector(val_t *o) { return o->type == VECTOR; }
bool isdetach(val_t *o) { return (o->type == DETACH); }

bool isnumber(val_t *o) { return (isint (o) || isfloat (o)); }
bool islist(val_t *o) { return (o->type == CONS || o == NIL); }

bool isfunc(val_t *o) {
	return (o->type == CONS && CAR(o)->type == SYMBOL && ((CAR(o) == lambda) || (CAR(o) == macro)))
		|| (o->type == CFUNC)
		|| (o->type == SPECIAL);
}

// Increments refcount of the value and returns the value.
val_t *increfs(val_t *o) {
	o->refs++;
	return o;
}

val_t *CAR(val_t *o) { cons_t *x = o->val; return x->car; }
val_t *CDR(val_t *o) { cons_t *x = o->val; return x->cdr; }
int OPROC(val_t *o) { detach_t *x = o->val; return x->proc; }
void *OREAD(val_t *o) { detach_t *x = o->val; return x->read; }
mp.mpz_t *OINT(val_t *o) {
	return o->val;
}
mp.mpf_t *OFLOAT(val_t *o) { return (o->val); }
mp.mpz_t *DINT(val_t *o) {	mp.mpz_t *x = o->val; return x; }
mp.mpf_t *DFLOAT(val_t *o) { mp.mpf_t *x = o->val; return x; }
int VLENGTH(val_t *o) { vector_t *x = o->val; return x->len; }
pub char **SYMNAME(val_t *so) { symbol_t *x = (so)->val; return &x->name; }


char *OSTR(val_t *o) {
	str_t *x = o->val;
	return x->raw;
}
int OSTRLEN(val_t *o) {
	str_t *x = o->val;
	return x->len;
}
char *OSTRP(val_t *o) {
	str_t *str = o->val;
	if (str->print == NULL) {
		str->print = util.addslashes(str->raw);
	}
	return str->print;
}

void setcar(val_t *o, *car) {
	cons_t *x = o->val;
	x->car = car;
}
void setcdr(val_t *o, *cdr) {
	cons_t *x = o->val;
	x->cdr = cdr;
}

// void *PAIRP(void *o) {
//	 return (o->type == CONS && !islist(CDR(o)));
// }

val_t *THROW(val_t *to, *ao) {
	err_thrown = to;
	err_attach = ao;
	return err_symbol;
}

val_t *c_vec (size_t len, val_t * init) {
	val_t *o = obj_create(VECTOR);
	vector_t *v = o->val;
	v->len = len;
	if (len == 0) {
		len = 1;
	}
	v->v = calloc!(len, sizeof (val_t **));
	for (size_t i = 0; i < v->len; i++) {
		v->v[i] = increfs(init);
	}
	return o;
}

val_t *list2vector (val_t *lst) {
	int len = 0;
	val_t *p = lst;
	while (p != NIL) {
		len++;
		p = CDR (p);
	}
	val_t *v = c_vec(len, NIL);
	p = lst;
	size_t i = 0;
	while (p != NIL) {
		vset(v, i, increfs(CAR (p)));
		i++;
		p = CDR(p);
	}
	return v;
}

void vset (val_t * vo, size_t i, val_t * val) {
	vector_t *v = vo->val;
	val_t *o = v->v[i];
	v->v[i] = val;
	obj_release(o);
}

val_t *vset_check(val_t *vo, *io, *val) {
	int i = into2int (io);
	vector_t *v = vo->val;
	if (i < 0 || i >= (int) v->len) {
		return THROW (out_of_bounds, increfs(io));
	}
	vset (vo, i, increfs(val));
	return increfs(val);
}

val_t *vget (val_t * vo, size_t i)
{
	vector_t *v = vo->val;
	return v->v[i];
}

val_t *vget_check (val_t * vo, val_t * io)
{
	int i = into2int (io);
	vector_t *v = vo->val;
	if (i < 0 || i >= (int) v->len) {
		return THROW (out_of_bounds, increfs(io));
	}
	return increfs(vget (vo, i));
}

val_t *vector_concat(val_t *a, *b) {
	size_t al = VLENGTH (a);
	size_t bl = VLENGTH (b);
	val_t *c = c_vec (al + bl, NIL);
	size_t i;
	for (i = 0; i < al; i++) {
		vset (c, i, increfs(vget (a, i)));
	}
	for (i = 0; i < bl; i++) {
		vset (c, i + al, increfs(vget (b, i)));
	}
	return c;
}

val_t *vector_sub (val_t * vo, int start, int end)
{
	vector_t *v = vo->val;
	if (end == -1)
		end = v->len - 1;
	val_t *newv = c_vec (1 + end - start, NIL);
	int i;
	for (i = start; i <= end; i++)
		vset (newv, i - start, increfs(vget (vo, i)));
	return newv;
}

void cons_clear(void *o) {
	cons_t *c = (cons_t *) o;
	c->car = NIL;
	c->cdr = NIL;
}



// exact
val_t *req_length(val_t * lst, val_t * thr, int n) {
	/* TODO detect loops? */
	int len = 0;
	val_t *p = lst;
	while (p != NIL) {
		len++;
		p = CDR (p);
		if (!islist (p)) {
			obj_release(thr);
			return THROW (getsym_improper_list(), lst);
		}
		if (len > n) {
			return THROW (err_wrong_number_of_arguments, thr);
		}
	}
	if (len != n) {
		return THROW (err_wrong_number_of_arguments, thr);
	}
	return T;
}

/* min */
val_t *reqm_length (val_t * lst, val_t * thr, int n) {
	/* TODO detect loops? */
	int len = 0;
	val_t *p = lst;
	while (p != NIL) {
		len++;
		p = CDR (p);
		if (!islist (p)) {
			obj_release(thr);
			return THROW (getsym_improper_list(), lst);
		}
		if (len >= n) return T;
	}
	if (len < n) {
		return THROW (err_wrong_number_of_arguments, thr);
	}
	return T;
}

/* max */
val_t *reqx_length (val_t * lst, val_t * thr, int n) {
	/* TODO detect loops? */
	int len = 0;
	val_t *p = lst;
	while (p != NIL)
	{
		len++;
		p = CDR (p);
		if (!islist (p)) {
			obj_release(thr);
			return THROW (getsym_improper_list(), lst);
		}
		if (len > n) return THROW (err_wrong_number_of_arguments, thr);
	}
	return T;
}

/* Verifies that list has form of a function. */
int is_func_form (val_t * lst)
{
	if (!islist (CAR (lst)))
		return 0;
	return is_var_list (CAR (lst));
}

/* Verifies that list is made only of symbols. */
int is_var_list (val_t * lst) {
	int nrest = -1;
	while (lst != NIL) {
		val_t *car = CAR(lst);
		/* &rest must be the second to last item */
		if (nrest >= 0) {
			nrest--;
			if (nrest < 0) return 0;
			if (car == rest) return 0;
		}
		if (car->type != SYMBOL) return 0;
		if (car == rest) nrest = 1;
		lst = CDR (lst);
	}
	if (nrest == 1) return 0;
	return 1;
}

// /* Determine if list is proper. */
// /* Determine if list is proper (ends with NIL) */
// val_t *properlistp (val_t * lst)
// {
// 	if (lst == NIL)
// 		return T;
// 	if (!iscons (lst))
// 		return NIL;
// 	return properlistp (CDR (lst));
// }



pub val_t *GET(val_t *so) {
	symbol_t *x = so->val;
	return *x->vals;
}

pub void SET(val_t *so, *o) {
	obj_release(GET(so));
	increfs(o);
	symbol_t *x = (so)->val;
	*x->vals = o;
}

// Binds val to name.
void bind(char *name, val_t *val) {
	val_t *sym = symbol(name);
	symbol_t *x = sym->val;
	obj_release(*x->vals);
	*x->vals = val;
}

symbol_t *symbol_create() {
	size_t cnt = 8;
	void *x = calloc!(cnt, sizeof(val_t *));
	symbol_t *s = calloc!(1, sizeof (symbol_t));
	s->cnt = cnt;
	s->vals = x;
	s->stack = x;
	return s;
}

/* Dynamic scoping */
pub void sympush(val_t *so, val_t *o) {
	symbol_t *s = so->val;
	s->vals++;
	// if (s->vals == s->cnt + s->stack) {
	//		 size_t n = s->vals - s->stack;
	//		 s->cnt *= 2;
	//		 s->stack = xrealloc (s->stack, s->cnt * sizeof (val_t *));
	//		 s->vals = s->stack + n;
	//	 }
	*s->vals = o;
	increfs(o);
}

/* Dynamic scoping */
pub void sympop (val_t * so) {
	obj_release(GET (so));
	symbol_t *s = so->val;
	s->vals--;
}



// Returns the symbol with the given name, creating it if needed.
pub val_t *symbol(char *name) {
	val_t *o = hashtab.ht_search(symbol_table, name, strlen (name));
	if (o) {
		return o;
	}
	o = obj_create(SYMBOL);
	*SYMNAME(o) = strings.newstr("%s", name);
	symbol_t *x = o->val;
	*x->vals = NIL;
	if (name[0] == ':') {
		SET (o, o);
	}
	hashtab.ht_insert(symbol_table, *SYMNAME(o), strlen(*SYMNAME(o)), o, sizeof(val_t *));
	return o;
}

// /* Used for debugging: print string followed by	*/
// void DB_OP(int str, o) {
//	 printf(str); obj_print(o,1);
// }

val_t *obj_create(int type) {
	val_t *o = mem.mm_alloc(mm_obj);
	o->type = type;
	o->refs++;
	switch (type) {
		case INT: { o->val = calloc!(1, sizeof(mp.mpz_t *)); }
		case FLOAT: { o->val = calloc!(1, sizeof(mp.mpf_t *)); }
		case CONS: { o->val = mem.mm_alloc(mm_cons); }
		case SYMBOL: { o->val = symbol_create (); }
		case STRING: { o->val = mem.mm_alloc(mm_str); }
		case VECTOR: { o->val = mem.mm_alloc(mm_vec); }
		case DETACH: { o->val = calloc!(1, sizeof (detach_t)); }
		case CFUNC, SPECIAL: {}
	}
	return o;
}

void obj_release(val_t *o) {
	if (o == NULL || o->type == SYMBOL) {
		return;
	}
	o->refs--;
	if (o->refs > 0) {
		return;
	}
	switch (o->type) {
		case FLOAT, INT: {
			free(o->val);
		}
		case STRING: {
			str_t *str = o->val;
			free(str->raw);
			if (str->print != NULL) {
				free(str->print);
			}
			mem.mm_free(mm_str, str);
		}
		case CONS: {
			obj_release(CAR (o));
			obj_release(CDR (o));
			cons_t *c = o->val;
			mem.mm_free (mm_cons, c);
		}
		case VECTOR: {
			vector_t *v = o->val;
			for (size_t i = 0; i < v->len; i++) {
				obj_release(v->v[i]);
			}
			free(v->v);
			mem.mm_free(mm_vec, v);
		}
		case DETACH: {
			detach_t *d = (o->val);
			reader_destroy (d->read);
			OS.close (d->in);
			OS.close (d->out);
			free ((o->val));
		}
		case CFUNC, SPECIAL: {}
	}
	mem.mm_free(mm_obj, o);
}

pub val_t *newcons(val_t *car, *cdr) {
	val_t *o = obj_create (CONS);
	setcar(o, car);
	setcdr(o, cdr);
	return o;
}

val_t *newfunc(cfunc_t f) {
	val_t *o = obj_create(CFUNC);
	o->fval = f;
	return o;
}

val_t *c_special(cfunc_t f) {
	val_t *o = obj_create(SPECIAL);
	o->fval = f;
	return o;
}

// Prints object o to stdout.
void obj_print(val_t *o, bool newline) {
	switch (o->type) {
		case CONS: {
			printf("(");
			val_t *p = o;
			while (p->type == CONS)
				{
					obj_print (CAR (p), 0);
					p = CDR (p);
					if (p->type == CONS)
						printf (" ");
				}
						if (p != NIL)
				{
					printf (" . ");
					obj_print (p, 0);
				}
						printf (")");
		}
		case INT: { mp.print("%Zd", OINT (o)); }
		case FLOAT: { mp.print("%.Ff", OFLOAT (o)); }
		case STRING: { printf ("%s", OSTRP (o)); }
		case SYMBOL: {
			symbol_t *x = (o->val);
			printf("%s", x->name);
		}
		case VECTOR: { vec_print (o); }
		case DETACH: { detach_print (o); }
		case CFUNC: { printf ("<cfunc>"); }
		case SPECIAL: { printf ("<special form>"); }
		default: { printf ("ERROR"); }
	}
	if (newline) printf ("\n");
}

void vec_print(val_t * vo) {
	vector_t *v = vo->val;
	if (v->len == 0) {
		printf ("[]");
		return;
	}
	printf ("[");
	for (size_t i = 0; i < v->len - 1; i++) {
		obj_print (v->v[i], 0);
		printf (" ");
	}
	obj_print (v->v[v->len - 1], 0);
	printf ("]");
}

uint32_t str_hash (val_t * o) {
	return hash (OSTR (o), OSTRLEN (o));
}

uint32_t int_hash(val_t * o) {
	char str[100] = {};
	mp.mpz_sprint(str, DINT(o));
	return hash(str, strlen(str));
}

uint32_t float_hash (val_t * o) {
	char str[100] = {};
	mp.mpf_sprint(str, DFLOAT(o));
	return hash(str, strlen(str));
}

uint32_t vector_hash (val_t * o)
{
	uint32_t accum = 0;
	vector_t *v = (o->val);
	size_t i;
	for (i = 0; i < v->len; i++)
		accum ^= obj_hash (v->v[i]);
	return accum;
}

uint32_t cons_hash (val_t * o)
{
	return obj_hash (CAR (o)) ^ obj_hash (CDR (o));
}

uint32_t symbol_hash (val_t * o) {
	char *n = *SYMNAME(o);
	return hash(n, strlen(n));
}

uint32_t obj_hash (val_t * o) {
	switch (o->type) {
		case CONS: { return cons_hash (o); }
		case INT: { return int_hash (o); }
		case FLOAT: { return float_hash (o); }
		case STRING: { return str_hash (o); }
		case SYMBOL: { return symbol_hash (o); }
		case VECTOR: { return vector_hash (o); }
		case DETACH: { return detach_hash (o); }
		case CFUNC, SPECIAL: {
			/* Imprecise, but close enough */
			return hash ((o->val), sizeof (void *));
		}
	}
	return 0;
}

uint32_t hash(void *key, size_t keylen) {
	uint32_t hash = 0;
	char *skey = key;
	for (uint32_t i = 0; i < keylen; ++i) {
			hash += skey[i];
			hash += (hash << 10);
			hash ^= (hash >> 6);
		}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}





pub val_t *getsym_improper_list() {
	return err_improper_list;
}

/* Stack counting */
uint32_t stack_depth = 0;
uint32_t max_stack_depth = 20000;

int interrupt = 0;
int interactive_mode = 0;
void handle_iterrupt (int sig)
{
	(void) sig;
	if (!interrupt && interactive_mode)
		{
			interrupt = 1;
			signal (SIGINT, &handle_iterrupt);
		}
	else
		{
			signal (SIGINT, SIG_DFL);
			OS.raise (SIGINT);
		}
}

void object_clear(void *o) {
	val_t *obj = o;
	obj->type = SYMBOL;
	obj->refs = 0;
	obj->fval = NULL;
	obj->val = NIL;
}

void vector_clear (void *o) {
	vector_t *v = o;
	v->v = NULL;
	v->len = 0;
}

pub void wisp_init() {
	mm_obj = mem.mm_create (sizeof (val_t), &object_clear);
	mm_cons = mem.mm_create (sizeof (cons_t), &cons_clear);
	mm_str = mem.mm_create (sizeof (str_t), &str_clear);
	mm_vec = mem.mm_create (sizeof (vector_t), &vector_clear);

	symbol_table = hashtab.new();
	/* Set up t and nil constants. The SET macro won't work until NIL is set. */
	NIL = symbol("nil");
	symbol_t *x = NIL->val;
	*x->vals = NIL;
	T = symbol("t");
	SET (T, T);

	bind("+", newfunc(&addition));
	bind("*", newfunc(&multiplication));
	bind("-", newfunc(&subtraction));
	bind("/", newfunc(&division));
	bind("=", newfunc(&num_eq));
	bind("<", newfunc(&num_lt));
	bind("<=", newfunc(&num_lte));
	bind(">", newfunc(&num_gt));
	bind(">=", newfunc(&num_gte));
	bind("%", newfunc(&modulus));

	bind("cdoc-string", newfunc(&cdoc_string));
	bind("apply", newfunc(&lisp_apply));
	bind("and", c_special (&lisp_and));
	bind("or", c_special (&lisp_or));
	bind("quote", c_special (&lisp_quote));
	bind("lambda", c_special (&lambda_f));
	bind("defun", c_special (&defun));
	bind("defmacro", c_special (&defmacro));
	bind("car", newfunc(&lisp_car));
	bind("cdr", newfunc(&lisp_cdr));
	bind("list", newfunc(&lisp_list));
	bind("if", c_special (&lisp_if));
	bind("not", newfunc(&nullp));
	bind("progn", c_special (&progn));
	bind("let", c_special (&let));
	bind("while", c_special (&lisp_while));
	bind("eval", newfunc(&eval_body));
	bind("print", newfunc(&lisp_print));
	bind("cons", newfunc(&lisp_cons));
	bind("cond", c_special (&lisp_cond));

	/* Symbol table */
	bind("set", newfunc(&lisp_set));
	bind("value", newfunc(&lisp_value));
	bind("symbol-name", newfunc(&symbol_name));

	/* Strings */
	bind("concat2", newfunc(&lisp_concat));

	/* Equality */
	bind("eq", newfunc(&eq));
	bind("eql", newfunc(&eql));
	bind("hash", newfunc(&lisp_hash));

	/* Predicates */
	bind("nullp", newfunc(&nullp));
	bind("funcp", newfunc(&funcp));
	bind("listp", newfunc(&listp));
	bind("symbolp", newfunc(&symbolp));
	bind("stringp", newfunc(&stringp));
	bind("numberp", newfunc(&numberp));
	bind("integerp", newfunc(&integerp));
	bind("floatp", newfunc(&floatp));
	bind("vectorp", newfunc(&vectorp));

	/* Input/Output */
	bind("load", newfunc(&lisp_load));
	bind("read-string", newfunc(&lisp_read_string));

	/* Error handling */
	bind("throw", newfunc(&throw));
	bind("catch", c_special (&catch));

	/* Vectors */
	bind("vset", newfunc(&lisp_vset));
	bind("vget", newfunc(&lisp_vget));
	bind("vlength", newfunc(&lisp_vlength));
	bind("make-vector", newfunc(&make_vector));
	bind("vconcat2", newfunc(&lisp_vconcat));
	bind("vsub", newfunc(&lisp_vsub));

	/* Internals */
	bind("refcount", newfunc(&lisp_refcount));
	bind("eval-depth", newfunc(&lisp_eval_depth));
	bind("max-eval-depth", newfunc(&lisp_max_eval_depth));

	/* System */
	bind("exit", newfunc(&lisp_exit));

	/* Detachments */
	bind("detach", newfunc(&lisp_detach));
	bind("receive", newfunc(&lisp_receive));
	bind("send", newfunc(&lisp_send));

	signal (SIGINT, &handle_iterrupt);

	out_of_bounds = symbol("index-out-of-bounds");
	lambda = symbol("lambda");
	macro = symbol("macro");
	quote = symbol("quote");
	rest = symbol("&rest");
	optional = symbol("&optional");
	doc_string = symbol("doc-string");

	/* error symbols */
	err_symbol = symbol("wisp-error");
	SET (err_symbol, err_symbol);
	err_thrown = err_attach = NIL;
	err_void_function = symbol("void-function");
	err_wrong_number_of_arguments = symbol("wrong-number-of-arguments");
	err_wrong_type = symbol("wrong-type-argument");
	err_improper_list = symbol("improper-list");
	err_improper_list_ending = symbol("improper-list-ending");
	err_interrupt = symbol("caught-interrupt");

	/* set up wisproot */
	const char *wisproot = self.getenv("WISPROOT");
	if (wisproot == NULL || strlen(wisproot) == 0) {
		wisproot = ".";
	}
	SET (symbol("wisproot"), newstring(wisproot));

	/* Load core lisp code. */
	char *corepath = strings.newstr("%s/core.wisp", wisproot);
	if (!load_file(NULL, corepath, 0)) {
		fprintf(stderr, "could not load core lisp at \"%s\": %s\n", corepath, strerror(errno));
		fprintf(stderr, "(WISPROOT=%s)\n", wisproot);
		exit(1);
	}
	free(corepath);
}

/* Convenience function for creating a REPL. */
pub void repl () {
	interactive_mode = 1;
	load_file (stdin, "<stdin>", 1);
}

/* Use the core functions above to eval each sexp in a file. */
pub bool load_file(FILE *f, char *filename, int interactive) {
	if (f == NULL) {
		f = fopen(filename, "r");
		if (f == NULL) return false;
	}
	reader_t *r = reader_create(f, NULL, filename, interactive);
	while (!r->eof) {
		val_t *sexp = read_sexp(r);
		if (sexp == err_symbol) {
			continue;
		}
		stack_depth = 0;
		val_t *ret = eval (sexp);
		if (ret == err_symbol) {
			printf ("error: ");
			val_t *c = newcons (err_thrown, newcons (err_attach, NIL));
			obj_print (c, 1);
			obj_release(c);
		} else {
			if (r->interactive) {
				obj_print (ret, 1);
			}
		}
		obj_release(sexp);
		obj_release(ret);
	}
	reader_destroy (r);
	return true;
}

val_t *eval_list (val_t * lst) {
	if (lst == NIL) {
		return NIL;
	}
	if (!iscons (lst)) {
		return THROW(err_improper_list_ending, increfs(lst));
	}
	val_t *car = eval (CAR (lst));
	if (car == err_symbol) {
		return car;
	}
	val_t *cdr = eval_list (CDR (lst));
	if (cdr == err_symbol) {
		obj_release(car);
		return err_symbol;
	}
	return newcons (car, cdr);
}

val_t *eval_body (val_t * body) {
	val_t *r = NIL;
	while (body != NIL) {
		obj_release(r);
		r = eval (CAR (body));
		if (r == err_symbol) {
			return r;
		}
		body = CDR (body);
	}
	return r;
}

val_t *assign_args (val_t * vars, val_t * vals)
{
	int optional_mode = 0;
	int len = 0;
	val_t *orig_vars = vars;
	while (vars != NIL) {
		val_t *var = CAR (vars);
		if (var == optional) {
			/* Turn on optional mode and continue. */
			optional_mode = 1;
			vars = CDR (vars);
			continue;
		}
			if (var == rest) {
			/* Assign the rest of the list and finish. */
			vars = CDR (vars);
			sympush (CAR (vars), vals);
			vals = NIL;
			break;
		}
			else if (!optional_mode && vals == NIL)
	{
		while (len > 0) {
			sympop (CAR (orig_vars));
			orig_vars = CDR (orig_vars);
			len--;
		}
		return THROW (err_wrong_number_of_arguments, NIL);
	}
			else if (optional_mode && vals == NIL)
	{
		sympush (var, NIL);
	}
			else {
				val_t *val = CAR (vals);
				sympush (var, val);
				len++;
			}
			vars = CDR (vars);
			if (vals != NIL) vals = CDR (vals);
		}

	/* vals should be consumed by now */
	if (vals != NIL) {
		unassign_args(vars);
		return THROW(err_wrong_number_of_arguments, NIL);
	}
	return T;
}

void unassign_args(val_t *vars) {
	if (vars == NIL) return;
	val_t *var = CAR (vars);
	if (var != rest && var != optional) {
		sympop (var);
	}
	unassign_args (CDR (vars));
}

val_t *eval(val_t * o) {
	/* Check for interrupts. */
	if (interrupt) {
		interrupt = 0;
		return THROW (err_interrupt, newstring("interrupted"));
	}
	if (o->type != CONS && o->type != SYMBOL) {
		return increfs(o);
	}
	if (o->type == SYMBOL) {
		return increfs(GET (o));
	}

	/* Find the function. */
	val_t *f = eval (CAR (o));
	if (f == err_symbol) {
		return f;
	}
	val_t *extrao = NIL;
	if (isvector (f)) {
		extrao = o = newcons (increfs(f), increfs(o));
		f = eval (symbol("vfunc"));
		if (f == err_symbol) {
			obj_release(extrao);
			return err_symbol;
		}
	}
	if (!isfunc (f)) {
		obj_release(f);
		return THROW (err_void_function, increfs(CAR (o)));
	}

	/* Check the stack */
	if (++stack_depth >= max_stack_depth)
		return THROW (symbol("max-eval-depth"), c_int (stack_depth--));

	/* Handle argument list */
	val_t *args = CDR (o);
	if (f->type == CFUNC || (f->type == CONS && (CAR (f) == lambda))) {
		/* c function or list function (eval args) */
		args = eval_list (args);
		if (args == err_symbol) {
			obj_release(f);
			obj_release(extrao);
			return err_symbol;
		}
	} else {
		/* so we can destroy args no matter what */
		increfs(args);
	}

	val_t *ret = apply (f, args);
	stack_depth--;
	obj_release(f);
	obj_release(args);
	obj_release(extrao);		/* vector as function */
	return ret;
}

val_t *apply(val_t *f, *args) {
	if (f->type == CFUNC || f->type == SPECIAL) {
		cfunc_t *cf = f->fval;
		return cf(args);
	}
	/* list form */
	val_t *vars = CAR (CDR (f));
	val_t *assr = assign_args (vars, args);
	if (assr == err_symbol) {
		err_attach = increfs(args);
		return err_symbol;
	}
	val_t *r;
	if (CAR (f) == lambda) {
		r = eval_body (CDR (CDR (f)));
	} else {
		val_t *body = eval_body (CDR (CDR (f)));
		r = eval (body);
		obj_release(body);
	}
	unassign_args (vars);
	return r;
}





void str_clear (void *s) {
	str_t *str = (str_t *) s;
	str->raw = NULL;
	str->print = NULL;
	str->len = 0;
}



val_t *c_str (char *str, size_t len) {
	val_t *o = obj_create(STRING);
	str_t *x = o->val;
	x->raw = str;
	x->len = len;
	return o;
}

pub val_t *newstring(const char *str) {
	return c_str(strings.newstr("%s", str), strlen(str));
}

val_t *c_ints(char *nstr) {
	val_t *o = obj_create(INT);
	mp.mpz_t *z = o->val;
	mp.mpz_set_str(z, nstr);
	return o;
}

val_t *c_int(int n) {
	val_t *o = obj_create (INT);
	mp.mpz_set_ui(o->val, n);
	return o;
}

val_t *c_floats(char *fstr) {
	val_t *o = obj_create (FLOAT);
	mp.mpf_t *f = o->val;
	mp.mpf_set_str(f, fstr);
	return o;
}

val_t *c_float(double d) {
	val_t *o = obj_create (FLOAT);
	mp.mpf_set_d(o->val, d);
	return o;
}

int into2int(val_t *into) {
	return mp.mpz_get_si(DINT(into));
}

double floato2float(val_t *floato) {
	return mp.mpf_get_d(DFLOAT(floato));
}

val_t *parent_detach = NULL;

uint8_t detach_hash (val_t * o)
{
	int proc = OPROC (o);
	return hash (&proc, sizeof (int));
}

void detach_print (val_t * o)
{
	int proc = OPROC (o);
	printf ("<detach %d>", proc);
}

val_t *c_detach (val_t * o) {
	if (o->type == SYMBOL) o = GET (o);
	val_t *dob = obj_create (DETACH);
	detach_t *d = ((dob)->val);
	int pipea[2];
	int pipeb[2];
	if (OS.pipe (pipea) != 0)
		THROW (symbol("detach-pipe-error"), newstring(strerror(errno)));
	if (OS.pipe (pipeb) != 0)
		THROW (symbol("detach-pipe-error"), newstring(strerror(errno)));
	d->proc = OS.fork ();
	if (d->proc == 0)
		{
			/* Child process */

			/* Set up pipes */
			d->in = pipeb[0];
			d->out = pipea[1];
			OS.close (pipeb[1]);
			OS.close (pipea[0]);

			/* Change stdin and stdout. */
			fclose (stdin);
			int stdout_no = OS.fileno (stdout);
			OS.close (stdout_no);
			OS.dup2 (d->out, stdout_no);
			d->read = reader_create (OS.fdopen (d->in, "r"), NULL, "parent", 0);
			parent_detach = dob;

			/* Execute given function. */
			val_t *f = newcons (o, NIL);
			eval (f);
			exit (0);
			return THROW (symbol("exit-failed"), dob);
		}
	/* Parent process */
	d->in = pipea[0];
	d->out = pipeb[1];
	OS.close (pipea[1]);
	OS.close (pipeb[0]);
	d->read = reader_create (OS.fdopen (d->in, "r"), NULL, "detach", 0);
	return dob;
}

val_t *lisp_detach(val_t * lst) {
	if (lst == doc_string) {
		return newstring("Create process detachment.");
	}
	if (req_length(lst, symbol("detach"), 1) == err_symbol) {
		return err_symbol;
	}
	return c_detach (CAR (lst));
}

val_t *lisp_receive (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Get an object from the detached process.");
	}
	if (req_length (lst,symbol("receive"),1) == err_symbol)	return err_symbol;
	val_t *d = CAR (lst);
	if (!isdetach (d)) return THROW (err_wrong_type, increfs(d));
	reader_t *r = OREAD (d);
	return read_sexp (r);
}

val_t *lisp_send (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Send an object to the parent process.");
	}
	if (req_length (lst,symbol("send"),1) == err_symbol)	return err_symbol;
	val_t *o = CAR (lst);
	if (parent_detach == NULL || parent_detach == NIL) {
		return THROW (symbol("send-from-non-detachment"), increfs(o));
	}
	obj_print(o, 1);
	return T;
}

/* stack-dependant reader state */
typedef {
	val_t *head;
	val_t *tail;
	int quote_mode, dotpair_mode, vector_mode;
} rstate_t;

typedef {
	eater.t eater;

	char *name;
	int interactive;
	char *prompt;

	/* indicators */
	int eof, error, shebang, done;

	/* state stack */
	size_t ssize;
	rstate_t *base;
	rstate_t *state;
} reader_t;


char *atom_chars =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
	"0123456789!#$%^&*-_=+|\\/?.~<>";
char *prompt = "wisp> ";

/* Create a new reader object, passing either a string or file handle
 * for parsing. */
reader_t *reader_create (FILE *f, char *str, char *name, int interactive) {
	reader_t *r = calloc!(1, sizeof (reader_t));
	
	r->name = name;
	if (!r->name) name = "<unknown>";
	r->interactive = interactive;
	r->prompt = prompt;
	r->eof = 0;
	r->error = 0;
	r->shebang = -1 + interactive;
	r->done = 0;
	/* state stack */
	r->ssize = 32;
	r->state = calloc!(r->ssize, sizeof (rstate_t));
	r->base = r->state;

	eater.init(&r->eater, f, str);
	return r;
}

void reader_destroy (reader_t * r) {
	reset (r);
	free(r->eater.buf);
	free(r->eater.readbuf);
	free(r->base);
	free(r);
}

/* Return height of sexp stack. */
size_t stack_height (reader_t * r) {
	return (r->state - r->base);
}

/* Push new list onto the sexp stack. */
void push (reader_t * r) {
	r->state++;
	if (r->state == r->base + r->ssize)
		{
			r->ssize *= 2;
			r->base = xrealloc (r->base, sizeof (rstate_t *) * r->ssize);
			r->state = r->base + r->ssize / 2;
		}
	/* clear the state */
	r->state->quote_mode = 0;
	r->state->dotpair_mode = 0;
	r->state->vector_mode = 0;
	r->state->head = r->state->tail = newcons (NIL, NIL);
}

/* Remove top object from the sexp stack. */
val_t *pop(reader_t * r) {
	if (!r->done && stack_height (r) <= 1) {
		read_error (r, "unbalanced parenthesis");
		return err_symbol;
	}
	if (!r->done && r->state->dotpair_mode == 1) {
		read_error (r, "missing cdr object for dotted pair");
		return err_symbol;
	}
	val_t *p = CDR(r->state->head);
	setcdr(r->state->head, NIL);
	obj_release(r->state->head);
	if (r->state->vector_mode) {
		r->state--;
		val_t *v = list2vector(p);
		obj_release(p);
		return v;
	}
	r->state--;
	return p;
}

void reset_buf (reader_t * r) {
	r->eater.bufp = r->eater.buf;
	*(r->eater.bufp) = '\0';
}

/* Remove top object from the sexp stack. */
void reset (reader_t * r) {
	r->done = 1;
	while (r->state != r->base) {
		obj_release(pop (r));
	}
	reset_buf (r);
	r->eater.readbufp = r->eater.readbuf;
	r->done = 0;
}

/* Print an error message. */
void read_error (reader_t * r, char *str)
{
	fprintf (stderr, "%s:%d: %s\n", r->name, r->eater.linenum, str);
	eater.t *e = &r->eater;
	eater.consume_line(e);
	reset (r);
	r->error = 1;
}

/* Determine if top list is empty. */
int list_empty (reader_t * r)
{
	return CDR (r->state->head) == NIL;
}

void print_prompt (reader_t * r)
{
	if (r->interactive && stack_height (r) == 1)
		printf ("%s", r->prompt);
}

/* Push a new object into the current list. */
void add(reader_t *r, val_t *o) {
	if (r->state->dotpair_mode == 2) {
		/* CDR already filled. Cannot add more. */
		read_error (r, "invalid dotted pair syntax - too many objects");
		return;
	}
	if (!r->state->dotpair_mode) {
		setcdr(r->state->tail, newcons(o, NIL));
		r->state->tail = CDR(r->state->tail);
		if (r->state->quote_mode) addpop (r);
	}
	else {
		setcdr(r->state->tail, o);
		r->state->dotpair_mode = 2;
		if (r->state->quote_mode) addpop (r);
	}
}

/* Pop sexp stack and add it to the new top list. */
void addpop (reader_t * r) {
	val_t *o = pop (r);
	if (!r->error) add (r, o);
}

/* Turn string in buffer into string	*/
val_t *parse_str (reader_t * r)
{
	size_t size = r->eater.bufp - r->eater.buf;
	char *str = strings.newstr("%s", r->eater.buf);
	reset_buf (r);
	return c_str (str, size);
}

/* Turn string in buffer into atom	*/
val_t *parse_atom (reader_t * r)
{
	char *str = r->eater.buf;
	char *end;

	/* Detect integer */
	int i = OS.strtol (str, &end, 10);
	(void) i;
	if (end != str && *end == '\0')
		{
			val_t *o = c_ints (str);
			reset_buf (r);
			return o;
		}

	/* Detect float */
	int d = strtod (str, &end);
	(void) d;
	if (end != str && *end == '\0')
		{
			val_t *o = c_floats (str);
			reset_buf (r);
			return o;
		}

	/* Might be a symbol then */
	char *p = r->eater.buf;
	while (p <= r->eater.bufp) {
		if (strchr (atom_chars, *p) == NULL) {
			char *errstr = strings.newstr("%s", "invalid symbol character: X");
			errstr[strlen (errstr) - 1] = *p;
			read_error (r, errstr);
			free (errstr);
			return NIL;
		}
		p++;
	}
	val_t *o = symbol(r->eater.buf);
	reset_buf (r);
	return o;
}

/* Read a single sexp from the reader. */
val_t *read_sexp (reader_t * r)
{
	/* Check for a shebang line. */
	if (r->shebang == -1) {
		char str[2];

		eater.t *e = &r->eater;
		str[0] = eater.getc(e);
		str[1] = eater.getc(e);

		if (str[0] == '#' && str[1] == '!') {
			r->shebang = 1;
			eater.consume_line(e);
		} else {
			r->shebang = 0;
			eater.putc(e, str[1]);
			eater.putc(e, str[0]);
		}
	}
	r->done = 0;
	r->error = 0;
	push (r);
	print_prompt (r);
	while (!r->eof && !r->error && (list_empty (r) || stack_height (r) > 1)) {
		int nc;

		eater.t *e = &r->eater;
		int c = eater.getc(e);

		switch (c) {
			case EOF: { r->eof = 1; }
			case ';': {
				/* Comments */
				eater.consume_line(e);
			}
			case '.': {
				/* Dotted pair */
				nc = eater.getc(e);

				if (strchr (" \t\r\n()", nc) != NULL) {
					if (r->state->dotpair_mode > 0) {
						read_error (r, "invalid dotted pair syntax");
					}
					else if (r->state->vector_mode > 0) {
						read_error (r, "dotted pair not allowed in vector");
					}
					else {
						r->state->dotpair_mode = 1;
						eater.t *e = &r->eater;
						eater.putc(e, nc);

					}
				} else {
					/* Turn it into a decimal point. */
					eater.t *e = &r->eater;
					eater.putc(e, nc);
					eater.putc(e, '.');
					eater.putc(e, '0');

				}
			}
			case '\n': { print_prompt (r); }
			case ' ', '\t', '\r': {}
			case '(': {
				push (r);
			}
			case ')': {
				if (r->state->quote_mode)
					read_error (r, "unbalanced parenthesis");
				else if (r->state->vector_mode)
					read_error (r, "unbalanced brackets");
				else
					addpop (r);
			}
			case '[': {
				/* Vectors */
				push (r);
				r->state->vector_mode = 1;
			}
			case ']': {
				if (r->state->quote_mode)
					read_error (r, "unbalanced parenthesis");
				else if (!r->state->vector_mode)
					read_error (r, "unbalanced brackets");
				else
					addpop (r);
			}
			case '\'': {
				push (r);
				add (r, quote);
				if (!r->error) {
					r->state->quote_mode = 1;
				}
			}
			case '"': {
				eater.t *e = &r->eater;
				eater.buf_read(e, "\"");

				add (r, parse_str (r));

				/* Throw away other quote. */
				eater.getc(e);

			}
			default: {
				/* numbers and symbols */
				eater.t *e = &r->eater;
				eater.buf_append(e, c);
				eater.buf_read(e, " \t\r\n()[];");

				val_t *o = parse_atom (r);
				if (!r->error) {
					add (r, o);
				}
			}
		}
	}
	if (!r->eof && !r->error) {
		eater.t *e = &r->eater;
		eater.consume_whitespace(e);
	}
	if (r->error)
		return err_symbol;

	/* Check state */
	r->done = 1;
	if (stack_height (r) > 1 || r->state->quote_mode || r->state->dotpair_mode == 1) {
		read_error (r, "premature end of file");
		return err_symbol;
	}
	if (list_empty (r)) {
		obj_release(pop (r));
		return NIL;
	}

	val_t *wrap = pop (r);
	val_t *sexp = increfs(CAR (wrap));
	obj_release(wrap);
	return sexp;
}

val_t *cdoc_string(val_t *lst) {
	if (lst == doc_string) {
		return newstring("Return doc-string for CFUNC or SPECIAL.");
	}
	if (req_length (lst,symbol("cdoc-string"),1) == err_symbol)	{
		return err_symbol;
	}
	val_t *fo = CAR (lst);
	int evaled = 0;
	if (fo->type == SYMBOL) {
		evaled = 1;
		fo = eval (fo);
	}
	if (fo->type != CFUNC && fo->type != SPECIAL) {
		if (evaled) obj_release(fo);
		return THROW(err_wrong_type, increfs(fo));
	}
	cfunc_t *f = fo->fval;
	val_t *str = f(doc_string);
	if (evaled) obj_release(fo);
	return str;
}

val_t *lisp_apply (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Apply function to a list.");
	}
	if (req_length (lst,symbol("apply"),2) == err_symbol)	return err_symbol;
	val_t *f = CAR (lst);
	val_t *args = CAR (CDR (lst));
	if (!islist (args))
		THROW (err_wrong_type, increfs(args));
	return apply (f, args);
}

val_t *lisp_and (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Evaluate each argument until one returns nil.");
	}
	val_t *r = T;
	val_t *p = lst;
	while (iscons (p)) {
		obj_release(r);
		r = eval (CAR (p));
		if (r == err_symbol || r == NIL) {
			return r;
		}
		p = CDR (p);
	}
	if (p != NIL) {
		THROW (getsym_improper_list(), increfs(lst));
	}
	return increfs(r);
}

val_t *lisp_or (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Evaluate each argument until one doesn't return nil.");
	}
	val_t *r = NIL;
	val_t *p = lst;
	while (iscons (p)) {
		r = eval (CAR (p));
		if (r == err_symbol) {
			return r;
		}
		if (r != NIL) {
			return increfs(r);
		}
		p = CDR (p);
	}
	if (p != NIL) {
		return THROW (getsym_improper_list(), increfs(lst));
	}
	return NIL;
}

val_t *lisp_cons (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Construct a new cons cell, given car and cdr.");
	}
	if (req_length (lst,symbol("cons"),2) == err_symbol)	return err_symbol;
	return newcons (increfs(CAR (lst)), increfs(CAR (CDR (lst))));
}

val_t *lisp_quote (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return argument unevaluated.");
	}
	if (req_length (lst,symbol("quote"),1) == err_symbol)	return err_symbol;
	return increfs(CAR (lst));
}

val_t *lambda_f (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Create an anonymous function.");
	}
	if (!is_func_form (lst))
		return THROW (symbol("bad-function-form"), increfs(lst));
	return newcons (lambda, increfs(lst));
}

val_t *defun (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Define a new function.");
	}
	if (CAR(lst)->type != SYMBOL || !is_func_form (CDR (lst))) {
		return THROW (symbol("bad-function-form"), increfs(lst));
	}
	val_t *f = newcons (lambda, increfs(CDR (lst)));
	SET (CAR (lst), f);
	return increfs(CAR (lst));
}

val_t *defmacro (val_t * lst) {
	// (defmacro setq (var val) (list 'set (list 'quote var) val))
	// It is treated exactly like a function, except that arguments are
	// never evaluated and its return value is directly evaluated.
	if (lst == doc_string) {
		return newstring("Define a new macro.");
	}
	if (CAR (lst)->type != SYMBOL || !is_func_form (CDR (lst))) {
		return THROW (symbol("bad-function-form"), increfs(lst));
	}
	val_t *f = newcons (macro, increfs(CDR (lst)));
	SET (CAR (lst), f);
	return increfs(CAR (lst));
}

val_t *lisp_cdr (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return cdr element of cons cell.");
	}
	if (req_length (lst,symbol("cdr"),1) == err_symbol)	return err_symbol;
	if (CAR (lst) == NIL) return NIL;
	if (!islist (CAR (lst))) return THROW (err_wrong_type, CAR (lst));
	return increfs(CDR (CAR (lst)));
}

val_t *lisp_car (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return car element of cons cell.");
	}
	if (req_length (lst,symbol("car"),1) == err_symbol)	return err_symbol;
	if (CAR (lst) == NIL) return NIL;
	if (!islist (CAR (lst))) return THROW (err_wrong_type, CAR (lst));
	return increfs(CAR (CAR (lst)));
}

val_t *lisp_list (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return arguments as a list.");
	}
	return increfs(lst);
}

val_t *lisp_if (val_t * lst) {
	if (lst == doc_string) {
		return newstring("If conditional special form.");
	}
	if (reqm_length (lst,err_wrong_number_of_arguments,2) == err_symbol)	return err_symbol;
	val_t *r = eval (CAR (lst));
	if (r == err_symbol) {
		return r;
	}
	if (r != NIL) {
		obj_release(r);
		return eval (CAR (CDR (lst)));
	}
	return eval_body (CDR (CDR (lst)));
}

val_t *lisp_cond (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Eval car of each argument until one is true. Then eval cdr of that argument.\n");
	}
	val_t *p = lst;
	while (p != NIL) {
		if (!iscons (p)) return THROW (getsym_improper_list(), increfs(lst));
		val_t *pair = CAR (p);
		if (!iscons (pair)) return THROW (err_wrong_type, increfs(pair));
		if (!islist (CDR (pair))) return THROW (getsym_improper_list(), increfs(pair));
		if (CDR (pair) == NIL) return increfs(CAR (pair));
		if (CDR (CDR (pair)) != NIL) return THROW (symbol("bad-form"), increfs(pair));
		val_t *r = eval (CAR (pair));
		if (r != NIL) {
			obj_release(r);
			return eval (CAR (CDR (pair)));
		}
		p = CDR (p);
	}
	return NIL;
}

val_t *progn (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Eval each argument and return the eval of the last.");
	}
	return eval_body (lst);
}

val_t *let (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Create variable bindings in a new scope, and eval body in that scope.");
	}
	/* verify structure */
	if (!islist (CAR (lst))) return THROW (symbol("bad-let-form"), increfs(lst));
	val_t *vlist = CAR (lst);
	while (vlist != NIL) {
		val_t *p = CAR (vlist);
		if (!islist (p)) return THROW (symbol("bad-let-form"), increfs(lst));
		if (CAR(p)->type != SYMBOL) {
			return THROW (symbol("bad-let-form"), increfs(lst));
		}
		vlist = CDR (vlist);
	}

	val_t *p;
	p = vlist = CAR (lst);
	int len = 0;
	while (p != NIL) {
		val_t *pair = CAR (p);
		val_t *e = eval (CAR (CDR (pair)));
		if (e == err_symbol) {
			/* Undo scoping */
			p = vlist;
			while (len) {
				sympop (CAR (CAR (p)));
				p = CDR (p);
				len--;
			}
			return err_symbol;
		}
		sympush (CAR (pair), e);
		obj_release(e);
		p = CDR (p);
		len++;
	}
	val_t *r = eval_body (CDR (lst));
	p = vlist;
	while (p != NIL)
		{
			val_t *pair = CAR (p);
			sympop (CAR (pair));
			p = CDR (p);
		}
	return r;
}

val_t *lisp_while(val_t * lst) {
	if (lst == doc_string) {
		return newstring("Continually evaluate body until first argument evals nil.");
	}
	if (reqm_length (lst,symbol("while"),1) == err_symbol)	return err_symbol;
	val_t *r = NIL;
	val_t *cond = CAR (lst);
	val_t *body = CDR (lst);
	val_t *condr;
	while (true) {
		condr = eval(cond);
		if (condr == NIL) break;
		obj_release(r);
		obj_release(condr);
		if (condr == err_symbol) {
			return condr;
		}
		r = eval_body (body);
	}
	return r;
}

val_t *eq (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if both arguments are the same lisp ");
	}
	if (req_length (lst,symbol("eq"),2) == err_symbol)	return err_symbol;
	val_t *a = CAR (lst);
	val_t *b = CAR (CDR (lst));
	if (a == b)
		return T;
	return NIL;
}

val_t *eql (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if both arguments are similar.");
	}
	if (req_length (lst,symbol("eql"),2) == err_symbol)	return err_symbol;
	val_t *a = CAR (lst);
	val_t *b = CAR (CDR (lst));
	if (a->type != b->type)
		return NIL;
	switch (a->type)
		{
		case INT, FLOAT: {	return num_eq (lst); }
		case SYMBOL, CONS: {
			if (a == b) return T;
		}
		case STRING: {
			if (OSTRLEN (a) == OSTRLEN (b))
				if (memcmp (OSTR (a), OSTR (b), OSTRLEN (a)) == 0)
					return T;
		}
		case VECTOR: { return NIL; }
		case DETACH: { if (a == b) return T; }
		case CFUNC, SPECIAL: { if (((a)->fval) == ((b)->fval)) return T; }
		}
	return NIL;
}

val_t *lisp_hash (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return integer hash of ");
	}
	if (req_length (lst,symbol("hash"),1) == err_symbol)	return err_symbol;
	return c_int (obj_hash (CAR (lst)));
}

val_t *lisp_print (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Print object or sexp in parse-able form.");
	}
	if (req_length (lst,symbol("print"),1) == err_symbol)	return err_symbol;
	obj_print(CAR (lst), 1);
	return NIL;
}

val_t *lisp_set (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Store object in symbol.");
	}
	if (req_length (lst,symbol("set"),2) == err_symbol)	return err_symbol;
	if ((CAR (lst))->type != SYMBOL) {
		return THROW (err_wrong_type, newcons (symbol("set"), CAR (lst)));
	}
	SET (CAR (lst), CAR (CDR (lst)));
	return increfs(CAR (CDR (lst)));
}

val_t *lisp_value (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Get value stored in symbol.");
	}
	if (req_length (lst,symbol("value"),1) == err_symbol)	return err_symbol;
	if (!issymbol(CAR (lst)))
		THROW (err_wrong_type, newcons (symbol("value"), CAR (lst)));

	return increfs(GET (CAR (lst)));
}

val_t *symbol_name(val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return symbol name as string.");
	}
	if (req_length (lst,symbol("symbol-name"),1) == err_symbol)	return err_symbol;
	if (!issymbol(CAR (lst))) return THROW (err_wrong_type, increfs(CAR (lst)));
	return newstring(*SYMNAME(CAR (lst)));
}

val_t *lisp_concat(val_t *lst) {
	if (lst == doc_string) {
		return newstring("Concatenate two strings.");
	}
	if (req_length (lst,symbol("concat"),2) == err_symbol)	return err_symbol;
	val_t *a = CAR (lst);
	val_t *b = CAR (CDR (lst));
	if (a->type != STRING) return THROW (err_wrong_type, increfs(a));
	if (b->type != STRING) return THROW (err_wrong_type, increfs(b));

	str_t *str1 = a->val;
	str_t *str2 = b->val;
	size_t nlen = str1->len + str2->len;
	char *nraw = calloc!(nlen + 1, 1);
	memcpy(nraw, str1->raw, str1->len);
	memcpy(nraw + str1->len, str2->raw, str2->len);
	nraw[nlen] = '\0';
	return c_str(nraw, nlen);
}

val_t *nullp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is nil.");
	}
	if (req_length (lst,symbol("nullp"),1) == err_symbol)	return err_symbol;
	if (CAR (lst) == NIL)
		return T;
	return NIL;
}

val_t *funcp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is a function.");
	}
	if (req_length (lst,symbol("funcp"),1) == err_symbol)	return err_symbol;
	if (isfunc (CAR (lst)))
		return T;
	return NIL;
}

val_t *listp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is a list.");
	}
	if (req_length (lst,symbol("listp"),1) == err_symbol)	return err_symbol;
	if (islist (CAR (lst)))
		return T;
	return NIL;
}

val_t *symbolp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is a symbol.");
	}
	if (req_length (lst,symbol("symbolp"),1) == err_symbol)	return err_symbol;
	if (issymbol(CAR (lst))) {
		return T;
	}
	return NIL;
}

val_t *numberp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is a number.");
	}
	if (req_length (lst,symbol("numberp"),1) == err_symbol)	return err_symbol;
	if (isnumber (CAR (lst)))
		return T;
	return NIL;
}

val_t *stringp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is a string.");
	}
	if (req_length (lst,symbol("stringp"),1) == err_symbol)	return err_symbol;
	if (CAR(lst)->type == STRING) return T;
	return NIL;
}

val_t *integerp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is an integer.");
	}
	if (req_length (lst,symbol("integerp"),1) == err_symbol)	return err_symbol;
	if (isint (CAR (lst)))
		return T;
	return NIL;
}

val_t *floatp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is a floating-point number.");
	}
	if (req_length (lst,symbol("floatp"),1) == err_symbol)	return err_symbol;
	if (isfloat (CAR (lst)))
		return T;
	return NIL;
}

val_t *vectorp (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return t if object is a vector.");
	}
	if (req_length (lst,symbol("vectorp"),1) == err_symbol)	return err_symbol;
	if (isvector (CAR (lst)))
		return T;
	return NIL;
}

val_t *lisp_load (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Evaluate contents of a file.");
	}
	if (req_length (lst,symbol("load"),1) == err_symbol)	return err_symbol;
	val_t *str = CAR (lst);
	if (str->type != STRING) {
		return THROW (err_wrong_type, increfs(str));
	}
	char *filename = OSTR (str);
	if (!load_file (NULL, filename, 0)) {
		return THROW (symbol("load-file-error"), increfs(str));
	}
	return T;
}

val_t *lisp_read_string (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Parse a string into a sexp or list ");
	}
	if (req_length (lst,symbol("eval-string"),1) == err_symbol)	return err_symbol;
	val_t *stro = CAR (lst);
	if (stro->type != STRING) {
		return THROW (err_wrong_type, increfs(stro));
	}
	char *str = OSTR (stro);
	reader_t *r = reader_create (NULL, str, "eval-string", 0);
	val_t *sexp = read_sexp (r);
	reader_destroy (r);
	if (sexp == err_symbol)
		THROW (symbol("parse-error"), increfs(stro));
	return sexp;
}

val_t *throw (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Throw an object, and attachment, as an exception.");
	}
	return THROW (increfs(CAR (lst)), increfs(CAR (CDR (lst))));
}

val_t *catch (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Catch an exception and return attachment.");
	}
	val_t *csym = eval (CAR (lst));
	if (csym == err_symbol) {
		return csym;
	}
	val_t *body = CDR (lst);
	val_t *r = eval_body (body);
	if (r == err_symbol) {
		if (csym == err_thrown) {
			obj_release(csym);
			obj_release(err_thrown);
			return err_attach;
		}
		return err_symbol;
	}
	return r;
}

val_t *lisp_vset (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Set slot in a vector to ");
	}
	if (req_length (lst,symbol("vset"),3) == err_symbol)	return err_symbol;
	val_t *vec = CAR (lst);
	val_t *ind = CAR (CDR (lst));
	val_t *val = CAR (CDR (CDR (lst)));
	if (!isvector (vec))
		THROW (err_wrong_type, increfs(vec));
	if (!isint (ind))
		THROW (err_wrong_type, increfs(ind));
	return vset_check (vec, ind, val);
}

val_t *lisp_vget (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Get object stored in vector slot.");
	}
	if (req_length (lst,symbol("vget"),2) == err_symbol)	return err_symbol;
	val_t *vec = CAR (lst);
	val_t *ind = CAR (CDR (lst));
	if (!isvector (vec))
		THROW (err_wrong_type, increfs(vec));
	if (!isint (ind))
		THROW (err_wrong_type, increfs(ind));
	return vget_check (vec, ind);
}

val_t *lisp_vlength (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return length of the vector.");
	}
	if (req_length (lst,symbol("vlength"),1) == err_symbol)	return err_symbol;
	val_t *vec = CAR (lst);
	if (!isvector (vec))
		THROW (err_wrong_type, increfs(vec));
	return c_int (VLENGTH (vec));
}

val_t *make_vector (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Make a new vector of given length, initialized to given ");
	}
	if (req_length (lst,symbol("make-vector"),2) == err_symbol)	return err_symbol;
	val_t *len = CAR (lst);
	val_t *o = CAR (CDR (lst));
	if (!isint (len))
		THROW (err_wrong_type, increfs(len));
	return c_vec (into2int (len), o);
}

val_t *lisp_vconcat (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Concatenate two vectors.");
	}
	if (req_length (lst,symbol("vconcat2"),2) == err_symbol)	return err_symbol;
	val_t *a = CAR (lst);
	val_t *b = CAR (CDR (lst));
	if (!isvector (a))
		return THROW (err_wrong_type, increfs(a));
	if (!isvector (b))
		return THROW (err_wrong_type, increfs(b));
	return vector_concat (a, b);
}

val_t *lisp_vsub (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return subsection of vector.");
	}
	if (reqm_length (lst,symbol("subv"),2) == err_symbol)	return err_symbol;
	val_t *v = CAR (lst);
	val_t *starto = CAR (CDR (lst));
	if (!isvector (v))
		THROW (err_wrong_type, increfs(v));
	if (!isint (starto))
		THROW (err_wrong_type, increfs(starto));
	int start = into2int (starto);
	if (start >= (int) VLENGTH (v))
		THROW (symbol("bad-index"), increfs(starto));
	if (start < 0)
		THROW (symbol("bad-index"), increfs(starto));
	if (CDR (CDR (lst)) == NIL)
		{
			/* to the end */
			return vector_sub (v, start, -1);
		}
	val_t *endo = CAR (CDR (CDR (lst)));
	if (!isint (endo))
		THROW (err_wrong_type, increfs(endo));
	int end = into2int (endo);
	if (end >= (int) VLENGTH (v))
		THROW (symbol("bad-index"), increfs(endo));
	if (end < start)
		THROW (symbol("bad-index"), increfs(endo));
	return vector_sub (v, start, end);
}

val_t *lisp_refcount (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return number of reference counts to ");
	}
	if (req_length (lst,symbol("refcount"),1) == err_symbol)	return err_symbol;
	return c_int (CAR (lst)->refs);
}

val_t *lisp_eval_depth (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return the current evaluation depth.");
	}
	if (req_length (lst,symbol("eval-depth"),0) == err_symbol)	return err_symbol;
	return c_int (stack_depth);
}

val_t *lisp_max_eval_depth (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Return or set the maximum evaluation depth.");
	}
	if (reqx_length (lst,symbol("max-eval-depth"),1) == err_symbol)	return err_symbol;
	if (lst == NIL)
		return c_int (max_stack_depth);
	val_t *arg = CAR (lst);
	if (!isint (arg))
		THROW (err_wrong_type, increfs(arg));
	int new_depth = into2int (arg);
	if (new_depth < 10)
		return NIL;
	max_stack_depth = new_depth;
	return increfs(arg);
}

val_t *lisp_exit (val_t * lst) {
	if (lst == doc_string) {
		return newstring("Halt the interpreter and return given integer.");
	}
	if (reqx_length (lst,symbol("exit"),1) == err_symbol)	return err_symbol;
	if (lst == NIL)
		exit(0);
	if (!isint (CAR (lst)))
		return THROW (err_wrong_type, increfs(CAR (lst)));
	exit (into2int (CAR (lst)));
	return NULL;
}

val_t *addition (val_t * lst) {
	DOC ("Perform addition operation.");
	return arith (ADD, lst);
}

val_t *multiplication (val_t * lst) {
	DOC ("Perform multiplication operation.");
	return arith (MUL, lst);
}

val_t *subtraction (val_t * lst) {
	DOC ("Perform subtraction operation.");
	return arith (SUB, lst);
}

val_t *division (val_t * lst) {
	DOC ("Perform division operation.");
	return arith (DIV, lst);
}

val_t *num_eq (val_t * lst) {
	DOC ("Compare two numbers by =.");
	return num_cmp (EQ, lst);
}

val_t *num_lt (val_t * lst) {
	DOC ("Compare two numbers by <.");
	return num_cmp (LT, lst);
}

val_t *num_lte (val_t * lst) {
	DOC ("Compare two numbers by <=.");
	return num_cmp (LTE, lst);
}

val_t *num_gt (val_t * lst) {
	DOC ("Compare two numbers by >.");
	return num_cmp (GT, lst);
}

val_t *num_gte (val_t * lst) {
	DOC ("Compare two numbers by >=.");
	return num_cmp (GTE, lst);
}

val_t *modulus (val_t * lst) {
	DOC ("Return modulo of arguments.");
	// REQ (lst, 2, symbol("%"));
	val_t *a = CAR (lst);
	val_t *b = CAR (CDR (lst));
	if (!isint (a))
		return THROW (err_wrong_type, increfs(a));
	if (!isint (b))
		return THROW (err_wrong_type, increfs(b));
	val_t *m = c_int (0);
	mp.mpz_mod(DINT (m), DINT (a), DINT (b));
	return m;
}

/* Maths */
val_t *arith(int op, val_t * lst) {
	if (op == DIV) {
		if (reqm_length (lst,symbol("div"),2) == err_symbol)	{
			return err_symbol;
		}
	}
	int intmode = 1;
	val_t *accumz = NIL;
	val_t *accumf = NIL;
	val_t *convf = c_float (0);
	switch (op) {
		case SUB, ADD: {
			accumz = c_int (0);
			accumf = c_float (0);
		}
		case MUL, DIV: {
			accumz = c_int (1);
			accumf = c_float (1);
		}
		}
	if (op == SUB || op == DIV) {
		val_t *num = CAR (lst);
		if (isfloat (num)) {
			intmode = 0;
			mp.mpf_set(DFLOAT (accumf), DFLOAT (num));
		} else if (isint (num)) {
			mp.mpf_set_z(DFLOAT (accumf), DINT (num));
			mp.mpz_set(DINT(accumz), DINT(num));
		} else {
			obj_release(accumz);
			obj_release(accumf);
			obj_release(convf);
			return THROW (err_wrong_type, increfs(num));
		}
		if (op == SUB && CDR (lst) == NIL) {
			if (intmode) {
				obj_release(accumf);
				obj_release(convf);
				mp.mpz_neg(DINT (accumz), DINT (accumz));
				return accumz;
			}
		else
			{
				obj_release(accumz);
				obj_release(convf);
				mp.mpf_neg(DFLOAT (accumf), DFLOAT (accumf));
				return accumf;
			}
	}
			lst = CDR (lst);
		}

	while (lst != NIL) {
		val_t *num = CAR (lst);
		if (!isnumber (num)) {
			obj_release(accumz);
			obj_release(accumf);
			obj_release(convf);
			THROW (err_wrong_type, increfs(num));
		}
		/* Check divide by zero */
		if (op == DIV) {
			double dnum;
			if (isfloat (num)) {
				dnum = floato2float (num);
			} else {
				dnum = into2int (num);
			}
			if (dnum == 0) {
				obj_release(accumz);
				obj_release(accumf);
				obj_release(convf);
				THROW (symbol("divide-by-zero"), increfs(num));
			}
		}

			if (intmode)
	{
		if (isfloat (num))
		{
			intmode = 0;
			mp.mpf_set_z(DFLOAT (accumf), DINT (accumz));
			/* Fall through to !intmode */
		}
		else if (isint (num))
		{
			switch (op) {
				case ADD: { mp.mpz_add(DINT (accumz), DINT (accumz), DINT (CAR (lst))); }
				case MUL: { mp.mpz_mul(DINT (accumz), DINT (accumz), DINT (CAR (lst))); }
				case SUB: { mp.mpz_sub(DINT (accumz), DINT (accumz), DINT (CAR (lst))); }
				case DIV: { mp.mpz_div(DINT (accumz), DINT (accumz), DINT (CAR (lst))); }
			}
		}
	}
			if (!intmode)
	{
		if (isfloat (num))
			switch (op) {
				case ADD: { mp.mpf_add(DFLOAT (accumf), DFLOAT (accumf), DFLOAT (num)); }
				case MUL: { mp.mpf_mul(DFLOAT (accumf), DFLOAT (accumf), DFLOAT (num)); }
				case SUB: { mp.mpf_sub(DFLOAT (accumf), DFLOAT (accumf), DFLOAT (num)); }
				case DIV: { mp.mpf_div(DFLOAT (accumf), DFLOAT (accumf), DFLOAT (num)); }
			}
		else if (isint (num))
		{
			/* Convert to float and add. */
			mp.mpf_set_z(DFLOAT (convf), DINT (num));
			switch (op) {
				case ADD: { mp.mpf_add(DFLOAT (accumf), DFLOAT (accumf), DFLOAT (convf)); }
				case MUL: { mp.mpf_mul(DFLOAT (accumf), DFLOAT (accumf), DFLOAT (convf)); }
				case SUB: { mp.mpf_sub(DFLOAT (accumf), DFLOAT (accumf), DFLOAT (convf)); }
				case DIV: { mp.mpf_div(DFLOAT (accumf), DFLOAT (accumf), DFLOAT (convf)); }
			}
		}
}
			lst = CDR (lst);
		}
	obj_release(convf);

	/* Destroy whatever went unused. */
	if (intmode) {
		obj_release(accumf);
		return accumz;
	}
	obj_release(accumz);
	return accumf;
}



val_t *num_cmp(int cmp, val_t * lst) {
	if (reqm_length (lst,symbol("compare-func"),2) == err_symbol)	return err_symbol;
	val_t *a = CAR (lst);
	val_t *b = CAR (CDR (lst));
	if (!isnumber (a)) return THROW (err_wrong_type, increfs(a));
	if (!isnumber (b)) return THROW (err_wrong_type, increfs(b));
	int r = 0;
	int invr = 1;
	if (isint(a) && isint(b)) {
		r = mp.mpz_cmp(DINT (a), DINT (b));
	} else if (isfloat(a) && isfloat(b)) {
		r = mp.mpf_cmp(DFLOAT (a), DFLOAT (b));
	} else if (isint (a) && isfloat (b)) {
		/* Swap and handle below. */
		val_t *c = b;
		b = a;
		a = c;
		invr = -1;
	}
	if (isfloat (a) && isint (b)) {
		/* Convert down. */
		val_t *convf = c_float(0);
		mp.mpf_set_z(DFLOAT(convf), DINT(b));
		r = mp.mpf_cmp(DFLOAT (a), DFLOAT (convf));
		obj_release(convf);
	}
	r *= invr;
	switch (cmp) {
		case EQ: { r = (0 == r); }
		case LT: { r = (r < 0); }
		case LTE: { r = (r <= 0); }
		case GT: { r = (r > 0); }
		case GTE: { r = (r >= 0); }
	}
	if (r) return T;
	return NIL;
}

void *xrealloc (void *p, size_t size) {
	void *np = realloc (p, size);
	if (np == NULL) panic("realloc failed");
	return np;
}

/*
Calling a function and receiving an object is considered an act of
creation, no matter what. This object _must_ either be destroyed with
+obj_release()+ or passed _directly_ as a return value. If passed, a
CFUNC doesn't need to increase the counter, as both a decrement are
increment are needed, which cancels out.

Object returners are responsible for increasing the reference counter.
If a CFUNC references into an s-expression to return it, the counter increase is required.

When just passing an object to a function, a CFUNC doesn't need to
increment the counter. That function is responsible for the new
references it makes. There is one exception to this:
+newcons()+. Objects passed to +newcons()+ need to be incremented,
unless they come directly from another function.

The reason for this is because this is a common thing to do,
-----------
newcons (c_int (100), NIL);
-----------

The +c_int()+ function already incremented the counter, so it need not
be incremented again.

If you store a reference to an argument anywhere, you need to increase
the reference counter for that object. The +SET+ macro does this for
you (+bind+ does not).
----------
return NIL;
----------

*/