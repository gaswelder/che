#link pthread

typedef void *thr_func(void *);

typedef uint64_t thr_t;

/*
 * pthread.h synopsis
 *
 * We can't just include <pthread.h> (or any other system header
 * for that matter) because system headers are typically not even
 * standard C, so Che wouldn't be able to parse them.
 */
pub int pthread_create(void *thread, void *attr, thr_func *f, void *arg);
pub int pthread_detach(void *thread);
pub int pthread_join(thr_t thread, void **res);


/*
 * Creates and starts a new thread running function 'f'.
 */
pub thr_t *thr_new(thr_func *f, void *arg)
{
	thr_t *t = calloc(1, sizeof(thr_t));
	if(!t) return NULL;

	if(pthread_create(t, NULL, f, arg) != 0) {
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
	int r = pthread_join(*t, res);
	free(t);
	return r == 0;
}

/*
 * Detaches the thread. Returns false on error.
 */
pub bool thr_detach(thr_t *t)
{
	int r = pthread_detach(t);
	free(t);
	return r == 0;
}
