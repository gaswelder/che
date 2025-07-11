pub void *xmalloc (size_t size) {
  void *p = calloc (size, 1);
  if (!p) panic("calloc failed");
  return p;
}

pub void *xrealloc (void *p, size_t size) {
  void *np = realloc (p, size);
  if (np == NULL) panic("realloc failed");
  return np;
}

