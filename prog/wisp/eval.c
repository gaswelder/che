#import hashtab.c
#import mem.c
#import mp.c
#import os/self
#import strings

enum { INT, FLOAT, STRING, SYMBOL, CONS, VECTOR, CFUNC, SPECIAL, DETACH }
enum { ADD, SUB, MUL, DIV }
enum { EQ, LT, LTE, GT, GTE }

#define SYM_CONSTANT 1

pub typedef {
	int type;
	uint32_t refs;
	union {
		void *val;
		cfunc_t *fval;
	} uval;
} object_t;

pub typedef {
	char *name;
	int props;
	object_t **vals;
	object_t **stack;
	uint32_t cnt;
} symbol_t;

pub typedef { object_t *car, *cdr; } cons_t;
pub typedef object_t *cfunc_t(object_t *);

typedef {
	object_t **v;
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
// #define DOC(str) if (lst == doc_string) return c_strs (strings.newstr("%s", str));

hashtab.hashtab_t *symbol_table = NULL;

mem.mmanager_t *mm_obj = NULL;
mem.mmanager_t *mm_cons = NULL;
mem.mmanager_t *mm_str = NULL;
mem.mmanager_t *mm_vec = NULL;

object_t *out_of_bounds = NULL;
object_t *NIL = NULL;
object_t *T = NULL;
object_t *lambda = NULL;
object_t *macro = NULL;
object_t *quote = NULL;
object_t *err_symbol = NULL;
object_t *err_thrown = NULL;
object_t *err_attach = NULL;
object_t *rest = NULL;
object_t *optional = NULL;
object_t *doc_string = NULL;
object_t *err_void_function = NULL;
object_t *err_wrong_number_of_arguments = NULL;
object_t *err_wrong_type = NULL;
object_t *err_improper_list = NULL;
object_t *err_improper_list_ending = NULL;
object_t *err_interrupt = NULL;

pub object_t *nil() {
	return NIL;
}

void DOC (const char *s) {
	(void) s;
}

bool issymbol(object_t *o) { return o->type == SYMBOL; }
bool iscons(object_t *o) { return o->type == CONS; }
bool isint(object_t *o) { return o->type == INT; }
bool isfloat(object_t *o) { return (o->type == FLOAT); }
bool isvector(object_t *o) { return o->type == VECTOR; }
bool isdetach(object_t *o) { return (o->type == DETACH); }

bool isnumber(object_t *o) { return (isint (o) || isfloat (o)); }
bool islist(object_t *o) { return (o->type == CONS || o == NIL); }

bool CONSTANTP(object_t *o) { return *SYMPROPS(o) & SYM_CONSTANT; }
bool isfunc(object_t *o) {
	return (o->type == CONS && CAR(o)->type == SYMBOL && ((CAR(o) == lambda) || (CAR(o) == macro)))
		|| (o->type == CFUNC)
		|| (o->type == SPECIAL);
}

// Increments refcount of object and returns it.
object_t *increfs(object_t *o) {
	o->refs++;
	return o;
}

object_t *CAR(object_t *o) { cons_t *x = (o)->uval.val; return x->car; }
object_t *CDR(object_t *o) { cons_t *x = (o)->uval.val; return x->cdr; }
int OPROC(object_t *o) { detach_t *x = o->uval.val; return x->proc; }
void *OREAD(object_t *o) { detach_t *x = o->uval.val; return x->read; }
mp.mpz_t *OINT(object_t *o) {
	return o->uval.val;
}
mp.mpf_t *OFLOAT(object_t *o) { return ((o)->uval.val); }
mp.mpz_t *DINT(object_t *o) {	mp.mpz_t *x = (o)->uval.val; return x; }
mp.mpf_t *DFLOAT(object_t *o) { mp.mpf_t *x = (o)->uval.val; return x; }
int VLENGTH(object_t *o) { vector_t *x = o->uval.val; return x->len; }
char **SYMNAME(object_t *so) { symbol_t *x = (so)->uval.val; return &x->name; }
int *SYMPROPS(object_t *o) { symbol_t *x = ((o)->uval.val); return &x->props; }

char *OSTR(object_t *o) {
	str_t *x = (o)->uval.val;
	return x->raw;
}
int OSTRLEN(object_t *o) {
	str_t *x = (o)->uval.val;
	return x->len;
}
char *OSTRP(object_t *o) {
	str_t *str = o->uval.val;
	if (str->print == NULL) {
		str->print = addslashes(str->raw);
	}
	return str->print;
}

void setcar(object_t *o, *car) {
	cons_t *x = o->uval.val;
	x->car = car;
}
void setcdr(object_t *o, *cdr) {
	cons_t *x = o->uval.val;
	x->cdr = cdr;
}

// void *PAIRP(void *o) {
//	 return (o->type == CONS && !islist(CDR(o)));
// }

object_t *THROW(object_t *to, *ao) {
	err_thrown = to;
	err_attach = ao;
	return err_symbol;
}

object_t *c_vec (size_t len, object_t * init) {
	object_t *o = obj_create(VECTOR);
	vector_t *v = o->uval.val;
	v->len = len;
	if (len == 0) {
		len = 1;
	}
	v->v = calloc!(len, sizeof (object_t **));
	for (size_t i = 0; i < v->len; i++) {
		v->v[i] = increfs(init);
	}
	return o;
}

object_t *list2vector (object_t * lst)
{
	int cnt = 0;
	object_t *p = lst;
	while (p != NIL)
		{
			cnt++;
			p = CDR (p);
		}
	object_t *v = c_vec (cnt, NIL);
	p = lst;
	size_t i = 0;
	while (p != NIL)
		{
			vset (v, i, increfs(CAR (p)));
			i++;
			p = CDR (p);
		}
	return v;
}

void vset (object_t * vo, size_t i, object_t * val) {
	vector_t *v = vo->uval.val;
	object_t *o = v->v[i];
	v->v[i] = val;
	obj_release(o);
}

object_t *vset_check(object_t *vo, *io, *val) {
	int i = into2int (io);
	vector_t *v = vo->uval.val;
	if (i < 0 || i >= (int) v->len) {
		return THROW (out_of_bounds, increfs(io));
	}
	vset (vo, i, increfs(val));
	return increfs(val);
}

object_t *vget (object_t * vo, size_t i)
{
	vector_t *v = vo->uval.val;
	return v->v[i];
}

object_t *vget_check (object_t * vo, object_t * io)
{
	int i = into2int (io);
	vector_t *v = vo->uval.val;
	if (i < 0 || i >= (int) v->len) {
		return THROW (out_of_bounds, increfs(io));
	}
	return increfs(vget (vo, i));
}

object_t *vector_concat (object_t * a, object_t * b)
{
	size_t al = VLENGTH (a);
	size_t bl = VLENGTH (b);
	object_t *c = c_vec (al + bl, NIL);
	size_t i;
	for (i = 0; i < al; i++)
		vset (c, i, increfs(vget (a, i)));
	for (i = 0; i < bl; i++)
		vset (c, i + al, increfs(vget (b, i)));
	return c;
}

object_t *vector_sub (object_t * vo, int start, int end)
{
	vector_t *v = vo->uval.val;
	if (end == -1)
		end = v->len - 1;
	object_t *newv = c_vec (1 + end - start, NIL);
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
object_t *req_length(object_t * lst, object_t * thr, int n) {
	/* TODO detect loops? */
	int cnt = 0;
	object_t *p = lst;
	while (p != NIL) {
			cnt++;
			p = CDR (p);
			if (!islist (p)) {
				obj_release(thr);
				return THROW (getsym_improper_list(), lst);
			}
			if (cnt > n) return THROW (err_wrong_number_of_arguments, thr);
		}
	if (cnt != n) return THROW (err_wrong_number_of_arguments, thr);
	return T;
}

/* min */
object_t *reqm_length (object_t * lst, object_t * thr, int n) {
	/* TODO detect loops? */
	int cnt = 0;
	object_t *p = lst;
	while (p != NIL)
		{
			cnt++;
			p = CDR (p);
			if (!islist (p))
	{
		obj_release(thr);
		return THROW (getsym_improper_list(), lst);
	}
			if (cnt >= n)
	return T;
		}
	if (cnt < n)
		return THROW (err_wrong_number_of_arguments, thr);
	return T;
}

/* max */
object_t *reqx_length (object_t * lst, object_t * thr, int n) {
	/* TODO detect loops? */
	int cnt = 0;
	object_t *p = lst;
	while (p != NIL)
		{
			cnt++;
			p = CDR (p);
			if (!islist (p))
	{
		obj_release(thr);
		return THROW (getsym_improper_list(), lst);
	}
			if (cnt > n) return THROW (err_wrong_number_of_arguments, thr);
		}
	return T;
}

/* Verifies that list has form of a function. */
int is_func_form (object_t * lst)
{
	if (!islist (CAR (lst)))
		return 0;
	return is_var_list (CAR (lst));
}

/* Verifies that list is made only of symbols. */
int is_var_list (object_t * lst) {
	int rest_cnt = -1;
	while (lst != NIL) {
		object_t *car = CAR(lst);
		/* &rest must be the second to last item */
		if (rest_cnt >= 0) {
			rest_cnt--;
			if (rest_cnt < 0) return 0;
			if (car == rest) return 0;
		}
		if (car->type != SYMBOL) return 0;
		if (car == rest) rest_cnt = 1;
		lst = CDR (lst);
	}
	if (rest_cnt == 1) return 0;
	return 1;
}

// /* Determine if list is proper. */
// /* Determine if list is proper (ends with NIL) */
// object_t *properlistp (object_t * lst)
// {
// 	if (lst == NIL)
// 		return T;
// 	if (!iscons (lst))
// 		return NIL;
// 	return properlistp (CDR (lst));
// }



object_t *GET(object_t *so) {
	symbol_t *x = so->uval.val;
	return *x->vals;
}

pub void SET(object_t *so, *o) {
	obj_release(GET(so));
	increfs(o);
	symbol_t *x = (so)->uval.val;
	*x->vals = o;
}

// Binds val to sym.
void bind(char *name, object_t *val) {
	object_t *sym = symbol(name);
	symbol_t *x = sym->uval.val;
	obj_release(*x->vals);
	*x->vals = val;
}

symbol_t *symbol_create() {
	size_t cnt = 8;
	void *x = calloc!(cnt, sizeof(object_t *));
	symbol_t *s = calloc!(1, sizeof (symbol_t));
	s->cnt = cnt;
	s->vals = x;
	s->stack = x;
	return s;
}

/* Dynamic scoping */
void sympush(object_t *so, object_t *o) {
	symbol_t *s = so->uval.val;
	s->vals++;
	// if (s->vals == s->cnt + s->stack) {
	//		 size_t n = s->vals - s->stack;
	//		 s->cnt *= 2;
	//		 s->stack = xrealloc (s->stack, s->cnt * sizeof (object_t *));
	//		 s->vals = s->stack + n;
	//	 }
	*s->vals = o;
	increfs(o);
}

/* Dynamic scoping */
void sympop (object_t * so) {
	obj_release(GET (so));
	symbol_t *s = (symbol_t *) ((so)->uval.val);
	s->vals--;
}

/*
Calling a function and receiving an object is considered an act of
creation, no matter what. This object _must_ either be destroyed with
+obj_release()+ or passed _directly_ as a return value. If passed, a
CFUNC doesn't need to increase the counter, as both a decrement are
increment are needed, which cancels out.

Object returners are responsible for increasing the reference
counter. The +increfs+ macro is provided for this. If a CFUNC references
into an s-expression to return it, the counter increase is required.

Be mindful of the order in which you increment and decrement a
counter. If you decrement the root first, then increment a child
object in that root, the child object may not exist anymore.

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

// Returns the symbol with the given name, creating it if needed.
pub object_t *symbol(char *name) {
	object_t *o = hashtab.ht_search(symbol_table, name, strlen (name));
	if (o) {
		return o;
	}
	o = obj_create(SYMBOL);
	*SYMNAME(o) = strings.newstr("%s", name);
	symbol_t *x = (o)->uval.val;
	*x->vals = NIL;
	if (name[0] == ':') {
		SET (o, o);
	}
	hashtab.ht_insert(symbol_table, *SYMNAME(o), strlen(*SYMNAME(o)), o, sizeof(object_t *));
	return o;
}

// /* Used for debugging: print string followed by	*/
// void DB_OP(int str, o) {
//	 printf(str); obj_print(o,1);
// }

object_t *obj_create(int type) {
	object_t *o = mem.mm_alloc(mm_obj);
	o->type = type;
	o->refs++;
	switch (type) {
		case INT: { o->uval.val = calloc!(1, sizeof(mp.mpz_t *)); }
		case FLOAT: { o->uval.val = calloc!(1, sizeof(mp.mpf_t *)); }
		case CONS: { o->uval.val = mem.mm_alloc(mm_cons); }
		case SYMBOL: { o->uval.val = symbol_create (); }
		case STRING: { o->uval.val = mem.mm_alloc(mm_str); }
		case VECTOR: { o->uval.val = mem.mm_alloc(mm_vec); }
		case DETACH: { o->uval.val = calloc!(1, sizeof (detach_t)); }
		case CFUNC, SPECIAL: {}
	}
	return o;
}

void obj_release(object_t *o) {
	if (o == NULL || o->type == SYMBOL) {
		return;
	}
	o->refs--;
	if (o->refs > 0) {
		return;
	}
	switch (o->type) {
		case FLOAT, INT: {
			free(o->uval.val);
		}
		case STRING: {
			str_t *str = o->uval.val;
			free(str->raw);
			if (str->print != NULL) {
				free(str->print);
			}
			mem.mm_free(mm_str, str);
		}
		case CONS: {
			obj_release(CAR (o));
			obj_release(CDR (o));
			cons_t *c = o->uval.val;
			mem.mm_free (mm_cons, c);
		}
		case VECTOR: {
			vector_t *v = o->uval.val;
			for (size_t i = 0; i < v->len; i++) {
				obj_release(v->v[i]);
			}
			free(v->v);
			mem.mm_free(mm_vec, v);
		}
		case DETACH: {
			detach_t *d = ((o)->uval.val);
			reader_destroy (d->read);
			OS.close (d->in);
			OS.close (d->out);
			free (((o)->uval.val));
		}
		case CFUNC, SPECIAL: {}
	}
	mem.mm_free(mm_obj, o);
}

pub object_t *newcons(object_t *car, *cdr) {
	object_t *o = obj_create (CONS);
	setcar(o, car);
	setcdr(o, cdr);
	return o;
}

object_t *newfunc(cfunc_t f) {
	object_t *o = obj_create(CFUNC);
	o->uval.fval = f;
	return o;
}

object_t *c_special(cfunc_t f) {
	object_t *o = obj_create(SPECIAL);
	o->uval.fval = f;
	return o;
}

// Prints object o to stdout.
void obj_print(object_t *o, bool newline) {
	switch (o->type) {
		case CONS: {
			printf("(");
			object_t *p = o;
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
			symbol_t *x = ((o)->uval.val);
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

void vec_print(object_t * vo) {
	vector_t *v = vo->uval.val;
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

uint32_t str_hash (object_t * o) {
	return hash (OSTR (o), OSTRLEN (o));
}

uint32_t int_hash(object_t * o) {
	char str[100] = {};
	mp.mpz_sprint(str, DINT(o));
	return hash(str, strlen(str));
}

uint32_t float_hash (object_t * o) {
	char str[100] = {};
	mp.mpf_sprint(str, DFLOAT(o));
	return hash(str, strlen(str));
}

uint32_t vector_hash (object_t * o)
{
	uint32_t accum = 0;
	vector_t *v = ((o)->uval.val);
	size_t i;
	for (i = 0; i < v->len; i++)
		accum ^= obj_hash (v->v[i]);
	return accum;
}

uint32_t cons_hash (object_t * o)
{
	return obj_hash (CAR (o)) ^ obj_hash (CDR (o));
}

uint32_t symbol_hash (object_t * o) {
	char *n = *SYMNAME(o);
	return hash(n, strlen(n));
}

uint32_t obj_hash (object_t * o) {
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
			return hash (((o)->uval.val), sizeof (void *));
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





pub object_t *getsym_improper_list() {
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
	object_t *obj = o;
	obj->type = SYMBOL;
	obj->refs = 0;
	obj->uval.fval = NULL;
	obj->uval.val = NIL;
}

void vector_clear (void *o) {
	vector_t *v = o;
	v->v = NULL;
	v->len = 0;
}

pub void wisp_init() {
	mm_obj = mem.mm_create (sizeof (object_t), &object_clear);
	mm_cons = mem.mm_create (sizeof (cons_t), &cons_clear);
	mm_str = mem.mm_create (sizeof (str_t), &str_clear);
	mm_vec = mem.mm_create (sizeof (vector_t), &vector_clear);

	symbol_table = hashtab.new();
	/* Set up t and nil constants. The SET macro won't work until NIL is set. */
	NIL = symbol("nil");
	symbol_t *x = NIL->uval.val;
	*x->vals = NIL;
	*SYMPROPS (NIL) |= SYM_CONSTANT;
	T = symbol("t");
	*SYMPROPS (T) |= SYM_CONSTANT;
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
	SET (symbol("wisproot"), c_strs (strings.newstr("%s", wisproot)));

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
pub bool load_file(FILE *fid, char *filename, int interactive) {
	if (fid == NULL) {
		fid = fopen(filename, "r");
		if (fid == NULL) return false;
	}
	reader_t *r = reader_create (fid, NULL, filename, interactive);
	while (!r->eof) {
		object_t *sexp = read_sexp(r);
		if (sexp == err_symbol) {
			continue;
		}
		stack_depth = 0;
		object_t *ret = eval (sexp);
		if (ret == err_symbol) {
			printf ("error: ");
			object_t *c = newcons (err_thrown, newcons (err_attach, NIL));
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

object_t *eval_list (object_t * lst) {
	if (lst == NIL) {
		return NIL;
	}
	if (!iscons (lst)) {
		return THROW(err_improper_list_ending, increfs(lst));
	}
	object_t *car = eval (CAR (lst));
	if (car == err_symbol) {
		return car;
	}
	object_t *cdr = eval_list (CDR (lst));
	if (cdr == err_symbol) {
		obj_release(car);
		return err_symbol;
	}
	return newcons (car, cdr);
}

object_t *eval_body (object_t * body) {
	object_t *r = NIL;
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

object_t *assign_args (object_t * vars, object_t * vals)
{
	int optional_mode = 0;
	int cnt = 0;
	object_t *orig_vars = vars;
	while (vars != NIL) {
		object_t *var = CAR (vars);
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
		while (cnt > 0)
			{
				sympop (CAR (orig_vars));
				orig_vars = CDR (orig_vars);
				cnt--;
			}
		return THROW (err_wrong_number_of_arguments, NIL);
	}
			else if (optional_mode && vals == NIL)
	{
		sympush (var, NIL);
	}
			else {
				object_t *val = CAR (vals);
				sympush (var, val);
				cnt++;
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

void unassign_args (object_t * vars)
{
	if (vars == NIL)
		return;
	object_t *var = CAR (vars);
	if (var != rest && var != optional)
		sympop (var);
	unassign_args (CDR (vars));
}

object_t *eval(object_t * o) {
	/* Check for interrupts. */
	if (interrupt) {
		interrupt = 0;
		return THROW (err_interrupt, c_strs (strings.newstr("%s", "interrupted")));
	}
	if (o->type != CONS && o->type != SYMBOL) {
		return increfs(o);
	}
	if (o->type == SYMBOL) {
		return increfs(GET (o));
	}

	/* Find the function. */
	object_t *f = eval (CAR (o));
	if (f == err_symbol) {
		return f;
	}
	object_t *extrao = NIL;
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
	object_t *args = CDR (o);
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

	object_t *ret = apply (f, args);
	stack_depth--;
	obj_release(f);
	obj_release(args);
	obj_release(extrao);		/* vector as function */
	return ret;
}

object_t *apply(object_t *f, *args) {
	if (f->type == CFUNC || f->type == SPECIAL) {
		cfunc_t *cf = f->uval.fval;
		return cf(args);
	}
	/* list form */
	object_t *vars = CAR (CDR (f));
	object_t *assr = assign_args (vars, args);
	if (assr == err_symbol) {
		err_attach = increfs(args);
		return err_symbol;
	}
	object_t *r;
	if (CAR (f) == lambda) {
		r = eval_body (CDR (CDR (f)));
	} else {
		object_t *body = eval_body (CDR (CDR (f)));
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



object_t *c_str (char *str, size_t len) {
	object_t *o = obj_create(STRING);
	str_t *x = o->uval.val;
	x->raw = str;
	x->len = len;
	return o;
}

pub object_t *c_strs (char *str) {
	return c_str (str, strlen (str));
}

object_t *c_ints(char *nstr) {
	object_t *o = obj_create(INT);
	mp.mpz_t *z = o->uval.val;
	mp.mpz_set_str(z, nstr);
	return o;
}

object_t *c_int(int n) {
	object_t *o = obj_create (INT);
	mp.mpz_set_ui(o->uval.val, n);
	return o;
}

object_t *c_floats(char *fstr) {
	object_t *o = obj_create (FLOAT);
	mp.mpf_t *f = o->uval.val;
	mp.mpf_set_str(f, fstr);
	return o;
}

object_t *c_float(double d) {
	object_t *o = obj_create (FLOAT);
	mp.mpf_set_d(o->uval.val, d);
	return o;
}

int into2int(object_t *into) {
	return mp.mpz_get_si(DINT(into));
}

double floato2float(object_t *floato) {
	return mp.mpf_get_d(DFLOAT(floato));
}

object_t *parent_detach = NULL;

uint8_t detach_hash (object_t * o)
{
	int proc = OPROC (o);
	return hash (&proc, sizeof (int));
}

void detach_print (object_t * o)
{
	int proc = OPROC (o);
	printf ("<detach %d>", proc);
}

object_t *c_detach (object_t * o) {
	if (o->type == SYMBOL) o = GET (o);
	object_t *dob = obj_create (DETACH);
	detach_t *d = ((dob)->uval.val);
	int pipea[2];
	int pipeb[2];
	if (OS.pipe (pipea) != 0)
		THROW (symbol("detach-pipe-error"), c_strs (strings.newstr("%s", strerror (errno))));
	if (OS.pipe (pipeb) != 0)
		THROW (symbol("detach-pipe-error"), c_strs (strings.newstr("%s", strerror (errno))));
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
			object_t *f = newcons (o, NIL);
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

object_t *lisp_detach(object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Create process detachment."));
	}
	if (req_length(lst, symbol("detach"), 1) == err_symbol) {
		return err_symbol;
	}
	return c_detach (CAR (lst));
}

object_t *lisp_receive (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Get an object from the detached process."));
	}
	if (req_length (lst,symbol("receive"),1) == err_symbol)	return err_symbol;
	object_t *d = CAR (lst);
	if (!isdetach (d)) return THROW (err_wrong_type, increfs(d));
	reader_t *r = OREAD (d);
	return read_sexp (r);
}

object_t *lisp_send (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Send an object to the parent process."));
	}
	if (req_length (lst,symbol("send"),1) == err_symbol)	return err_symbol;
	object_t *o = CAR (lst);
	if (parent_detach == NULL || parent_detach == NIL) {
		return THROW (symbol("send-from-non-detachment"), increfs(o));
	}
	obj_print(o, 1);
	return T;
}

/* stack-dependant reader state */
typedef {
	object_t *head;
	object_t *tail;
	int quote_mode, dotpair_mode, vector_mode;
} rstate_t;

/* the reader object */
typedef {
	/* source */
	FILE *fid;
	char *str;

	/* meta */
	char *name;
	int interactive;
	char *prompt;

	/** reader state **/
	uint32_t linecnt;
	char *strp;

	/* atom read buffer */
	char *buf, *bufp;
	size_t buflen;

	/* read buffer */
	int *readbuf, *readbufp;
	size_t readbuflen;

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
reader_t *reader_create (FILE * fid, char *str, char *name, int interactive) {
	reader_t *r = calloc!(1, sizeof (reader_t));
	r->fid = fid;
	r->strp = r->str = str;
	r->name = name;
	if (!r->name) name = "<unknown>";
	r->interactive = interactive;
	r->prompt = prompt;
	r->linecnt = 1;
	r->eof = 0;
	r->error = 0;
	r->shebang = -1 + interactive;
	r->done = 0;

	/* read buffers */
	r->buflen = 1024;
	r->buf = calloc!(r->buflen + 1, 1);
	r->bufp = r->buf;
	r->readbuflen = 8;
	r->readbuf = calloc!(r->readbuflen, sizeof (int));
	r->readbufp = r->readbuf;

	/* state stack */
	r->ssize = 32;
	r->state = calloc!(r->ssize, sizeof (rstate_t));
	r->base = r->state;
	return r;
}

void reader_destroy (reader_t * r) {
	reset (r);
	free(r->buf);
	free(r->readbuf);
	free(r->base);
	free(r);
}

/* Read next character in the stream. */
int reader_getc (reader_t * r) {
	int c;
	if (r->readbufp > r->readbuf)
		{
			c = *(r->readbufp);
			r->readbufp--;
			return c;
		}
	if (r->str != NULL)
		{
			c = *(r->strp);
			if (c != '\0')
	r->strp++;
			else
	return EOF;
		}
	else
		c = fgetc (r->fid);
	return c;
}

/* Unread a byte. */
void reader_putc (reader_t * r, int c) {
	r->readbufp++;
	if (r->readbufp == r->readbuf + r->readbuflen)
		{
			r->readbuflen *= 2;
			r->readbuf = xrealloc (r->readbuf, sizeof (int) * r->readbuflen);
			r->readbufp = r->readbuf + r->readbuflen / 2;
		}
	*(r->readbufp) = c;
}

/* Consume remaining whitespace on line, including linefeed. */
void consume_whitespace (reader_t * r) {
	int c = reader_getc (r);
	while (strchr (" \t\r", c) != NULL)
		c = reader_getc (r);
	if (c != '\n')
		reader_putc (r, c);
}

/* Consume remaining characters on line, including linefeed. */
void consume_line (reader_t * r) {
	int c = reader_getc (r);
	while (c != '\n' && c != EOF)
		c = reader_getc (r);
	if (c != '\n')
		reader_putc (r, c);
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
object_t *pop(reader_t * r) {
	if (!r->done && stack_height (r) <= 1) {
		read_error (r, "unbalanced parenthesis");
		return err_symbol;
	}
	if (!r->done && r->state->dotpair_mode == 1) {
		read_error (r, "missing cdr object for dotted pair");
		return err_symbol;
	}
	object_t *p = CDR(r->state->head);
	setcdr(r->state->head, NIL);
	obj_release(r->state->head);
	if (r->state->vector_mode) {
		r->state--;
		object_t *v = list2vector(p);
		obj_release(p);
		return v;
	}
	r->state--;
	return p;
}

void reset_buf (reader_t * r) {
	r->bufp = r->buf;
	*(r->bufp) = '\0';
}

/* Remove top object from the sexp stack. */
void reset (reader_t * r) {
	r->done = 1;
	while (r->state != r->base) {
		obj_release(pop (r));
	}
	reset_buf (r);
	r->readbufp = r->readbuf;
	r->done = 0;
}

/* Print an error message. */
void read_error (reader_t * r, char *str)
{
	fprintf (stderr, "%s:%d: %s\n", r->name, r->linecnt, str);
	consume_line (r);
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
void add(reader_t *r, object_t *o) {
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
	object_t *o = pop (r);
	if (!r->error) add (r, o);
}

/* Append character to buffer. */
void buf_append (reader_t * r, char c) {
	if (r->bufp == r->buf + r->buflen) {
		r->buflen *= 2;
		r->buf = xrealloc (r->buf, r->buflen + 1);
		r->bufp = r->buf + r->buflen / 2;
	}
	*(r->bufp) = c;
	*(r->bufp + 1) = '\0';
	r->bufp++;
}

/* Load into buffer until character, ignoring escaped ones. */
int buf_read (reader_t * r, char *halt) {
	int c = reader_getc(r);
	int esc = 0;
	if (c == '\\') {
		c = reader_getc (r);
		esc = 1;
	}
	while ((esc || strchr (halt, c) == NULL) && (c != EOF)) {
		buf_append (r, c);
		c = reader_getc (r);
		esc = 0;
		if (c == '\\') {
			c = reader_getc (r);
			esc = 1;
		}
	}
	reader_putc (r, c);
	return !esc;
}

/* Turn string in buffer into string	*/
object_t *parse_str (reader_t * r)
{
	size_t size = r->bufp - r->buf;
	char *str = strings.newstr("%s", r->buf);
	reset_buf (r);
	return c_str (str, size);
}

/* Turn string in buffer into atom	*/
object_t *parse_atom (reader_t * r)
{
	char *str = r->buf;
	char *end;

	/* Detect integer */
	int i = OS.strtol (str, &end, 10);
	(void) i;
	if (end != str && *end == '\0')
		{
			object_t *o = c_ints (str);
			reset_buf (r);
			return o;
		}

	/* Detect float */
	int d = strtod (str, &end);
	(void) d;
	if (end != str && *end == '\0')
		{
			object_t *o = c_floats (str);
			reset_buf (r);
			return o;
		}

	/* Might be a symbol then */
	char *p = r->buf;
	while (p <= r->bufp)
		{
			if (strchr (atom_chars, *p) == NULL)
	{
		char *errstr = strings.newstr("%s", "invalid symbol character: X");
		errstr[strlen (errstr) - 1] = *p;
		read_error (r, errstr);
		free (errstr);
		return NIL;
	}
			p++;
		}
	object_t *o = symbol(r->buf);
	reset_buf (r);
	return o;
}

/* Read a single sexp from the reader. */
object_t *read_sexp (reader_t * r)
{
	/* Check for a shebang line. */
	if (r->shebang == -1)
		{
			char str[2];
			str[0] = reader_getc (r);
			str[1] = reader_getc (r);
			if (str[0] == '#' && str[1] == '!')
	{
		/* Looks like a she-bang line. */
		r->shebang = 1;
		consume_line (r);
	}
			else
	{
		r->shebang = 0;
		reader_putc (r, str[1]);
		reader_putc (r, str[0]);
	}
		}

	r->done = 0;
	r->error = 0;
	push (r);
	print_prompt (r);
	while (!r->eof && !r->error && (list_empty (r) || stack_height (r) > 1))
		{
			int nc;
			int c = reader_getc (r);
			switch (c)
	{
	case EOF: { r->eof = 1; }

		/* Comments */
	case ';': { consume_line (r); }

		/* Dotted pair */
	case '.': {
		nc = reader_getc (r);
		if (strchr (" \t\r\n()", nc) != NULL)
			{
				if (r->state->dotpair_mode > 0)
		read_error (r, "invalid dotted pair syntax");
				else if (r->state->vector_mode > 0)
		read_error (r, "dotted pair not allowed in vector");
				else
		{
			r->state->dotpair_mode = 1;
			reader_putc (r, nc);
		}
			}
		else {
			/* Turn it into a decimal point. */
			reader_putc (r, nc);
			reader_putc (r, '.');
			reader_putc (r, '0');
		}
	}

		/* Whitespace */
	case '\n': {
		r->linecnt++;
		print_prompt (r);
	}
	case ' ', '\t', '\r': {}
	

		/* Parenthesis */
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

		/* Vectors */
	case '[': {
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

		/* Quoting */
	case '\'': {
		push (r);
		add (r, quote);
		if (!r->error)
			r->state->quote_mode = 1;
	}

		/* strings */
	case '"': {
		buf_read (r, "\"");
		add (r, parse_str (r));
		reader_getc (r);	/* Throw away other quote. */
	}

		/* numbers and symbols */
	default: {
		buf_append (r, c);
		buf_read (r, " \t\r\n()[];");
		object_t *o = parse_atom (r);
		if (!r->error)
			add (r, o);
	}
	}
		}
	if (!r->eof && !r->error)
		consume_whitespace (r);
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

	object_t *wrap = pop (r);
	object_t *sexp = increfs(CAR (wrap));
	obj_release(wrap);
	return sexp;
}



object_t *cdoc_string(object_t *lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return doc-string for CFUNC or SPECIAL."));
	}
	if (req_length (lst,symbol("cdoc-string"),1) == err_symbol)	return err_symbol;
	object_t *fo = CAR (lst);
	int evaled = 0;
	if (fo->type == SYMBOL) {
		evaled = 1;
		fo = eval (fo);
	}
	if (fo->type != CFUNC && fo->type != SPECIAL) {
		if (evaled) obj_release(fo);
		return THROW(err_wrong_type, increfs(fo));
	}
	cfunc_t *f = fo->uval.fval;
	object_t *str = f(doc_string);
	if (evaled) obj_release(fo);
	return str;
}

object_t *lisp_apply (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Apply function to a list."));
	}
	if (req_length (lst,symbol("apply"),2) == err_symbol)	return err_symbol;
	object_t *f = CAR (lst);
	object_t *args = CAR (CDR (lst));
	if (!islist (args))
		THROW (err_wrong_type, increfs(args));
	return apply (f, args);
}

object_t *lisp_and (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Evaluate each argument until one returns nil."));
	}
	object_t *r = T;
	object_t *p = lst;
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

object_t *lisp_or (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Evaluate each argument until one doesn't return nil."));
	}
	object_t *r = NIL;
	object_t *p = lst;
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

object_t *lisp_cons (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Construct a new cons cell, given car and cdr."));
	}
	if (req_length (lst,symbol("cons"),2) == err_symbol)	return err_symbol;
	return newcons (increfs(CAR (lst)), increfs(CAR (CDR (lst))));
}

object_t *lisp_quote (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return argument unevaluated."));
	}
	if (req_length (lst,symbol("quote"),1) == err_symbol)	return err_symbol;
	return increfs(CAR (lst));
}

object_t *lambda_f (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Create an anonymous function."));
	}
	if (!is_func_form (lst))
		THROW (symbol("bad-function-form"), increfs(lst));
	return newcons (lambda, increfs(lst));
}

object_t *defun (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Define a new function."));
	}
	if (CAR(lst)->type != SYMBOL || !is_func_form (CDR (lst))) {
		return THROW (symbol("bad-function-form"), increfs(lst));
	}
	object_t *f = newcons (lambda, increfs(CDR (lst)));
	SET (CAR (lst), f);
	return increfs(CAR (lst));
}

object_t *defmacro (object_t * lst) {
	// (defmacro setq (var val) (list 'set (list 'quote var) val))
	// It is treated exactly like a function, except that arguments are
	// never evaluated and its return value is directly evaluated.
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Define a new macro."));
	}
	if (CAR (lst)->type != SYMBOL || !is_func_form (CDR (lst))) {
		return THROW (symbol("bad-function-form"), increfs(lst));
	}
	object_t *f = newcons (macro, increfs(CDR (lst)));
	SET (CAR (lst), f);
	return increfs(CAR (lst));
}

object_t *lisp_cdr (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return cdr element of cons cell."));
	}
	if (req_length (lst,symbol("cdr"),1) == err_symbol)	return err_symbol;
	if (CAR (lst) == NIL) return NIL;
	if (!islist (CAR (lst))) return THROW (err_wrong_type, CAR (lst));
	return increfs(CDR (CAR (lst)));
}

object_t *lisp_car (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return car element of cons cell."));
	}
	if (req_length (lst,symbol("car"),1) == err_symbol)	return err_symbol;
	if (CAR (lst) == NIL) return NIL;
	if (!islist (CAR (lst))) return THROW (err_wrong_type, CAR (lst));
	return increfs(CAR (CAR (lst)));
}

object_t *lisp_list (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return arguments as a list."));
	}
	return increfs(lst);
}

object_t *lisp_if (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "If conditional special form."));
	}
	if (reqm_length (lst,err_wrong_number_of_arguments,2) == err_symbol)	return err_symbol;
	object_t *r = eval (CAR (lst));
	if (r == err_symbol) {
		return r;
	}
	if (r != NIL) {
		obj_release(r);
		return eval (CAR (CDR (lst)));
	}
	return eval_body (CDR (CDR (lst)));
}

object_t *lisp_cond (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Eval car of each argument until one is true. Then eval cdr of that argument.\n"));
	}
	object_t *p = lst;
	while (p != NIL) {
		if (!iscons (p)) return THROW (getsym_improper_list(), increfs(lst));
		object_t *pair = CAR (p);
		if (!iscons (pair)) return THROW (err_wrong_type, increfs(pair));
		if (!islist (CDR (pair))) return THROW (getsym_improper_list(), increfs(pair));
		if (CDR (pair) == NIL) return increfs(CAR (pair));
		if (CDR (CDR (pair)) != NIL) return THROW (symbol("bad-form"), increfs(pair));
		object_t *r = eval (CAR (pair));
		if (r != NIL) {
			obj_release(r);
			return eval (CAR (CDR (pair)));
		}
		p = CDR (p);
	}
	return NIL;
}

object_t *progn (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Eval each argument and return the eval of the last."));
	}
	return eval_body (lst);
}

object_t *let (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Create variable bindings in a new scope, and eval body in that scope."));
	}
	/* verify structure */
	if (!islist (CAR (lst))) return THROW (symbol("bad-let-form"), increfs(lst));
	object_t *vlist = CAR (lst);
	while (vlist != NIL) {
		object_t *p = CAR (vlist);
		if (!islist (p)) return THROW (symbol("bad-let-form"), increfs(lst));
		if (CAR(p)->type != SYMBOL) {
			return THROW (symbol("bad-let-form"), increfs(lst));
		}
		vlist = CDR (vlist);
	}

	object_t *p;
	p = vlist = CAR (lst);
	int cnt = 0;
	while (p != NIL) {
		object_t *pair = CAR (p);
		object_t *e = eval (CAR (CDR (pair)));
		if (e == err_symbol) {
			/* Undo scoping */
			p = vlist;
			while (cnt) {
				sympop (CAR (CAR (p)));
				p = CDR (p);
				cnt--;
			}
			return err_symbol;
		}
		sympush (CAR (pair), e);
		obj_release(e);
		p = CDR (p);
		cnt++;
	}
	object_t *r = eval_body (CDR (lst));
	p = vlist;
	while (p != NIL)
		{
			object_t *pair = CAR (p);
			sympop (CAR (pair));
			p = CDR (p);
		}
	return r;
}

object_t *lisp_while(object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Continually evaluate body until first argument evals nil."));
	}
	if (reqm_length (lst,symbol("while"),1) == err_symbol)	return err_symbol;
	object_t *r = NIL;
	object_t *cond = CAR (lst);
	object_t *body = CDR (lst);
	object_t *condr;
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

object_t *eq (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if both arguments are the same lisp "));
	}
	if (req_length (lst,symbol("eq"),2) == err_symbol)	return err_symbol;
	object_t *a = CAR (lst);
	object_t *b = CAR (CDR (lst));
	if (a == b)
		return T;
	return NIL;
}

object_t *eql (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if both arguments are similar."));
	}
	if (req_length (lst,symbol("eql"),2) == err_symbol)	return err_symbol;
	object_t *a = CAR (lst);
	object_t *b = CAR (CDR (lst));
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
		case CFUNC, SPECIAL: { if (((a)->uval.fval) == ((b)->uval.fval)) return T; }
		}
	return NIL;
}

