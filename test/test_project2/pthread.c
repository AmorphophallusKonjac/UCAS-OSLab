#include <stdio.h>
#include <unistd.h>

const int threshold = 8;

volatile int sum[2];
volatile int cnt;

struct arg {
	int idx, print_location;
} thread_arg[2];

int abs(int x)
{
	if (x < 0)
		return -x;
	return x;
}

void *adder(void *arg)
{
	struct arg *args = (struct arg *)arg;
	while (1) {
		sys_move_cursor(0, args->print_location);
		++sum[args->idx];
		printf("> [TASK] This task is child thread %d. sum[0]: %d; sum[1]: %d",
		       args->idx, sum[0], sum[1]);
		if (abs(sum[args->idx] - sum[(args->idx) ^ 1]) >= threshold) {
			__sync_fetch_and_add(&cnt, 1);
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
		sys_thread_create(&tid[i], (long)adder,
				  (void *)(&thread_arg[i]));
	}
	while (1) {
		sys_move_cursor(0, print_location);
		printf("> [TASK] This task is father thread. (%d)", cnt);
		sys_thread_yield();
	}
}