#ifndef IPC_H
#define IPC_H

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <errno.h>
#include <string.h>

#define SERVER_SOCK_FILE "command.sock"
#define OUT_SOCK_FILE "out.sock"
#define OUT_SOCK_LOCK "out.lock"

#define LOCKED 1
#define UNLOCKED 0

int SockCreate(char *socketPath);

int SocketFDInit();

void SockaddrSet(struct sockaddr_un *addr, char *socketPath);

int PassFD(int fd, int sentFd, struct sockaddr_un *to, socklen_t tolen);

int InitLock(char *file, struct flock *lock);

int TestFileLock(char *file, int *lockPid);

int LockFile(char *file);

int SockConnect(char *socketPath);

int SendToUnconnected(int fd, char *message, struct sockaddr_un *to, socklen_t tolen);

#endif
