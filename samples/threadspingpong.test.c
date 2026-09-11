#import os/threads
#import error

typedef {
	threads.mtx_t *lock;
	threads.cnd_t *cnd;
	int value;
	bool stop;
} control_t;

char *msgs[10] = {};
int n = 0;

void tput(const char *format, ...) {
	char *msg = calloc!(20, 1);
	va_list args = {};
	va_start(args, format);
	vsprintf(msg, format, args);
	msgs[n++] = msg;
}

int main() {
	control_t c = {};
	c.lock = threads.mtx_new();
	c.cnd = threads.cnd_new();
	threads.thr_t *t = threads.start(tfunc, &c);
	
	threads.lock(c.lock);
	while (true) {
		if (c.value % 2 == 0) {
			tput("main: %d", c.value);
			c.value++;
		}
		if (c.value == 9) {
			c.stop = true;
			threads.unlock(c.lock);
			break;
		}
		threads.unlock_wait_lock(c.lock, c.cnd);
	}

	error.t err = {};
	threads.wait(t, &err);
	if (err.set) {
		panic("thread wait failed: %s", err.msg);
	}
	threads.cnd_free(c.cnd);
	threads.mtx_free(c.lock);

	const char *expected[] = {
		"main: 0",
		"thread: 1",
		"main: 2",
		"thread: 3",
		"main: 4",
		"thread: 5",
		"main: 6",
		"thread: 7",
		"main: 8",
		"thread: 9",
	};
	for (int i = 0; i < 10; i++) {
		if (strcmp(expected[i], msgs[i]) != 0) panic("!");
	}

	return 0;
}

void *tfunc(void *arg) {
	control_t *c = arg;
	while (true) {
		threads.lock(c->lock);
		if (c->value % 2 == 1) {
			tput("thread: %d", c->value);
			if (c->stop) {
				threads.unlock(c->lock);
				break;
			}
			c->value++;
		}
		threads.unlock(c->lock);
		threads.wake_one(c->cnd);
	}
	return NULL;
}
