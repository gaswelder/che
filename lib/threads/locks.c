
/*
 * We can't know sizes of mutexes and condition variables without
 * parsing <pthread.h>. But we can't parse system headers either.
 * So we just take a guess and hope this size will be more than
 * enough.
 */
#define MTX_SIZE 64
#define CND_SIZE 64
/*
 * Here we will see mtx_t and cnd_t as opaque regions of bytes in
 * memory, since only pointers to mtx_t and cnd_t will be used.
 */
typedef char mtx_t;
typedef char cnd_t;

/*
 * Pointer to pthread_mutex_t is same as a void pointer.
 * Same for pthread_cond_t pointer and any other pointer.
 */
pub int pthread_mutex_init(void *m, void *attr);
pub int pthread_mutex_destroy(void *m);
pub int pthread_mutex_lock(void *mutex);
pub int pthread_mutex_unlock(void *mutex);

pub int pthread_cond_broadcast(void *c);
pub int pthread_cond_destroy(void *c);
pub int pthread_cond_init(void *c, const void *attr);
pub int pthread_cond_signal(void *c);
pub int pthread_cond_wait(void *c, void *m);

pub int pthread_self();

/*
 * Creates and returns a new mutex
 */
pub mtx_t *mtx_new()
{
	mtx_t *m = calloc(1, MTX_SIZE);
	if(!m) return NULL;

	int r = pthread_mutex_init(m, NULL);
	if(r != 0) {
		free(m);
		return NULL;
	}
	return m;
}

pub void mtx_free(mtx_t *m)
{
	pthread_mutex_destroy(m);
	free(m);
}

/*
 * Locks the mutex.
 */
pub int mtx_lock(mtx_t *mtx) {
	return pthread_mutex_lock(mtx) == 0;
}

/*
 * Unlocks the mutex.
 */
pub int mtx_unlock(mtx_t *mtx) {
	return pthread_mutex_unlock( mtx ) == 0;
}


/*
 * Creates and return a new condition variable
 */
pub cnd_t *cnd_new()
{
	cnd_t *c = calloc(1, CND_SIZE);
	if(!c) return NULL;

	int r = pthread_cond_init(c, NULL);
	if(r != 0) {
		free(c);
		return NULL;
	}
	return c;
}

pub void cnd_free(cnd_t *c)
{
	pthread_cond_destroy(c);
	free(c);
}

/*
 * Unlocks the mutex, waits for the condition, locks the mutex again.
 */
pub int cnd_wait(cnd_t *cond, mtx_t *mtx) {
	return pthread_cond_wait( cond, mtx ) == 0;
}

/*
 * Wakes one of the threads waiting for the given condition.
 */
pub bool cnd_signal(cnd_t *cond) {
	return pthread_cond_signal(cond) == 0;
}

/*
 * Wakes all threads waiting for the given condition.
 */
pub int cnd_broadcast(cnd_t *cond) {
	return pthread_cond_broadcast( cond ) == 0;
}
