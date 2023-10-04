#include <stdio.h>
#include <unistd.h>

int sum[2];
int cnt;

struct arg {
    int idx, print_location;
} thread_arg[2];

void *adder(void *arg) {
    struct arg *args = (struct arg *) arg;
    while (1) {
        sys_move_cursor(0, args->print_location);
        printf("> [TASK] This task is child thread %d. (%d)", args->idx, ++sum[args->print_location]);
        if (sum[args->print_location] - sum[(args->print_location) ^ 1] >= 20) {
            ++cnt;
            sys_thread_yield();
        }
    }
}

int main(void)
{
    int print_location = 6;
    int tid[2];
    for (int i = 0; i < 2; ++i) {
        thread_arg[i].idx = i;
        thread_arg[i].print_location = print_location + i + 1;
        sys_thread_create(&tid[i], (long)adder, (void *)(&thread_arg[i]));
    }
    while (1) {
        sys_move_cursor(0, print_location);
        printf(">[TASK] This task is father thread. (%d)", cnt);
        sys_thread_yield();
    }
}