object_t *lisp_hash (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return integer hash of "));
	}
	if (req_length (lst,symbol("hash"),1) == err_symbol)	return err_symbol;
	return c_int (obj_hash (CAR (lst)));
}

object_t *lisp_print (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Print object or sexp in parse-able form."));
	}
	if (req_length (lst,symbol("print"),1) == err_symbol)	return err_symbol;
	obj_print(CAR (lst), 1);
	return NIL;
}

object_t *lisp_set (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Store object in symbol."));
	}
	if (req_length (lst,symbol("set"),2) == err_symbol)	return err_symbol;
	if ((CAR (lst))->type != SYMBOL) {
		return THROW (err_wrong_type, newcons (symbol("set"), CAR (lst)));
	}
	if (CONSTANTP (CAR (lst)))
		return THROW (symbol("setting-constant"), CAR (lst));

	SET (CAR (lst), CAR (CDR (lst)));
	return increfs(CAR (CDR (lst)));
}

object_t *lisp_value (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Get value stored in symbol."));
	}
	if (req_length (lst,symbol("value"),1) == err_symbol)	return err_symbol;
	if (!issymbol(CAR (lst)))
		THROW (err_wrong_type, newcons (symbol("value"), CAR (lst)));

	return increfs(GET (CAR (lst)));
}

