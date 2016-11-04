/*
 * A nice problem from Tanenbaum's book on operating systems:
 * create N threads and make them start in sorted order, first thread
 * number 1, then thread number 2, and so on.
 */

import "threads"

struct control {
	int id;
	mtx_t *lock;
	cnd_t *cnd;
	bool running;
};

int main()
{
	mtx_t *lock = mtx_new();
	cnd_t *cnd = cnd_new();

	int i;
	const int N = 4;
	struct control args[N];
	thr_t *t[N];

	/*
	 * Start threads one by one
	 */
	for(i = 0; i < N; i++) {
		struct control *c = &args[i];
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
	for(i = 0; i < N; i++) {
		thr_join(t[i], NULL);
	}

	cnd_free(cnd);
	mtx_free(lock);

	return 0;
}

void *tfunc(void *arg)
{
	struct control *c = arg;

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

