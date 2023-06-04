typedef {
	size_t len;
	union {
		int i;
		void *v;
	} *vals;
} foo;
