#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include "ipc.h"

#include <fcntl.h>

/* malloc function */
#include <stdlib.h>

#include <string.h>

int PingCommand()
{
	int fd;
	struct sockaddr_un addr;
	char buff[8192];
	struct sockaddr_un from;
	int len;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, CLIENT_SOCK_FILE);
	unlink(CLIENT_SOCK_FILE);

	if ((fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0)
	{
		perror("fd");
		return -1;
	}

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		_exit(1);
	}

	memset(&from, 0, sizeof(from));
	from.sun_family = AF_UNIX;
	strcpy(from.sun_path, SERVER_SOCK_FILE);
	if (connect(fd, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		perror("connect");
		_exit(1);
	}

	strcpy(buff, "ping");
	if (send(fd, buff, strlen(buff) + 1, 0) == -1)
	{
		perror("send");
		_exit(1);
	}
	printf("sent: ping\n");

	if ((len = recv(fd, buff, 8192, 0)) < 0)
	{
		perror("recv");
		_exit(1);
	}
	printf("receive %d %s\n", len, buff);

	return 0;
}

int OtherCommands(char *buff)
{
	int fd;
	struct sockaddr_un from;

	memset(&from, 0, sizeof(from));
	from.sun_family = AF_UNIX;
	strcpy(from.sun_path, SERVER_SOCK_FILE);

	if ((fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0)
	{
		perror("fd");
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		perror("connect");
		_exit(1);
	}

	if (send(fd, buff, strlen(buff) + 1, 0) == -1)
	{
		perror("send");
		_exit(1);
	}
	printf("sent: %s\n", buff);
}

int main(int argc, char *argv[])
{
	int file;

	if (!strcmp(argv[1], "ping"))
		PingCommand();
	else
	{
		char command[200];
		size_t i;
		int allCharLen;

		strncpy(command, argv[1], 200);
		allCharLen = strlen(argv[1]);

		for (i = 2; i < argc; i++)
		{
			allCharLen = strlen(argv[i]) + 1 + allCharLen;

			if (allCharLen > 200)
				break;

			strcat(command, " ");
			strcat(command, argv[i]);
		}
		command[199] = '\0';

		OtherCommands(command);
	}
}
