#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fs.h>

#define SIZE (1 << 20)

static char buff[64];

int offset = 0;

int main(void)
{
	int fd = sys_fopen("8MB", O_RDWR);

	// write 'hello world!' * 10
	for (int i = 0; i < 5; i++) {
		sys_lseek(fd, offset, SEEK_SET);
		sys_fwrite(fd, "hello world!\n", 13);
		offset += SIZE;
	}

	offset = 0;

	// read
	for (int i = 0; i < 5; i++) {
		sys_lseek(fd, offset, SEEK_SET);
		sys_fread(fd, buff, 13);
		for (int j = 0; j < 13; j++) {
			printf("%c", buff[j]);
		}
		offset += SIZE;
	}

	sys_lseek(fd, offset / 2, SEEK_SET);
	int num = 100;
	sys_fread(fd, (char *)&num, 4);
	printf("%d\n", num);

	sys_fclose(fd);

	return 0;
}