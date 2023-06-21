#import os/threads

typedef {
	threads.mtx_t *lock;
	threads.cnd_t *cnd;
	int value;
	bool stop;
} control_t;

int main() {
	control_t c = {};
	c.lock = threads.mtx_new();
	c.cnd = threads.cnd_new();
	threads.thr_t *t = threads.thr_new(tfunc, &c);
	
	threads.mtx_lock(c.lock);
	while (true) {
		if (c.value % 2 == 0) {
			printf("main: %d\n", c.value);
			c.value++;
		}
		if (c.value == 9) {
			c.stop = true;
			threads.mtx_unlock(c.lock);
			break;
		}
		threads.unlock_wait_lock(c.lock, c.cnd);
	}
	threads.thr_join(t, NULL);
	threads.cnd_free(c.cnd);
	threads.mtx_free(c.lock);
	return 0;
}

void *tfunc(void *arg) {
	control_t *c = arg;
	while (true) {
		threads.mtx_lock(c->lock);
		if (c->value % 2 == 1) {
			printf("thread: %d\n", c->value);
			if (c->stop) {
				threads.mtx_unlock(c->lock);
				break;
			}
			c->value++;
		}
		threads.mtx_unlock(c->lock);
		threads.wake_one(c->cnd);
	}
	return NULL;
}
