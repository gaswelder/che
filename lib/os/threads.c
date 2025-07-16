#link pthread
#type pthread_t
#type pthread_mutex_t
#type pthread_cond_t
#include <pthread.h>

pub typedef void *thr_func(void *);

pub typedef {
	pthread_t t;
} thr_t;

pub typedef {
	pthread_mutex_t m;
} mtx_t;

pub typedef {
	pthread_cond_t c;
} cnd_t;

// Creates and starts a new thread running function 'f'.
pub thr_t *start(thr_func *f, void *arg) {
	thr_t *t = calloc!(1, sizeof(thr_t));
	if (OS.pthread_create(&t->t, NULL, f, arg) != 0) {
		panic("thread creation failed: %s", strerror(errno));
	}
	return t;
}

// Waits for thread t to finish, puts the exit value into res.
// Return an error code, zero meaning no error.
pub int wait(thr_t *t, void **res) {
	int err = OS.pthread_join(t->t, res);
	free(t);
	return err;
}

typedef {
	size_t n;
	void **args;
	thr_func *f;
} tasklist_t;

void *seqworker(void *arg) {
	tasklist_t *l = arg;
	for (size_t i = 0; i < l->n; i++) {
		l->f(l->args[i]);
	}
	return NULL;
}

// Runs 8 threads in parallel, each executing thrmain with args[i].
pub void parallel(thr_func *thrmain, void **args, size_t n) {
	int per_worker = (n + (8 - 1)) / 8;

	tasklist_t *lists = calloc!(8, sizeof(tasklist_t));
	for (int i = 0; i < 8; i++) {
		lists[i].f = thrmain;
		lists[i].args = calloc!(per_worker, sizeof(void *));
	}

	for (size_t i = 0; i < n; i++) {
		size_t w = i % 8;
		tasklist_t *l = &lists[w];
		l->args[l->n++] = args[i];
	}

	// Spawn the workers.
	thr_t **tt = calloc!(n, sizeof(thr_t));
	for (size_t i = 0; i < 8; i++) {
		tt[i] = start(&seqworker, &lists[i]);
	}

	// Wait for all to finish.
	for (size_t i = 0; i < 8; i++) {
		int err = wait(tt[i], NULL);
		if (err) {
			panic("thread wait failed: %d (%s)", err, strerror(err));
		}
	}

	free(tt);
	for (int i = 0; i < 8; i++) {
		free(lists[i].args);
	}
	free(lists);
}

/*
 * Detaches the thread. Returns false on error.
 */
pub bool thr_detach(thr_t *t) {
	int r = OS.pthread_detach(t->t);
	free(t);
	return r == 0;
}



/*
 * Creates and returns a new mutex
 */
pub mtx_t *mtx_new() {
	mtx_t *m = calloc!(1, sizeof(mtx_t));
	if (OS.pthread_mutex_init(&m->m, NULL) != 0) {
		panic("mutex_init failed");
	}
	return m;
}

pub void mtx_free(mtx_t *m) {
	OS.pthread_mutex_destroy(&m->m);
	free(m);
}

// Locks mutex m.
pub bool lock(mtx_t *m) {
	return OS.pthread_mutex_lock(&m->m) == 0;
}

// Unlocks mutex m.
pub bool unlock(mtx_t *m) {
	return OS.pthread_mutex_unlock(&m->m) == 0;
}

/*
 * Creates and return a new condition variable
 */
pub cnd_t *cnd_new() {
	cnd_t *c = calloc!(1, sizeof(cnd_t));
	if (OS.pthread_cond_init(&c->c, NULL) != 0) {
		free(c);
		return NULL;
	}
	return c;
}

pub void cnd_free(cnd_t *c) {
	OS.pthread_cond_destroy(&c->c);
	free(c);
}

/*
 * Unlocks the mutex, waits for the condition, locks the mutex again.
 */
pub bool unlock_wait_lock(mtx_t *mtx, cnd_t *cond) {
	return OS.pthread_cond_wait( &cond->c, &mtx->m ) == 0;
}

// Wakes one of the threads waiting for cond.
// Does nothing if there are no threads waiting for cond.
pub bool wake_one(cnd_t *cond) {
	return OS.pthread_cond_signal(&cond->c) == 0;
}

// Wakes all threads waiting for cond.
pub bool wake_all(cnd_t *cond) {
	return OS.pthread_cond_broadcast(&cond->c) == 0;
}

// intentionally weird name to avoid conflicts.
#define THPIPESIZE 32

pub typedef {
	mtx_t *lock;
	cnd_t *sig_wrote; // wakes up readers
	cnd_t *sig_read; // wakes up writers
	void **items;
	int start, len;
	bool closed;
} pipe_t;

pub pipe_t *newpipe() {
	pipe_t *p = calloc!(1, sizeof(pipe_t));
	p->lock = mtx_new();
	p->sig_wrote = cnd_new();
	p->sig_read = cnd_new();
	p->items = calloc!(THPIPESIZE, sizeof(void *));
	return p;
}

pub void freepipe(pipe_t *p) {
	cnd_free(p->sig_wrote);
	cnd_free(p->sig_read);
	mtx_free(p->lock);
	free(p->items);
}

// Reads one entry from the pipe into out.
// Returns true on success or false if the pipe has been closed
// and there is no buffered data.
pub bool pread(pipe_t *p, void **out) {
	lock(p->lock);
	while (true) {
		if (p->len > 0) {
			*out = p->items[p->start];
			p->start = (p->start + 1) % THPIPESIZE;
			p->len--;
			unlock(p->lock);
			wake_one(p->sig_read);
			return true;
		}
		if (p->closed) {
			unlock(p->lock);
			return false;
		}
		unlock_wait_lock(p->lock, p->sig_wrote);
	}
}

pub void pclose(pipe_t *p) {
	lock(p->lock);
	p->closed = true;
	wake_all(p->sig_wrote);
	unlock(p->lock);
}

pub void pwrite(pipe_t *p, void *data) {
	lock(p->lock);
	if (p->closed) {
		panic("writing to a closed pipe");
	}
	while (true) {
		if (p->len < THPIPESIZE) {
			int pos = (p->start + p->len) % THPIPESIZE;
			p->items[pos] = data;
			p->len++;
			unlock(p->lock);
			wake_one(p->sig_wrote);
			return;
		}
		unlock_wait_lock(p->lock, p->sig_read);
	}
}
