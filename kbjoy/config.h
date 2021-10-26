/* contains keyboard keys mapped on each button */

#include <stdio.h>
#include <string.h>
#include <linux/input-event-codes.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

#define ARGLEN 25

#define RUN_NONE 0
#define RUN_TOGGLE 1
#define RUN_HOLD 2

struct kbjoy_joystick
{
    int directionUP;
    int directionDOWN;
    int directionLEFT;
    int directionRIGHT;
    /* the absolute distance of the joystick */
    int defaultForce;

    /* when the runMode is active, the value of the axis is different,
    ## the player may be running in game. It emulate a semi-analog joystick */
    int RunKey;
    /* runInput define if the runMode is:
    ## "NONE" -> the run mode is disabled, you can only running or walking
    ## "TOGGLE" -> the first time, the run mode is active, the second time it is inactive
    ## "HOLD" -> the run mode is active only when the key is pressed */
    int runInput;
    int runForce;
};

struct kbjoy_gamepad
{
    int btnA;
    int btnB;
    int btnY;
    int btnX;
    int btnTR;
    int btnTR2;
    int btnTL;
    int btnTL2;
    int btnSTART;
    int btnSELECT;
    int btnTHUMBL;
    int btnTHUMBR;
    int dpadUP;
    int dpadDOWN;
    int dpadLEFT;
    int dpadRIGHT;

    struct kbjoy_joystick joystick_L;
    struct kbjoy_joystick joystick_R;
};

void InitGamepadStruct(struct kbjoy_gamepad *mapping);

void InitJoystickStruct(struct kbjoy_joystick *joystick);

int *GetGamepadBtn(char *buff, struct kbjoy_gamepad *mapping);

int *GetJoystickConfig(char *configName, struct kbjoy_joystick *joystick, int *isKeyValue);

void SetKeyboardKey(char *buff, int *gamepadKey);

int ReadConfig(int fdConfig, struct kbjoy_gamepad *mapping);

void timeout_handler(int sig);

int GotoEndLine(int fd);

int ReadArg(int fd, char buff[], int len, char **configValue);

void ConfigParse(int fd, struct kbjoy_gamepad *mapping);