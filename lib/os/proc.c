#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

pub typedef int workerfunc_t(void *);

/*
 * Processes the list of jobs using forking processes,
 * each process running f on its corresponding item of the jobs list.
 * jobs is a null-terminated list of pointers.
*/
pub void parallel(workerfunc_t f, void **jobs) {
	int nworkers = 0;
    void **job = jobs;
	for (int i = 0; i < 8; i++) {
        if (!*job) break;
		spawn(f, *job++);
		nworkers++;
	}
	while (nworkers > 0) {
		OS.wait(NULL);
		nworkers--;
		if (*job) {
			spawn(f, *job++);
			nworkers++;
		}
	}
}

void spawn(workerfunc_t f, void *job) {
    int pid = OS.fork();
    if (pid < 0) panic("fork failed");
    if (pid > 0) return;
	exit(f(job));
}
