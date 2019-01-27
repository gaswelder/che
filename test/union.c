struct foo {
	size_t len;
	union {
		int i;
		void *v;
	} *vals;
};
