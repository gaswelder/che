#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/wait.h>

#import reader
#import writer

pub typedef {
	reader.t *stdout, *stderr;
	writer.t *stdin;
    int pid;
} proc_t;

/*
 * Executes the program with given args and environment.
 * argv is a NULL-terminated list of strings, where argv[0] is the program path.
 * env is a NULL-terminated list of k=v strings specifying environment variables.
 */
pub proc_t *spawn(char *argv[], *env[]) {
    // Make three pipes.
	// Remember they are {read, write}.
    int in[2] = {0};
    int out[2] = {0};
    int err[2] = {0};
    bool ok1 = OS.pipe(in) == 0;
    bool ok2 = OS.pipe(out) == 0;
    bool ok3 = OS.pipe(err) == 0;
    if (!ok1 || !ok2 || !ok3) {
        if (ok1) {
            OS.close(in[0]); OS.close(in[1]);
        }
        if (ok2) {
            OS.close(out[0]); OS.close(out[1]);
        }
        if (ok3) {
            OS.close(err[0]); OS.close(err[1]);
        }
        return NULL;
    }

    int pid = OS.fork();
    if (pid < 0) {
        OS.close(in[0]); OS.close(in[1]);
        OS.close(out[0]); OS.close(out[1]);
        OS.close(err[0]); OS.close(err[1]);
        return NULL;
    }

    if (pid > 0) {
        // Parent
        proc_t *p = calloc(1, sizeof(proc_t));
        if (!p) {
            // oopsie
            OS.close(in[0]); OS.close(in[1]);
            OS.close(out[0]); OS.close(out[1]);
            OS.close(err[0]); OS.close(err[1]);
            OS.kill(pid, OS.SIGKILL);
            return NULL;
        }
        p->pid = pid;
        p->stdin = writer.fd(in[1]);
        p->stdout = reader.fd(out[0]);
        p->stderr = reader.fd(err[0]);
        OS.close(in[0]);
        OS.close(out[1]);
        OS.close(err[1]);
        return p;
    } else {
        // child proc
        OS.close(in[1]);
        OS.close(out[0]);
        OS.close(err[0]);
        if (OS.dup2(in[0], OS.STDIN_FILENO) < 0) {
            panic("dup2 failed");
        }
        if (OS.dup2(out[1], OS.STDOUT_FILENO) < 0) {
            panic("dup2 failed");
        }
        if (OS.dup2(err[1], OS.STDERR_FILENO) < 0) {
            panic("dup2 failed");
        }
        OS.execve(argv[0], argv, env);
        // Shouldn't reach here.
        panic("execve '%s' failed: %s\n", argv[0], strerror(errno));
    }
}

pub bool wait(proc_t *p, int *status) {
    bool ok = OS.waitpid(p->pid, status, 0) == p->pid;
	reader.free(p->stdout);
	reader.free(p->stderr);
    free(p);
    return ok;
}


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
		spawnp(f, *job++);
		nworkers++;
	}
	while (nworkers > 0) {
		OS.wait(NULL);
		nworkers--;
		if (*job) {
			spawnp(f, *job++);
			nworkers++;
		}
	}
}

void spawnp(workerfunc_t f, void *job) {
    int pid = OS.fork();
    if (pid < 0) panic("fork failed");
    if (pid > 0) return;
	exit(f(job));
}
