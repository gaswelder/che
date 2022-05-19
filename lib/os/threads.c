#link pthread
#type pthread_t
#type pthread_mutex_t
#type pthread_cond_t
#include <pthread.h>

typedef void *thr_func(void *);

typedef {
	pthread_t t;
} thr_t;

/*
 * Creates and starts a new thread running function 'f'.
 */
pub thr_t *thr_new(thr_func *f, void *arg)
{
	thr_t *t = calloc(1, sizeof(thr_t));
	if(!t) return NULL;

	if(pthread_create(&t->t, NULL, f, arg) != 0) {
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
	int r = pthread_join(t->t, res);
	free(t);
	return r == 0;
}

/*
 * Detaches the thread. Returns false on error.
 */
pub bool thr_detach(thr_t *t)
{
	int r = pthread_detach(t->t);
	free(t);
	return r == 0;
}

typedef {
	pthread_mutex_t m;
} mtx_t;

typedef {
	pthread_cond_t c;
} cnd_t;

/*
 * Creates and returns a new mutex
 */
pub mtx_t *mtx_new()
{
	mtx_t *m = calloc(1, sizeof(*m));
	if(!m) return NULL;

	int r = pthread_mutex_init(&m->m, NULL);
	if(r != 0) {
		free(m);
		return NULL;
	}
	return m;
}

pub void mtx_free(mtx_t *m)
{
	pthread_mutex_destroy(&m->m);
	free(m);
}

/*
 * Locks the mutex.
 */
pub int mtx_lock(mtx_t *mtx) {
	return pthread_mutex_lock(&mtx->m) == 0;
}

/*
 * Unlocks the mutex.
 */
pub int mtx_unlock(mtx_t *mtx) {
	return pthread_mutex_unlock( &mtx->m ) == 0;
}

/*
 * Creates and return a new condition variable
 */
pub cnd_t *cnd_new()
{
	cnd_t *c = calloc(1, sizeof(*c));
	if(!c) return NULL;

	int r = pthread_cond_init(&c->c, NULL);
	if(r != 0) {
		free(c);
		return NULL;
	}
	return c;
}

pub void cnd_free(cnd_t *c)
{
	pthread_cond_destroy(&c->c);
	free(c);
}

/*
 * Unlocks the mutex, waits for the condition, locks the mutex again.
 */
pub int cnd_wait(cnd_t *cond, mtx_t *mtx) {
	return pthread_cond_wait( &cond->c, &mtx->m ) == 0;
}

/*
 * Wakes one of the threads waiting for the given condition.
 */
pub bool cnd_signal(cnd_t *cond) {
	return pthread_cond_signal(&cond->c) == 0;
}

/*
 * Wakes all threads waiting for the given condition.
 */
pub int cnd_broadcast(cnd_t *cond) {
	return pthread_cond_broadcast( &cond->c ) == 0;
}
