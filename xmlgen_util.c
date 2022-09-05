#import os/misc

int main() {
    printf("%f\n", timediff());
    while (rand() / (float)RAND_MAX > 0.00000001) {
        // printf("%f\n", rand() / (float)RAND_MAX);
    }
    printf("%f\n", timediff());
}

timeval_t last = {};

/*
 * Returns diff in ms
 */
float timediff()
{
    timeval_t now = get_time();
    int32_t diff = time_usec(&now) - time_usec(&last);
    memcpy(&last, &now, sizeof(last));
    return (float)diff / 1000;
}
