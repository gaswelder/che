#import os/threads

#define N 16

int main()
{
	/*
	 * Create N threads, passing them different arguments.
	 */
	thr_t *t[N] = {0};
	int args[N] = {0};
	for(int i = 0; i < N; i++) {
		args[i] = i + 1;
		t[i] = thr_new(threadmain, &args[i]);
		assert(t[i]);
	}

	/*
	 * Wait for all threads, printing their return values.
	 * Each thread should return square of its argument.
	 */
	for(int i = 0; i < N; i++) {
		void *r = NULL;
		thr_join(t[i], &r);
		if((i+1)*(i+1) != *((int*)r)) {
			puts("FAIL");
			exit(1);
		}
		free(r);
	}

	puts("OK");
	return 0;
}

void *threadmain(void *arg)
{
	int val = *((int *) arg);
	//sleep(val);

	int *r = calloc(1, sizeof(int));
	*r = val * val;
	return r;
}
