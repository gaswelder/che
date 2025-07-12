pub void *xmalloc (size_t size) {
  return calloc!(size, 1);
}

pub void *xrealloc (void *p, size_t size) {
  void *np = realloc (p, size);
  if (np == NULL) panic("realloc failed");
  return np;
}

