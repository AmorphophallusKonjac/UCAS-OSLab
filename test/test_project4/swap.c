#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char *argv[])
{
	srand(clock());
	long data[16];
	for (int i = 0; i < 16; ++i) {
		data[i] = rand();
	}
	for (uintptr_t i = 0; i < 0x9000lu; i += 0x1000lu) {
		*(long *)i = data[i / 4096];
	}
	for (uintptr_t i = 0; i < 0x9000lu; i += 0x1000lu) {
		printf("0x%lx, %ld\n", i, data[i / 4096]);
		if (*(long *)i != data[i / 4096]) {
			printf("Error!\n");
		}
	}
	printf("Success!\n");
	return 0;
}