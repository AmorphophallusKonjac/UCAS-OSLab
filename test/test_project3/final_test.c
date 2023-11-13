#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define BUF_LEN 10

int main(int argc, char *argv[])
{
	int print_location = (argc == 1) ? 0 : atoi(argv[1]);
	pid_t pidfly = sys_exec("fly", 0, 0);
	sys_taskset(pidfly, 1);

	char print_location1[BUF_LEN];
	assert(itoa(print_location + 1, print_location1, BUF_LEN, 10) != -1);
	char *argv1[2] = { "fine-grained-lock", print_location1 };
	pid_t pid1 = sys_exec(argv1[0], 2, argv1);
	sys_taskset(pid1, 1);

	char print_location2[BUF_LEN];
	assert(itoa(print_location + 3, print_location2, BUF_LEN, 10) != -1);
	char *argv2[2] = { "fine-grained-lock", print_location2 };
	pid_t pid2 = sys_exec(argv2[0], 2, argv2);
	sys_taskset(pid2, 2);

	sys_move_cursor(0, print_location);
	printf("wait fine-grained-lock.");

	sys_waitpid(pid1);
	sys_waitpid(pid2);

	sys_move_cursor(0, print_location);
	printf("final test done.       ");
	return 0;
}