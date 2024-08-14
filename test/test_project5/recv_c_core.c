#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

char buff[700000];

uint16_t fletcher16(uint8_t *data, int n)
{
	uint16_t sum1 = 0;
	uint16_t sum2 = 0;

	for (int i = 0; i < n; i++) {
		sum1 = (sum1 + data[i]) % 0xff;
		sum2 = (sum2 + sum1) % 0xff;
	}
	return (sum2 << 8) | sum1;
}

int main(void)
{
	int print_location = 5;
    sys_move_cursor(0, print_location);
	int len = 0;
	printf("timer: %d\n", sys_get_tick() / sys_get_timebase());
	len = sys_net_recv_protocol(buff);
	printf("timer: %d\n", sys_get_tick() / sys_get_timebase());
	printf("%d\n", len);
	printf("%d\n", fletcher16((uint8_t *)buff, len));
	return 0;
}