object_t *symbol_name(object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return symbol name as string."));
	}
	if (req_length (lst,symbol("symbol-name"),1) == err_symbol)	return err_symbol;
	if (!issymbol(CAR (lst))) return THROW (err_wrong_type, increfs(CAR (lst)));
	return c_strs(strings.newstr("%s", *SYMNAME(CAR (lst))));
}

object_t *lisp_concat(object_t *lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Concatenate two strings."));
	}
	if (req_length (lst,symbol("concat"),2) == err_symbol)	return err_symbol;
	object_t *a = CAR (lst);
	object_t *b = CAR (CDR (lst));
	if (a->type != STRING) return THROW (err_wrong_type, increfs(a));
	if (b->type != STRING) return THROW (err_wrong_type, increfs(b));

	str_t *str1 = a->uval.val;
	str_t *str2 = b->uval.val;
	size_t nlen = str1->len + str2->len;
	char *nraw = calloc!(nlen + 1, 1);
	memcpy(nraw, str1->raw, str1->len);
	memcpy(nraw + str1->len, str2->raw, str2->len);
	nraw[nlen] = '\0';
	return c_str(nraw, nlen);
}

object_t *nullp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is nil."));
	}
	if (req_length (lst,symbol("nullp"),1) == err_symbol)	return err_symbol;
	if (CAR (lst) == NIL)
		return T;
	return NIL;
}

