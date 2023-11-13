#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#define LOCK_KEY 86

int main(int argc, char *argv[])
{
	sys_waitpid(atoi(argv[2]));

	int print_location = (argc == 1) ? 0 : atoi(argv[1]);
	int mutex_idx = sys_mutex_init(LOCK_KEY);
	clock_t begin = clock();
	for (int i = 0; i < 10000; ++i) {
		sys_mutex_acquire(mutex_idx);
		sys_mutex_release(mutex_idx);
		// sys_move_cursor(0, print_location);
		// printf("fine-grained-lock %d acquire and release lock %d times",
		//    sys_getpid(), i + 1);
	}
	clock_t end = clock();
	sys_move_cursor(0, print_location + 1);
	printf("fine-grained-lock %d done with %ld ticks", sys_getpid(),
	       end - begin);
	return 0;
}