#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

void test_fork(int x)
{
	int ret = sys_fork();
	if (ret == 0) {
		printf("i am child: pid = %d\n", sys_getpid());
	} else if (ret > 0) {
		printf("i am parent : pid = %d  ret = %d\n", sys_getpid(), ret);
	} else {
		printf("fork error\n");
	}
}

int main(int argc, char *argv[])
{
	test_fork(3);
	return 0;
}