object_t *funcp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is a function."));
	}
	if (req_length (lst,symbol("funcp"),1) == err_symbol)	return err_symbol;
	if (isfunc (CAR (lst)))
		return T;
	return NIL;
}

object_t *listp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is a list."));
	}
	if (req_length (lst,symbol("listp"),1) == err_symbol)	return err_symbol;
	if (islist (CAR (lst)))
		return T;
	return NIL;
}

object_t *symbolp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is a symbol."));
	}
	if (req_length (lst,symbol("symbolp"),1) == err_symbol)	return err_symbol;
	if (issymbol(CAR (lst))) {
		return T;
	}
	return NIL;
}

object_t *numberp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is a number."));
	}
	if (req_length (lst,symbol("numberp"),1) == err_symbol)	return err_symbol;
	if (isnumber (CAR (lst)))
		return T;
	return NIL;
}

object_t *stringp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is a string."));
	}
	if (req_length (lst,symbol("stringp"),1) == err_symbol)	return err_symbol;
	if (CAR(lst)->type == STRING) return T;
	return NIL;
}

object_t *integerp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is an integer."));
	}
	if (req_length (lst,symbol("integerp"),1) == err_symbol)	return err_symbol;
	if (isint (CAR (lst)))
		return T;
	return NIL;
}

