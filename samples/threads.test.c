#import os/threads
#import error

const int N = 16;

int main() {
	int expected[N] = {};

	// Start N threads with different arguments.
	threads.thr_t *t[N] = {0};
	for (int i = 0; i < N; i++) {
		int arg = i + 1;
		t[i] = threads.start(threadmain, box(&arg, sizeof(arg)));
		expected[i] = arg * arg;
	}

	// Wait for threads and get their return values.
	error.t err = {};
	for (int i = 0; i < N; i++) {
		box_t *b = threads.wait(t[i], &err);
		if (err.set) {
			panic("thread wait failed: %s", err.msg);
		}
		int result = 0;
		unbox(b, &result, sizeof(result));
		if (result != expected[i]) {
			panic("result from %d = %d (want %d)", i, result, expected[i]);
		}
	}
	return 0;
}

void *threadmain(void *arg) {
	int val = 0;
	unbox((box_t *) arg, &val, sizeof(val));
	int result = val * val;
	return box(&result, sizeof(result));
}

typedef {
	char *bytes;
	size_t size;
} box_t;

// Allocates a copy of data on the head and returns the pointer.
box_t *box(void *data, size_t datasize) {
	box_t *b = calloc!(1, sizeof(box_t));
	b->bytes = calloc!(datasize, 1);
	b->size = datasize;
	memcpy(b->bytes, data, datasize);
	return b;
}

// Copies data from b to place, deallocates b.
void unbox(box_t *b, void *place, size_t placesize) {
	if (b->size != placesize) {
		panic("box size %zu != place size %zu", b->size, placesize);
	}
	memcpy(place, b->bytes, placesize);
	free(b->bytes);
	free(b);
}
