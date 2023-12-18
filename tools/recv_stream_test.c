/*
 * @Author: konjac
 * @Date: 2023-12-18 04:28:24
 * @LastEditors: konjac wangmingyu21a@mails.ucas.ac.cn
 * @LastEditTime: 2023-12-18 04:35:30
 * @FilePath: /OS-Lab/tools/recv_stream_test.c
 * @Description: 
 * 
 * Copyright (c) 2023 by ${git_name_email}, All Rights Reserved. 
 */
#include <stdio.h>
#include <stdlib.h>

unsigned short fletcher16(char *data, int n)
{
	unsigned short sum1 = 0;
	unsigned short sum2 = 0;

	for (int i = 0; i < n; i++) {
		sum1 = (sum1 + data[i]) % 0xff;
		sum2 = (sum2 + sum1) % 0xff;
	}
	return (sum2 << 8) | sum1;
}

int main()
{
	FILE *file;
	int file_size;
	char *buffer;
	file = fopen(
		"/home/stu/pkt-rx-tx-master/pktRxTx-Linux/ascii_hoshiguma.txt",
		"r");
	fseek(file, 0, SEEK_END); // 定位到文件末尾
	file_size = ftell(file); // 获取文件大小
	fseek(file, 0, SEEK_SET); // 重新定位到文件开头
	buffer = (char *)malloc(file_size);
	fread(buffer, 1, file_size, file);
	fclose(file);
	unsigned short val = fletcher16(buffer, file_size);
	printf("file_size %d\n", file_size);
	printf("fletcher16 result: %u\n", val);
}