object_t *floatp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is a floating-point number."));
	}
	if (req_length (lst,symbol("floatp"),1) == err_symbol)	return err_symbol;
	if (isfloat (CAR (lst)))
		return T;
	return NIL;
}

object_t *vectorp (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return t if object is a vector."));
	}
	if (req_length (lst,symbol("vectorp"),1) == err_symbol)	return err_symbol;
	if (isvector (CAR (lst)))
		return T;
	return NIL;
}

object_t *lisp_load (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Evaluate contents of a file."));
	}
	if (req_length (lst,symbol("load"),1) == err_symbol)	return err_symbol;
	object_t *str = CAR (lst);
	if (str->type != STRING) {
		return THROW (err_wrong_type, increfs(str));
	}
	char *filename = OSTR (str);
	if (!load_file (NULL, filename, 0)) {
		return THROW (symbol("load-file-error"), increfs(str));
	}
	return T;
}

object_t *lisp_read_string (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Parse a string into a sexp or list "));
	}
	if (req_length (lst,symbol("eval-string"),1) == err_symbol)	return err_symbol;
	object_t *stro = CAR (lst);
	if (stro->type != STRING) {
		return THROW (err_wrong_type, increfs(stro));
	}
	char *str = OSTR (stro);
	reader_t *r = reader_create (NULL, str, "eval-string", 0);
	object_t *sexp = read_sexp (r);
	reader_destroy (r);
	if (sexp == err_symbol)
		THROW (symbol("parse-error"), increfs(stro));
	return sexp;
}

