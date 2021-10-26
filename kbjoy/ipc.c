/* This file contains all functions about inter processus communication
##  Including socket functions and locking functions
*/

#include "ipc.h"

/* Create new unix domain socket and bind it, return fd */
int SockCreate(char *socketPath)
{
    int fd;
    struct sockaddr_un addr;

    if ((fd = SocketFDInit()) < 0)
        return -1;

    SockaddrSet(&addr, socketPath);
    unlink(socketPath);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        syslog(LOG_CRIT, "socket '%s' bind failed: %d (%s)\n", socketPath, errno, strerror(errno));
        return -1;
    }

    return fd;
}

/* Create unix domain Socket, return fd */
int SocketFDInit()
{
    int fd;

    if ((fd = socket(PF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
        syslog(LOG_CRIT, "socket fd: %d (%s)\n", errno, strerror(errno));

        return -1;
    }
    return fd;
}

/* Init sockadrr with the path of the socket */
void SockaddrSet(struct sockaddr_un *addr, char *socketPath)
{
    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;

    strcpy(addr->sun_path, socketPath);
}

/* Send a file descriptor over a socket
## I don't understand everything in this function */
int PassFD(int fd, int sentFd, struct sockaddr_un *to, socklen_t tolen)
{
    struct msghdr msg = {0};
    struct iovec io;
    /* = {.iov_base = "ABC", .iov_len = 3}; */
    /* struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); */
    struct cmsghdr *cmsg;

    char buf[CMSG_SPACE(sizeof(sentFd))];
    memset(buf, '\0', sizeof(buf));

    io.iov_base = "ABC";
    io.iov_len = 3;

    msg.msg_name = (struct sockaddr *)to;
    msg.msg_namelen = tolen;

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    cmsg = CMSG_FIRSTHDR(&msg);

    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(sentFd));

    *((int *)CMSG_DATA(cmsg)) = sentFd;

    msg.msg_controllen = CMSG_SPACE(sizeof(sentFd));

    if (sendmsg(fd, &msg, 0) < 0)
    {
        syslog(LOG_CRIT, "failed to send message: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    syslog(LOG_INFO, "fd transmi : %d \n", sentFd);
    return 0;
}

/* Create file descriptor and set the flock struct, return fd */
int InitLock(char *file, struct flock *lock)
{
    int fd;

    if ((fd = open(file, O_WRONLY)) < 0)
    {
        syslog(LOG_CRIT, "open failed: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    memset(lock, 0, sizeof(*lock));
    lock->l_type = F_WRLCK;

    return fd;
}

/* Test if a file is locked
## lockPid return the pid of the lock process (if the file is locked) */
int TestFileLock(char *file, int *lockPid)
{
    int fd;
    struct flock lock;

    syslog(LOG_INFO, "test of locking: %s \n", file);

    if ((fd = InitLock(file, &lock)) < 0)
        return -1;

    if (fcntl(fd, F_GETLK, &lock) < 0)
    {
        syslog(LOG_CRIT, "lock test failed: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    close(fd);

    if ((int)lock.l_type != F_UNLCK)
    {
        syslog(LOG_INFO, "file is already locked: %s \n", file);

        if (lockPid != NULL)
            *lockPid = lock.l_pid;
        
        return LOCKED;
    }

    return UNLOCKED;
}

/* Lock an unlocked file, return fd */
int LockFile(char *file)
{
    int fd;
    struct flock lock;

    syslog(LOG_INFO, "locking: %s \n", file);

    if ((fd = InitLock(file, &lock)) < 0)
        return -1;

    if (fcntl(fd, F_SETLK, &lock) < 0)
    {
        syslog(LOG_CRIT, "locking failed: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    return fd;
}

/* Connect to the specified socket, return fd */
int SockConnect(char *socketPath)
{
    int fd;
    struct sockaddr_un addr;

    if ((fd = SocketFDInit()) < 0)
        return -1;

    SockaddrSet(&addr, socketPath);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        syslog(LOG_CRIT, "socket '%s' connection failed: %d (%s)\n", socketPath, errno, strerror(errno));
        return -1;
    }

    return fd;
}

/* Send a message to a socket in unconnected mode */
int SendToUnconnected(int fd, char *message, struct sockaddr_un *to, socklen_t tolen)
{
    if ((sendto(fd, message, strlen(message) + 1, 0, (struct sockaddr *)to, tolen)) < 0)
    {
        syslog(LOG_CRIT, "sendto failed, message= %s : %d (%s)\n", message, errno, strerror(errno));
        return -1;
    }

    return 0;
}