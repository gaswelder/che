pub void *xrealloc (void *p, size_t size) {
  void *np = realloc (p, size);
  if (np == NULL) panic("realloc failed");
  return np;
}

