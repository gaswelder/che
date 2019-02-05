/*
 * A nice problem from Tanenbaum's book on operating systems:
 * create N threads and make them start in sorted order, first thread
 * number 1, then thread number 2, and so on.
 */

import "threads"

typedef {
	int id;
	mtx_t *lock;
	cnd_t *cnd;
	bool running;
} control_t;

int main()
{
	mtx_t *lock = mtx_new();
	cnd_t *cnd = cnd_new();

	const int N = 4;
	control_t args[N] = {0};
	thr_t *t[N] = {0};

	/*
	 * Start threads one by one
	 */
	for(int i = 0; i < N; i++) {
		control_t *c = &args[i];
		c->id = i + 1;
		c->lock = lock;
		c->cnd = cnd;
		c->running = false;
		printf("0: creating %d\n", c->id);
		t[i] = thr_new(tfunc, c);

		printf("0: waiting for %d to start\n", c->id);
		mtx_lock(c->lock);
		while(!c->running) {
			cnd_wait(c->cnd, c->lock);
		}
		printf("0: %d ok\n", c->id);
		mtx_unlock(c->lock);
	}

	puts("OK");
	
	/*
	 * Wait for threads to finish
	 */
	for(int i = 0; i < N; i++) {
		thr_join(t[i], NULL);
	}

	cnd_free(cnd);
	mtx_free(lock);

	return 0;
}

void *tfunc(void *arg)
{
	control_t *c = arg;

	printf("%d: created\n", c->id);
	mtx_lock(c->lock);
	printf("%d: signalling\n", c->id);
	c->running = true;
	cnd_signal(c->cnd);
	mtx_unlock(c->lock);

	printf("%d: working\n", c->id);
	//sleep(1);
	printf("%d: finishing\n", c->id);
	return NULL;
}

