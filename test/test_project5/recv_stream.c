/*
 * @Author: konjac
 * @Date: 2023-12-17 22:07:26
 * @LastEditors: konjac wangmingyu21a@mails.ucas.ac.cn
 * @LastEditTime: 2023-12-18 04:45:47
 * @FilePath: /OS-Lab/test/test_project5/recv_stream.c
 * @Description: 
 * 
 * Copyright (c) 2023 by wangmingyu21a@mails.ucas.ac.cn, All Rights Reserved. 
 */
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

char buff[100000];

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
	int nbytes = 91096 + 4;
	sys_net_recv_stream(buff, nbytes);
	int size = *(int *)buff;
	memcpy((uint8_t *)buff, (uint8_t *)buff + 4, size - 4);
	uint16_t val = fletcher16((uint8_t *)buff, size - 4);
	sys_move_cursor(0, print_location);
	printf("file_size %d\n", size - 4);
	printf("fletcher16 result: %d\n", val);
	return 0;
}