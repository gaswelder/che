#link pthread
#type pthread_t
#type pthread_mutex_t
#type pthread_cond_t
#include <pthread.h>

pub typedef void *thr_func(void *);

pub typedef {
	pthread_t t;
} thr_t;

/*
 * Creates and starts a new thread running function 'f'.
 */
pub thr_t *thr_new(thr_func *f, void *arg) {
	thr_t *t = calloc(1, sizeof(thr_t));
	if (!t) {
		return NULL;
	}
	if (OS.pthread_create(&t->t, NULL, f, arg) != 0) {
		free(t);
		return NULL;
	}
	return t;
}

/*
 * Joins the given thread waiting for its finishing.
 * If res is not null, it will be assigned the thread's exit value.
 */
pub bool thr_join(thr_t *t, void **res) {
	int r = OS.pthread_join(t->t, res);
	free(t);
	return r == 0;
}

/*
 * Detaches the thread. Returns false on error.
 */
pub bool thr_detach(thr_t *t) {
	int r = OS.pthread_detach(t->t);
	free(t);
	return r == 0;
}

pub typedef {
	pthread_mutex_t m;
} mtx_t;

pub typedef {
	pthread_cond_t c;
} cnd_t;

/*
 * Creates and returns a new mutex
 */
pub mtx_t *mtx_new()
{
	mtx_t *m = calloc(1, sizeof(mtx_t));
	if (!m) {
		return NULL;
	}
	if (OS.pthread_mutex_init(&m->m, NULL) != 0) {
		free(m);
		return NULL;
	}
	return m;
}

pub void mtx_free(mtx_t *m) {
	OS.pthread_mutex_destroy(&m->m);
	free(m);
}

/*
 * Locks the mutex.
 */
pub bool mtx_lock(mtx_t *mtx) {
	return OS.pthread_mutex_lock(&mtx->m) == 0;
}

/*
 * Unlocks the mutex.
 */
pub bool mtx_unlock(mtx_t *mtx) {
	return OS.pthread_mutex_unlock( &mtx->m ) == 0;
}

/*
 * Creates and return a new condition variable
 */
pub cnd_t *cnd_new() {
	cnd_t *c = calloc(1, sizeof(cnd_t));
	if (!c) {
		return NULL;
	}
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
