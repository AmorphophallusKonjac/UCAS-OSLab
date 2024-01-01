#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fs.h>
#include <time.h>

#define SIZE (1ul << 20)

#define BLOCK_SIZE (1ul << 12)

char buf[BLOCK_SIZE];

int main(int argc, char **argv)
{
	int print_location = (argc == 1) ? 0 : atoi(argv[1]);
	clock_t begin = clock();
	int fd = sys_fopen("1MB", O_RDWR);
	for (int i = 0; i < SIZE; i += BLOCK_SIZE) {
		sys_fread(fd, buf, BLOCK_SIZE);
	}
	sys_fclose(fd);
	clock_t end = clock();
	sys_move_cursor(0, print_location);
	printf("read done with %ld ticks\n", end - begin);
	return 0;
}