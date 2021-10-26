/* this process send the path of the keyboard device
## who had pressed the specified trigger key */

#include "trigger.h"

int main(int argc, char *argv[])
{
    int ret;
    pid_t lockpid;
    int fdOutSend;
    int *fdKeyboards;
    int *keyboardNames;
    int i;
    int triggerKey;

    /* remove the compiler warning */
    if (argc > 0)
        argc = argc;

    /* prevent accidentally starting by the user */
    if ((ret = TestFileLock(WORKDIR "/process.lock", &lockpid)) < 0)
        return -1;

    if (ret == UNLOCKED)
    {
        syslog(LOG_CRIT, "kbjoy daemon is not running !\n");
        return -1;
    }

    if (lockpid != getppid())
    {
        syslog(LOG_CRIT, "this program mustn't be launched by the user\n");
        return -1;
    }

    /* send success message to the kbjoy client */
    if ((triggerKey = atoi(argv[2])) < 1)
    {
        syslog(LOG_CRIT, "wrong trigger keycode\n");
        return -1;
    }

    /* send success message to the kbjoy client */
    if ((fdOutSend = atoi(argv[1])) < 1)
    {
        syslog(LOG_CRIT, "wrong file descriptor\n");
        return -1;
    }

    if (send(fdOutSend, "kbjoy-trigger good", 20, 0) == -1)
    {
        syslog(LOG_CRIT, "send failed: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    if (send(fdOutSend, "list-start", 11, 0) < 0)
    {
        syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "list-start", errno, strerror(errno));
        return -1;
    }

    /* open all keyboards for reading */
    fdKeyboards = malloc((argc - 3) * sizeof(int));
    keyboardNames = malloc((argc - 3) * sizeof(int));

    for (i = 3; i < argc; i++)
    {
        int eventNumber;
        char buff[20];
        int flags;

        /* atoi return 0 if an error has occured,
        ## so we need to make a separate test */
        if (*argv[i] == '0')
            eventNumber = 0;
        else if ((eventNumber = atoi(argv[i])) < 1)
        {
            syslog(LOG_CRIT, "wrong event number\n");
            return -1;
        }

        /* make sure there is no more characters */
        if (eventNumber < 100)
            sprintf(buff, "/dev/input/event%d", eventNumber);
        keyboardNames[i - 3] = eventNumber;

        if (send(fdOutSend, buff, 20, 0) < 0)
            syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));

        if ((fdKeyboards[i - 3] = open(buff, O_RDONLY)) < 0)
        {
            syslog(LOG_CRIT, "cannot open keyboard : %d (%s)\n", errno, strerror(errno));
            return -1;
        }

        /* configure the file descriptor to be nonblock */
        flags = fcntl(fdKeyboards[i - 3], F_GETFL, 0);
        fcntl(fdKeyboards[i - 3], F_SETFL, flags | O_NONBLOCK);
    }

    if (send(fdOutSend, "list-end", 9, 0) < 0)
    {
        syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", "list-end", errno, strerror(errno));
        return -1;
    }

    loop(fdOutSend, fdKeyboards, keyboardNames, argc - 3, triggerKey);
    return 0;
}

void loop(int fdOutSend, int *fdKeyboards, int *keyboardNames, int KeyboardsLength, int triggerKey)
{
    for (;;)
    {
        int i;

        for (i = 0; i < KeyboardsLength; i++)
        {
            struct input_event keyboardEvent;
            int size = sizeof(struct input_event);

            if (read(fdKeyboards[i], &keyboardEvent, size) < size)
            {
                if (errno == EAGAIN)
                    continue;

                syslog(LOG_CRIT, "read keyboard failed: %d (%s)\n", errno, strerror(errno));
                exit(10);
            }

            if (keyboardEvent.type == EV_KEY && keyboardEvent.code == triggerKey && keyboardEvent.value != 2)
            {
                char buff[30];
                /* make sure there is no more characters */
                if (keyboardNames[i] < 100)
                    sprintf(buff, "trigger:%d;/dev/input/event%d", keyboardEvent.value,
                            keyboardNames[i]);

                if (send(fdOutSend, buff, 30, 0) < 0)
                    syslog(LOG_CRIT, "send failed, message= %s : %d (%s)\n", buff, errno, strerror(errno));
            }
        }
    }
}