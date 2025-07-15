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

// Runs n threads in parallel, each executing thmain with args[i].
pub void parallel(thr_func *thrmain, void **args, size_t n) {
	// Spawn the workers.
	thr_t **tt = calloc!(n, sizeof(thr_t));
	for (size_t i = 0; i < n; i++) {
		tt[i] = start(thrmain, args[i]);
	}

	// Wait for all to finish.
	for (size_t i = 0; i < n; i++) {
		int err = wait(tt[i], NULL);
		if (err) {
			panic("thread wait failed: %d (%s)", err, strerror(err));
		}
	}

	free(tt);
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

/*
 * Wakes one of the threads blocked on the given condition.
 * If there are no threads waiting, does nothing.
 */
pub bool wake_one(cnd_t *cond) {
	return OS.pthread_cond_signal(&cond->c) == 0;
}

/*
 * Wakes all threads waiting for the given condition.
 */
pub bool cnd_broadcast(cnd_t *cond) {
	return OS.pthread_cond_broadcast(&cond->c) == 0;
}
