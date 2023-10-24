#include <stdio.h>
#include <unistd.h>

const int threshold = 4;

volatile int sum[2];
volatile int cnt[2];
volatile int total_cnt;

struct arg {
	int idx, print_location;
} thread_arg[2];

static char blank[] = {
	"                                                                    "
};

void *adder(void *arg)
{
	struct arg *args = (struct arg *)arg;
	while (1) {
		++sum[args->idx];
		sys_move_cursor(0, args->print_location);
		printf("> [TASK] This task is child thread %d, yield (%d). sum[0]: %d; sum[1]: %d",
		       args->idx, cnt[args->idx], sum[0], sum[1]);
		if ((sum[args->idx] - sum[(args->idx) ^ 1]) >= threshold) {
			__sync_fetch_and_add(&total_cnt, 1);
			__sync_fetch_and_add(&cnt[args->idx], 1);
			sys_move_cursor(0, args->print_location);
			printf("> [TASK] This task is child thread %d, yield (%d). sum[0]: %d; sum[1]: %d",
			       args->idx, cnt[args->idx], sum[0], sum[1]);
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
		printf("> [TASK] This task is father thread. (%d)", total_cnt);
		sys_thread_yield();
	}
}