object_t *throw (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Throw an object, and attachment, as an exception."));
	}
	return THROW (increfs(CAR (lst)), increfs(CAR (CDR (lst))));
}

object_t *catch (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Catch an exception and return attachment."));
	}
	object_t *csym = eval (CAR (lst));
	if (csym == err_symbol) {
		return csym;
	}
	object_t *body = CDR (lst);
	object_t *r = eval_body (body);
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

object_t *lisp_vset (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Set slot in a vector to "));
	}
	if (req_length (lst,symbol("vset"),3) == err_symbol)	return err_symbol;
	object_t *vec = CAR (lst);
	object_t *ind = CAR (CDR (lst));
	object_t *val = CAR (CDR (CDR (lst)));
	if (!isvector (vec))
		THROW (err_wrong_type, increfs(vec));
	if (!isint (ind))
		THROW (err_wrong_type, increfs(ind));
	return vset_check (vec, ind, val);
}

object_t *lisp_vget (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Get object stored in vector slot."));
	}
	if (req_length (lst,symbol("vget"),2) == err_symbol)	return err_symbol;
	object_t *vec = CAR (lst);
	object_t *ind = CAR (CDR (lst));
	if (!isvector (vec))
		THROW (err_wrong_type, increfs(vec));
	if (!isint (ind))
		THROW (err_wrong_type, increfs(ind));
	return vget_check (vec, ind);
}

