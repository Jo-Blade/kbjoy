#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include "ipc.h"

#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

int main()
{
	int fd;
	struct sockaddr_un addr;
	int ret;
	char buff[8192];
	struct sockaddr_un from;
	int len;

	char connectionID;
	char beginMessage[7];

	/* create new socket */
	if ((fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		_exit(1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, CLIENT_SOCK_FILE);
	unlink(CLIENT_SOCK_FILE);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		_exit(1);
	}

	/* connect to the kbjoy command socket */
	memset(&from, 0, sizeof(from));
	from.sun_family = AF_UNIX;
	strcpy(from.sun_path, SERVER_SOCK_FILE);
	if (connect(fd, (struct sockaddr *)&from, sizeof(from)) == -1)
	{
		perror("connect");
		_exit(1);
	}

	/* initiate a connection to the kbjoy output socket */
	strcpy(buff, "connect");
	if (send(fd, buff, strlen(buff) + 1, 0) == -1)
	{
		perror("send");
		_exit(1);
	}
	printf("sent: connect\n");

	/* get the response */
	if ((len = recv(fd, buff, 8192, 0)) < 0)
	{
		perror("recv");
		_exit(1);
	}
	printf("receive %d %s\n", len, buff);

	if (strncmp(buff, "accept:", 7) == 0)
	{
		/* get the connection ID */
		printf("connection ID: %d\n", (int)buff[7]);
		connectionID = buff[7];
	}
	else
	{
		printf("connection refused\n");
		_exit(1);
	}

	/* get the file descriptor of the kbjoy output socket */
	struct msghdr msg = {0};

	char m_buffer[256];
	struct iovec io = {.iov_base = m_buffer, .iov_len = sizeof(m_buffer)};
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;

	char c_buffer[256];
	msg.msg_control = c_buffer;
	msg.msg_controllen = sizeof(c_buffer);

	if (recvmsg(fd, &msg, 0) < 0)
		printf("Failed to receive message\n");

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

	unsigned char *data = CMSG_DATA(cmsg);

	printf("About to extract fd\n");
	int fd4 = *((int *)data);
	printf("Extracted fd %d\n", fd4);

	//***********************************
	/* lock the output of the kbjoy process */
	pid_t pid = getpid();
	printf("pid: %d \n", pid);

	unlink(CLIENT_SOCK_FILE);
	close(fd);

	char *file = "/run/kbjoy/out.lock";
	int fdlock;
	struct flock lock;

	fdlock = open(file, O_WRONLY);
	printf("testlock\n");
	/* Initialize the flock structure. */
	memset(&lock, 0, sizeof(lock));
	lock.l_type = F_WRLCK;
	/* Place a write lock on the file. */

	if (fcntl(fdlock, F_SETLK, &lock) < 0)
	{
		perror("lock");
		_exit(-1);
	}
	printf("file locked\n");

	//***********************************

	// ret = recv(fd4, buff, 8192, 0);
	// printf("recvlast: %s\n", buff);

	/* add a timeout of 5s */
	printf("timeout set \n");

	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	setsockopt(fd4, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

	/* skip older unread messages before the begin message */
	strcpy(beginMessage, "begin:");
	beginMessage[6] = connectionID;

	do
	{
		ret = recv(fd4, buff, 8192, 0);

		if (ret < 0)
		{
			printf("timeout triggered \n");
			perror("recv");
			_exit(1);
		}
		else
		{
			printf("skipped: %s\n", buff);
		}
	} while (strncmp(buff, beginMessage, 7));

	/* remove timeout */
	printf("timeout removed \n");
	tv.tv_sec = 0;
	setsockopt(fd4, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

	do
	{
		recv(fd4, buff, 8192, 0);
		printf("recv: %s\n", buff);

	} while (strcmp(buff, "disconnect"));

	return 0;
}
