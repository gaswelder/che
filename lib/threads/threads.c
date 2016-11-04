#link pthread

#include <pthread.h>

#type pthread_t

typedef void *thr_func(void *);

struct __thr {
	pthread_t t;
};

typedef struct __thr thr_t;

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
