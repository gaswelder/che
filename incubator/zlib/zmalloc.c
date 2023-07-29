pub void zcfree(void *opaque, *ptr) {
    (void)opaque;
    free(ptr);
}

pub void *zcalloc(void *opaque, size_t items, size_t size) {
    (void)opaque;
    return calloc(items, size);
}
