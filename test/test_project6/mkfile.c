#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fs.h>

#define SIZE (1ul << 23)

int main(void)
{
	sys_touch("big_file");
	int fd = sys_fopen("big_file", O_RDWR);
	sys_lseek(fd, SIZE, SEEK_END);
	sys_fclose(fd);
	return 0;
}