/*
  Define a once() function depending on the availability of atomics. If this is
  compiled with DYNAMIC_CRC_TABLE defined, and if CRCs will be computed in
  multiple threads, and if atomics are not available, then get_crc_table() must
  be called to initialize the tables and must return before any threads are
  allowed to compute or combine CRCs.
*/

/* Definition of once functionality. */
typedef struct once_s once_t;

/* Check for the availability of atomics. */
#if defined(__STDC__) && __STDC_VERSION__ >= 201112L && \
    !defined(__STDC_NO_ATOMICS__)

#include <stdatomic.h>

/* Structure for once(), which must be initialized with ONCE_INIT. */
struct once_s {
    atomic_flag begun;
    atomic_int done;
};
#define ONCE_INIT {ATOMIC_FLAG_INIT, 0}

/*
  Run the provided init() function exactly once, even if multiple threads
  invoke once() at the same time. The state must be a once_t initialized with
  ONCE_INIT.
 */
void once(state, init)
    once_t *state;
    void (*init)(void);
{
    if (!atomic_load(&state->done)) {
        if (atomic_flag_test_and_set(&state->begun))
            while (!atomic_load(&state->done))
                ;
        else {
            init();
            atomic_store(&state->done, 1);
        }
    }
}

#else   /* no atomics */

/* Structure for once(), which must be initialized with ONCE_INIT. */
struct once_s {
    volatile int begun;
    volatile int done;
};
#define ONCE_INIT {0, 0}

/* Test and set. Alas, not atomic, but tries to minimize the period of
   vulnerability. */
int test_and_set(flag)
    int volatile *flag;
{
    int was;

    was = *flag;
    *flag = 1;
    return was;
}

/* Run the provided init() function once. This is not thread-safe. */
void once(state, init)
    once_t *state;
    void (*init)(void);
{
    if (!state->done) {
        if (test_and_set(&state->begun))
            while (!state->done)
                ;
        else {
            init();
            state->done = 1;
        }
    }
}

#endif