object_t *lisp_vlength (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return length of the vector."));
	}
	if (req_length (lst,symbol("vlength"),1) == err_symbol)	return err_symbol;
	object_t *vec = CAR (lst);
	if (!isvector (vec))
		THROW (err_wrong_type, increfs(vec));
	return c_int (VLENGTH (vec));
}

object_t *make_vector (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Make a new vector of given length, initialized to given "));
	}
	if (req_length (lst,symbol("make-vector"),2) == err_symbol)	return err_symbol;
	object_t *len = CAR (lst);
	object_t *o = CAR (CDR (lst));
	if (!isint (len))
		THROW (err_wrong_type, increfs(len));
	return c_vec (into2int (len), o);
}

object_t *lisp_vconcat (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Concatenate two vectors."));
	}
	if (req_length (lst,symbol("vconcat2"),2) == err_symbol)	return err_symbol;
	object_t *a = CAR (lst);
	object_t *b = CAR (CDR (lst));
	if (!isvector (a))
		return THROW (err_wrong_type, increfs(a));
	if (!isvector (b))
		return THROW (err_wrong_type, increfs(b));
	return vector_concat (a, b);
}

object_t *lisp_vsub (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return subsection of vector."));
	}
	if (reqm_length (lst,symbol("subv"),2) == err_symbol)	return err_symbol;
	object_t *v = CAR (lst);
	object_t *starto = CAR (CDR (lst));
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
	object_t *endo = CAR (CDR (CDR (lst)));
	if (!isint (endo))
		THROW (err_wrong_type, increfs(endo));
	int end = into2int (endo);
	if (end >= (int) VLENGTH (v))
		THROW (symbol("bad-index"), increfs(endo));
	if (end < start)
		THROW (symbol("bad-index"), increfs(endo));
	return vector_sub (v, start, end);
}

