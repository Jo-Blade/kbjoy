#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include "kbjoy.h"
#include "server.h"

void DaemonInit();

int main(/*int argc, char *argv[]*/)
{
    struct kbjoy kbjoyStruct;

    /* Daemonise processus */
    DaemonInit();

    /* Initialise Sockets and lock files */
    ServerInit(&kbjoyStruct);

    /* Start the infinite loop */
    ServerMain(&kbjoyStruct);

    return (EXIT_SUCCESS);
}

void DaemonInit()
{
    pid_t process_id = 0;
    pid_t sid = 0;

    /* Create child process */
    process_id = fork();

    /* Indication of fork() failure */
    if (process_id < 0)
    {
        syslog(LOG_CRIT, "Fork failed: %d (%s)\n", errno, strerror(errno));
        /* Return failure in exit status */
        exit(EXIT_FAILURE);
    }

    /*unmask the file mode */
    umask((mode_t)0133);

    /* Change the current working directory */
    chdir(WORKDIR);

    /* PARENT PROCESS. Need to kill it. */
    if (process_id > 0)
    {
        syslog(LOG_INFO, "process_id of child process %d \n", process_id);

        /* return success in exit status */
        exit(EXIT_SUCCESS);
    }

    /* set new session */
    sid = setsid();
    if (sid < 0)
    {
        /* Return failure */
        exit(EXIT_FAILURE);
    }

    /* Close stdin. stdout and stderr */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    umask((mode_t)0113);

    syslog(LOG_NOTICE, "Starting Ended");
}