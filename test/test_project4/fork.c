#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdatomic.h>
#include <pthread.h>
#include <assert.h>

void test()
{
	int y = 0;
}

void test_fork()
{
	int x = 0;
	// printl("test printl\n");
	int ret = sys_fork();
	// printl("fork ret %d\n", ret);
	if (ret == 0) {
		x = 2;
		// test();
		sys_move_cursor(0, 3);
		printf("i am child: pid = %d\n", sys_getpid());
		printf("x = %d\n", x);
	} else if (ret > 0) {
		sys_move_cursor(0, 5);
		x = 1;
		printf("i am parent: pid = %d  ret = %d\n", sys_getpid(), ret);
		printf("x = %d\n", x);
	} else {
		printf("fork error\n");
	}
}

int main(int argc, char *argv[])
{
	test_fork();
	return 0;
}
