/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define SHELL_BEGIN 20

#define BUF_SIZE 512

#define SYSCALL_NUM 96

struct pair {
	char name[32];
	long (*parser)();
} command[SYSCALL_NUM];

/*****************************************************************************************/

void execParser(int argc, char **argv)
{
	int needWait = 1;
	int pid = 0;
	if (strcmp(argv[argc - 1], "&") == 0) {
		needWait = 0;
		--argc;
	}
#ifdef S_CORE
	// if (argc == 1) {
	// 	printf("Error: Miss Program ID\n");
	// 	return;
	// }
	// sys_call();
#else
	if (argc == 1) {
		printf("Error: Miss Task Name!\n");
		return;
	}
	pid = sys_exec(argv[1], argc - 1, argv + 1);
#endif
	if (!pid) {
		printf("Info: execute %s failed.\n", argv[1]);
		return;
	} else {
		printf("Info: execute %s successfully, pid = %d ...\n", argv[1],
		       pid);
	}
	if (needWait) {
		sys_waitpid(pid);
	}
}

void killParser(int argc, char **argv)
{
	if (argc < 2) {
		printf("Error: Miss pid!\n");
		return;
	}
	pid_t pid = atoi(argv[1]);
	if (!pid) {
		printf("Error: Unknown pid %s!\n", argv[1]);
		return;
	}
	sys_kill(pid);
}

void clearParser(int argc, char **argv)
{
	sys_screen_clear();
	sys_move_cursor(0, SHELL_BEGIN);
	printf("------------------- COMMAND -------------------\n");
}

void tasksetParser(int argc, char **argv)
{
	if (argc < 2) {
		printf("Error: Miss argument!\n");
		return;
	}
	if (strcmp(argv[1], "-p") == 0) {
		if (argc < 3) {
			printf("Error: Miss mask and pid!\n");
			return;
		}
		if (argc < 4) {
			printf("Error: Miss pid!\n");
			return;
		}
		if (argc > 4) {
			printf("Error: too many arguments!\n");
			return;
		}
		int mask = atoi(argv[2]);
		pid_t pid = atoi(argv[3]);
		if (!mask) {
			printf("Error: Unknown mask!\n");
			return;
		}
		if (!pid) {
			printf("Error: Unknown pid!\n");
			return;
		}
		if (sys_taskset(pid, mask) == 0) {
			printf("Info: set task %d mask to %d...\n", pid, mask);
		} else {
			printf("Error: could not find task %d!\n", pid);
		}
	} else {
		int needWait = 1;
		if (strcmp(argv[argc - 1], "&") == 0) {
			needWait = 0;
			--argc;
		}
		if (argc < 3) {
			printf("Error: Miss task name!\n");
			return;
		}
		int mask = atoi(argv[1]);
		pid_t pid = sys_exec(argv[2], argc - 2, argv + 2);
		if (!pid) {
			printf("Info: execute %s failed.\n", argv[2]);
			return;
		}
		sys_taskset(pid, mask);
		printf("Info: execute %s successfully, pid = %d , mask = %d...\n",
		       argv[2], pid, mask);
		if (needWait) {
			sys_waitpid(pid);
		}
	}
}

/*****************************************************************************************/

void psParser(int argc, char **argv)
{
	if (argc > 1) {
		printf("Error: Too many arguments!\n");
		return;
	}
	sys_ps();
}

void parseArg(char *arg, int *argc, char **argv)
{
	*argc = 0;
	argv[*argc] = strtok(arg, " ");
	if (argv[*argc] == NULL) {
		return;
	} else {
		*argc = 1;
	}
	while ((argv[*argc] = strtok(NULL, " ")) != NULL) {
		*argc = *argc + 1;
	}
}

int getCommandId(char *action)
{
	int ret = -1;
	for (int i = 0; i < SYSCALL_NUM; ++i) {
		if (strcmp(action, command[i].name) == 0) {
			return i;
		}
	}
	return ret;
}

void clientEntrypoint(char *arg)
{
	int argc = 0;
	char *argv[BUF_SIZE];
	int commandId;
	parseArg(arg, &argc, argv);
	if (!argc)
		return;
	if ((commandId = getCommandId(argv[0])) == -1) {
		printf("Error: Unknown Command %s!\n", argv[0]);
		return;
	}
	command[commandId].parser(argc, argv);
}

void initSyscall()
{
	strcpy(command[0].name, "exec");
	command[0].parser = (long (*)())execParser;

	strcpy(command[1].name, "kill");
	command[1].parser = (long (*)())killParser;

	strcpy(command[2].name, "ps");
	command[2].parser = (long (*)())psParser;

	strcpy(command[3].name, "clear");
	command[3].parser = (long (*)())clearParser;

	strcpy(command[4].name, "taskset");
	command[4].parser = (long (*)())tasksetParser;
}

int main(void)
{
	initSyscall();
	char buf[BUF_SIZE + 1];
	int ch;
	int len = 0;
	sys_move_cursor(0, SHELL_BEGIN);
	printf("------------------- COMMAND -------------------\n");
	printf("> root@UCAS_OS# ");

	while (1) {
		// TODO [P3-task1]: call syscall to read UART port
		ch = sys_getchar();
		if (ch == -1)
			continue;
		if (ch == '\r') {
			printf("\n");
			clientEntrypoint(buf);
			len = 0;
			buf[len] = '\0';
			printf("> root@UCAS_OS# ");
		} else {
			if (ch == 8 || ch == 127) {
				if (len) {
					sys_backspace();
					buf[--len] = '\0';
				}
			} else {
				if (len == BUF_SIZE) {
					printf("\nError: command too long!\n> root@UCAS_OS# ");
					len = 0;
					buf[len] = '\0';
					continue;
				}
				buf[len++] = ch;
				buf[len] = '\0';
				printf("%c", ch);
			}
		}
		// TODO [P3-task1]: parse input
		// note: backspace maybe 8('\b') or 127(delete)

		// TODO [P3-task1]: ps, exec, kill, clear

		/************************************************************/
		/* Do not touch this comment. Reserved for future projects. */
		/************************************************************/
	}

	return 0;
}
