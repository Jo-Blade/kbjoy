#ifndef PATH_H
#define PATH_H

#define WORKDIR "/run/kbjoy"

#define PROCESS_LOCK "process.lock"

#define BUFF_LEN 256

/* MAX_PLAYERS must be < 10 (else "list" and "trigger" will not work) */
#define MAX_PLAYERS 8

#define KBJOYREMAPPER "/usr/lib/kbjoy/kbjoy-remapper"

#define DEFAULTCONFIG "/etc/kbjoy-default.map"

#define KBJOYTRIGGER "/usr/lib/kbjoy/kbjoy-trigger"

struct kbjoy
{
    /* used to read the command socket */
    int fdInRead;
    /* used to read the out socket who is used as output*/
    int fdOutRead;
    /* used to send message to the out socket */
    int fdOutSend;

    /* used to save all players connected (child process) */
    pid_t allPlayers[MAX_PLAYERS];

    /* used to save all created virtual joysticks */
    int allJoysticks[MAX_PLAYERS];

    /* used to save the trigger process (if started) */
    pid_t triggerProcess;
};

#endif
