#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fs.h>

#define SIZE (1ul << 20)

#define BLOCK_SIZE (1ul << 12)

char buf[BLOCK_SIZE];

int main(void)
{
	sys_touch("1MB");
	int fd4 = sys_fopen("1MB", O_RDWR);
	sys_lseek(fd4, SIZE, SEEK_END);
	sys_fclose(fd4);
	sys_touch("8MB");
	int fd8 = sys_fopen("8MB", O_RDWR);
	sys_lseek(fd8, SIZE * 8, SEEK_END);
	sys_lseek(fd8, 0, SEEK_SET);
	for (int i = 0; i < SIZE * 8; i += BLOCK_SIZE) {
		sys_fread(fd8, buf, BLOCK_SIZE);
	}
	sys_fclose(fd8);
	return 0;
}