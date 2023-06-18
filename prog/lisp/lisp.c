#import strings

int main() {
  item_t *input = read();
  item_t *r = eval(input);
  print(r);
  return 0;
}

char *symbols[1000] = {0};

char *intern(char *sym) {
  int i = 0;
  while (i < 1000 && symbols[i]) {
    char *s = symbols[i];
    if (strcmp(s, sym) == 0) {
      return s;
    }
    i++;
  }
  if (i == 1000) {
    fprintf(stderr, "too many symbols\n");
    exit(1);
  }

  symbols[i] = strings.newstr("%s", sym);
  return symbols[i];
}

typedef {
  bool list;
  void *data;
  item_t *next;
} item_t;

item_t *sym(char *s) {
  item_t *i = calloc(1, sizeof(item_t));
  i->list = false;
  i->data = s;
  return i;
}

item_t *cons(item_t *data, item_t *next) {
  item_t *r = calloc(1, sizeof(item_t));
  r->list = true;
  r->data = data;
  r->next = next;
  return r;
}

item_t *car(item_t *x) {
  if (!x) {
    panic("car: NULL");
  }
  if (!x->list) {
    print(x);
    panic("car: item not a list");
  }
  return x->data;
}

char *desc(item_t *x) {
  if (!x) {
    return "NULL";
  }
  if (x->list) {
    return strings.newstr("list (%s, %s)", desc((item_t *)x->data), desc(x->next));
  }
  return strings.newstr("sym(%s)", x->data);
}

item_t *cdr(item_t *x) {
  if (!x) {
    panic("cdr: NULL");
  }
  if (!x->list) {
    print(x);
    panic("cdr: item not a list: %s", desc(x));
  }
  return x->next;
}

int peek() {
  int c = getchar();
  if (c == EOF) {
    return EOF;
  }
  ungetc(c, stdin);
  return c;
}

void skipspaces() {
  while (isspace(peek())) {
    getchar();
  }
}

item_t *read() {
  skipspaces();
  if (peek() == '(') {
    return readlist();
  }
  return readsymbol();
}

item_t *reverse(item_t *in) {
  item_t *r = NULL;
  item_t *l = in;
  while (l) {
    r = cons(car(l), r);
    l = cdr(l);
  }
  return r;
}

item_t *readlist() {
  getchar(); // "("
  item_t *r = NULL;
  
  skipspaces();
  while (peek() != EOF && peek() != ')') {
    r = cons(read(), r);
    skipspaces();
  }
  if (peek() != ')') {
    fprintf(stderr, "expected )\n");
    exit(1);
  }
  getchar(); // ")"
  return reverse(r);
}

item_t *readsymbol() {
  char buf[60] = {0};
  int pos = 0;
  while (peek() != EOF && !isspace(peek()) && peek() != ')') {
    buf[pos++] = getchar();
  }
  return sym(intern(buf));
}

void print(item_t *x) {
  if (!x) {
    puts("NULL");
    return;
  }
  if (!x->list) {
    printf("%s", (char *) x->data);
    return;
  }
  putchar('(');
  item_t *l = x;
  int i = 0;
  while (l) {
    if (i > 0) {
      putchar(' ');
    }
    print(car(l));
    l = cdr(l);
    i++;
  }
  putchar(')');
}


void dunno(item_t *i) {
  fprintf(stderr, "how to eval? ");
  print(i);
  puts("");
  exit(1);
}

item_t *eval(item_t *i) {
  if (i->list) {
    return eval_list(i);
  }
  dunno(i);
  return NULL;
}

item_t *eval_list(item_t *l) {
  item_t *first = car(l);
  if (first->list) {
    dunno(first);
    return NULL;
  }
  return runfunc(first->data, cdr(l));
}

item_t *runfunc(char *name, item_t *args) {
  if (name == intern("apply")) return apply(args);
  if (name == intern("quote")) return car(args);
  if (name == intern("cons")) return cons(car(args), car(cdr(args)));
  if (name == intern("cond")) return cond(args);
  if (name == intern("eq?")) {
    if (eval(car(args)) == eval(car(cdr(args)))) {
      return sym("true");
    }
    return NULL;
  }
  panic("unknown function %s", name);
  return NULL;
}


item_t *cond(item_t *args) {
  item_t *l = args;
  while (l) {
    item_t *cas = car(l);
    item_t *cond = car(cas);
    if (eval(cond)) {
      item_t *result = car(cdr(cas));
      return eval(result);
    }
    l = cdr(l);
  }
  return NULL;
}

item_t *apply(item_t *args) {
  item_t *fn = car(args);
  if (fn->list) {
    puts("how to apply list?");
    exit(1);
  }
  return runfunc(fn->data, eval(car(cdr(args))));
}