object_t *lisp_refcount (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return number of reference counts to "));
	}
	if (req_length (lst,symbol("refcount"),1) == err_symbol)	return err_symbol;
	return c_int (CAR (lst)->refs);
}

object_t *lisp_eval_depth (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return the current evaluation depth."));
	}
	if (req_length (lst,symbol("eval-depth"),0) == err_symbol)	return err_symbol;
	return c_int (stack_depth);
}

object_t *lisp_max_eval_depth (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Return or set the maximum evaluation depth."));
	}
	if (reqx_length (lst,symbol("max-eval-depth"),1) == err_symbol)	return err_symbol;
	if (lst == NIL)
		return c_int (max_stack_depth);
	object_t *arg = CAR (lst);
	if (!isint (arg))
		THROW (err_wrong_type, increfs(arg));
	int new_depth = into2int (arg);
	if (new_depth < 10)
		return NIL;
	max_stack_depth = new_depth;
	return increfs(arg);
}

object_t *lisp_exit (object_t * lst) {
	if (lst == doc_string) {
		return c_strs(strings.newstr("%s", "Halt the interpreter and return given integer."));
	}
	if (reqx_length (lst,symbol("exit"),1) == err_symbol)	return err_symbol;
	if (lst == NIL)
		exit(0);
	if (!isint (CAR (lst)))
		return THROW (err_wrong_type, increfs(CAR (lst)));
	exit (into2int (CAR (lst)));
	return NULL;
}

object_t *addition (object_t * lst) {
	DOC ("Perform addition operation.");
	return arith (ADD, lst);
}

object_t *multiplication (object_t * lst) {
	DOC ("Perform multiplication operation.");
	return arith (MUL, lst);
}

object_t *subtraction (object_t * lst) {
	DOC ("Perform subtraction operation.");
	return arith (SUB, lst);
}

object_t *division (object_t * lst) {
	DOC ("Perform division operation.");
	return arith (DIV, lst);
}

object_t *num_eq (object_t * lst) {
	DOC ("Compare two numbers by =.");
	return num_cmp (EQ, lst);
}

object_t *num_lt (object_t * lst) {
	DOC ("Compare two numbers by <.");
	return num_cmp (LT, lst);
}

object_t *num_lte (object_t * lst) {
	DOC ("Compare two numbers by <=.");
	return num_cmp (LTE, lst);
}

object_t *num_gt (object_t * lst) {
	DOC ("Compare two numbers by >.");
	return num_cmp (GT, lst);
}

object_t *num_gte (object_t * lst) {
	DOC ("Compare two numbers by >=.");
	return num_cmp (GTE, lst);
}

object_t *modulus (object_t * lst) {
	DOC ("Return modulo of arguments.");
	// REQ (lst, 2, symbol("%"));
	object_t *a = CAR (lst);
	object_t *b = CAR (CDR (lst));
	if (!isint (a))
		return THROW (err_wrong_type, increfs(a));
	if (!isint (b))
		return THROW (err_wrong_type, increfs(b));
	object_t *m = c_int (0);
	mp.mpz_mod(DINT (m), DINT (a), DINT (b));
	return m;
}

/* Maths */
object_t *arith(int op, object_t * lst) {
	if (op == DIV) {
		if (reqm_length (lst,symbol("div"),2) == err_symbol)	{
			return err_symbol;
		}
	}
	int intmode = 1;
	object_t *accumz = NIL;
	object_t *accumf = NIL;
	object_t *convf = c_float (0);
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
		object_t *num = CAR (lst);
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
		object_t *num = CAR (lst);
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



object_t *num_cmp(int cmp, object_t * lst) {
	if (reqm_length (lst,symbol("compare-func"),2) == err_symbol)	return err_symbol;
	object_t *a = CAR (lst);
	object_t *b = CAR (CDR (lst));
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
		object_t *c = b;
		b = a;
		a = c;
		invr = -1;
	}
	if (isfloat (a) && isint (b)) {
		/* Convert down. */
		object_t *convf = c_float(0);
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

pub char *addslashes(char *cleanstr) {
  /* Count the quotes and backquotes. */
  char *p = cleanstr;
  int cnt = 0;
  while (*p != '\0')
    {
      if (*p == '\\' || *p == '"')
	cnt++;
      p++;
    }

  /* Two extra for quotes and one for each character that needs
     escaping. */
  char *str = calloc!(strlen (cleanstr) + (size_t)cnt + 2 + 1, 1);
  strcpy (str + 1, cleanstr);

  /* Place backquotes. */
  char *c = str + 1;
  while (*c != '\0')
    {
      if (*c == '\\' || *c == '"')
	{
	  memmove (c + 1, c, strlen (c) + 1);
	  *c = '\\';
	  c++;
	}
      c++;
    }

  /* Surrounding quotes. */
  str[0] = '"';
  str[strlen (str) + 1] = '\0';
  str[strlen (str)] = '"';

  return str;
}

void *xrealloc (void *p, size_t size) {
  void *np = realloc (p, size);
  if (np == NULL) panic("realloc failed");
  return np;
}
