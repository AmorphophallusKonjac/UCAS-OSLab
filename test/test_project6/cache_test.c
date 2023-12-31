#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define BUF_LEN 20

int main(void)
{
	sys_move_cursor(0, 1);
	int print_location = 1;
	printf("start first test\n");

	char print_location1[BUF_LEN];
	assert(itoa(print_location + 1, print_location1, BUF_LEN, 10) != -1);

	char print_location2[BUF_LEN];
	assert(itoa(print_location + 3, print_location2, BUF_LEN, 10) != -1);

	char *argv1[2] = { "rdfile", print_location1 };
	pid_t pid = sys_exec("rdfile", 2, argv1);

	sys_waitpid(pid);

	sys_move_cursor(0, 3);
	printf("start second test\n");

	char *argv2[2] = { "rdfile", print_location2 };
	sys_exec("rdfile", 2, argv2);

	sys_move_cursor(0, 5);
	printf("finished\n");

	return 0;
}