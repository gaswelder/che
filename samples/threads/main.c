#import os/threads

#define N 16

int main() {
	/*
	 * Create N threads, passing them different arguments.
	 */
	threads.thr_t *t[N] = {0};
	for (int i = 0; i < N; i++) {
		int arg = i + 1;
		t[i] = threads.thr_new(threadmain, box(&arg, sizeof(arg)));
		assert(t[i]);
	}

	/*
	 * Wait for all threads, printing their return values.
	 * Each thread should return the square of its argument.
	 */
	for (int i = 0; i < N; i++) {
		void *r = NULL;
		threads.thr_join(t[i], &r);
		int result = 0;
		unbox(r, &result, sizeof(result));
		printf("result from %d = %d\n", i, result);
	}
	return 0;
}

void *threadmain(void *arg) {
	int val = 0;
	unbox(arg, &val, sizeof(val));
	int result = val * val;
	return box(&result, sizeof(result));
}

/**
 * Allocates a copy of data on the head and returns the pointer.
 */
void *box(void *data, size_t datasize) {
	char *packagebytes = calloc(datasize, 1);
	if (!packagebytes) {
		return NULL;
	}
	char *databytes = (char *) data;
	for (size_t i = 0; i < datasize; i++) {
		packagebytes[i] = databytes[i];
	}
	return packagebytes;
}

/**
 * Frees the heap pointer, copying its data to the given place.
 */
void unbox(void *package, void *place, size_t placesize) {
	char *packagebytes = (char *) package;
	char *placebytes = (char *) place;
	for (size_t i = 0; i < placesize; i++) {
		placebytes[i] = packagebytes[i];
	}
	free(package);
}
