#import os/exec

int main() {
    char *argv[] = {"/bin/ls", "exec.c", NULL};
    char *env[] = {NULL};
    exec_result r = exec("/bin/ls", argv, env);
    printf("err = %s, exit status = %d\n", r.error, r.status);
}
