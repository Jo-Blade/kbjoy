#include "remapper.h"

int main(int argc, char *argv[])
{
    int ret;
    pid_t lockpid;
    int fdOutSend;
    int fdGamepad;
    int fdKeyboard;
    int fdConfig;
    struct kbjoy_gamepad mapping;

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
    if ((fdOutSend = atoi(argv[1])) < 1)
    {
        syslog(LOG_CRIT, "wrong file descriptor\n");
        return -1;
    }

    if (send(fdOutSend, "kbjoy-remapper good", 20, 0) == -1)
    {
        syslog(LOG_CRIT, "send failed: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    /* get virtual controller file descriptor */
    if ((fdGamepad = atoi(argv[2])) < 1)
    {
        syslog(LOG_CRIT, "wrong file descriptor\n");
        return -1;
    }

    /* get keyboard file descriptor */
    if ((fdKeyboard = atoi(argv[3])) < 1)
    {
        syslog(LOG_CRIT, "wrong file descriptor\n");
        return -1;
    }

    if (KeyboardReset(fdKeyboard) < 0)
    {
        syslog(LOG_CRIT, "keyboard reset failed\n");
        return -1;
    }

    /* get config file descriptor */
    if ((fdConfig = atoi(argv[4])) < 1)
    {
        syslog(LOG_CRIT, "wrong file descriptor\n");
        return -1;
    }

    /* configure keys mapping */
    if (ReadConfig(fdConfig, &mapping) < 0)
    {
        syslog(LOG_CRIT, "gamepad mapping configuration failed\n");
        return -1;
    }

    if ((*argv[5] < 1) || (*argv[5] > MAX_PLAYERS))
    {
        syslog(LOG_CRIT, "error: invalid player number\n");
        return -1;
    }

    if (loop(fdGamepad, fdKeyboard, &mapping, *argv[5], fdOutSend) < 0)
        return -1;

    return 0;
}

int loop(int fdGamepad, int fdKeyboard, struct kbjoy_gamepad *mapping, int playerNumber, int fdOutSend)
{
    struct input_event ev;

    struct input_event keyboardEvent;
    int size = sizeof(struct input_event);

    /* the shortcut ctrl+alt+supr close app and delete the virtual controller */
    int rescueKey;

    /* some keys exist twice on a keyboard, make sure they don't be pressed together */
    int cloneKeys;

    /* the shortcut super+echap will pause or start the remapping :
    ## There is two modes :
    ## keyboard-mode : the keyboard works as normally
    ## gamepad-mode : the keyboard is locked and its keys
    ##                are mapped on the virtual controller */
    int pauseMappingKey;
    int mappingRunning;

    /* count the number of keys pressed on the same time */
    int keyPressedCount;

    /* contains the value of the joysticks */
    struct joystick_data joystickL;
    struct joystick_data joystickR;

    /* initialise the joystick data struct */
    memset(&joystickL, 0, sizeof(struct joystick_data));
    memset(&joystickR, 0, sizeof(struct joystick_data));
    joystickL.axisX.axisID = ABS_X;
    joystickL.axisY.axisID = ABS_Y;
    joystickR.axisX.axisID = ABS_RX;
    joystickR.axisY.axisID = ABS_RY;

    keyPressedCount = 0;
    rescueKey = 0;
    pauseMappingKey = 0;
    cloneKeys = 0;

    mappingRunning = 1;

    /* Getting exclusive access */
    if (ioctl(fdKeyboard, EVIOCGRAB, 1) < 0)
    {
        syslog(LOG_CRIT, "cannot get exclusive access: %d (%s)\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (;;)
    {
        int ret;
        ret = 0;

        /* setting the memory for event */
        memset(&ev, 0, sizeof(struct input_event));
        ev.type = EV_KEY;

        if (read(fdKeyboard, &keyboardEvent, size) < size)
        {
            syslog(LOG_CRIT, "read keyboard failed: %d (%s)\n", errno, strerror(errno));
            /* il faudrait que je teste l'erreur pour faire la difference entre bug et clavier deco */
            exit(10);

            return -1;
        }

        if (keyboardEvent.type == EV_KEY)
        {
            /* count the total of keys pressed */
            if (keyboardEvent.value == 1)
                keyPressedCount++;
            else if ((keyboardEvent.value == 0) && (keyPressedCount > 0))
                keyPressedCount--;

            /* count the number of clone Keys: right ctrl/ left ctrl || left super/right super */
            if (keyboardEvent.code == KEY_LEFTCTRL || keyboardEvent.code == KEY_RIGHTCTRL || keyboardEvent.code == KEY_LEFTMETA || keyboardEvent.code == KEY_RIGHTMETA)
            {
                if (keyboardEvent.value == 1)
                    cloneKeys++;
                else if (keyboardEvent.value == 0 && cloneKeys > 0)
                    cloneKeys--;
            }

            /* test if a keyboard shortcut is pressed */
            if (KeyboardShortcuts(fdKeyboard, &rescueKey, &keyPressedCount, &cloneKeys, &pauseMappingKey, &mappingRunning, playerNumber,
                                  fdOutSend, &keyboardEvent) < 0)
                return -1;

            /* if the mapping is paused, don't read keys */
            if (!mappingRunning)
                continue;

            /* map the key */
            if ((ret = WriteButton(fdGamepad, mapping, &keyboardEvent)) == 1)
                continue;
            else if (ret < 0)
                return -1;

            /* map the joysticks */
            if ((ret = ParseJoysticks(fdGamepad, mapping, &keyboardEvent, &joystickL, &joystickR)) < 0)
                return -1;
        }
        else if (keyboardEvent.type == EV_SYN)
        {
            ev.type = EV_SYN;
            ev.code = SYN_REPORT;
            ev.value = 0;

            /* writing the sync report */
            if (write(fdGamepad, &ev, sizeof(struct input_event)) < 0)
            {
                syslog(LOG_CRIT, "gamepad sync report failed: %d (%s)\n", errno, strerror(errno));
                return -1;
            }
        }
    }
    return 0;
}

/* write the value of an axis */
int WriteAxis(int fdGamepad, struct axis_data *axisData, int runMode, struct kbjoy_joystick *joystickConfig)
{
    struct input_event ev;

    /* setting the memory for event */
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_ABS;
    ev.code = axisData->axisID;

    ev.value = axisData->positiveDirection - axisData->negativeDirection;

    if (runMode)
        ev.value *= joystickConfig->runForce;
    else
        ev.value *= joystickConfig->defaultForce;

    /* writing the key change */
    if (write(fdGamepad, &ev, sizeof(struct input_event)) < 0)
    {
        syslog(LOG_CRIT, "gamepad key write failed: %d (%s)\n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

/* test if a key mapped on a gamepad button is pressed, if yes write the change
## return 1 if something is done
## return 0 is nothing is done
## return -1 on error */
int WriteButton(int fdGamepad, struct kbjoy_gamepad *mapping, struct input_event *readKey)
{
    struct input_event ev;
    int keyCode;
    /* setting the memory for event */
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_KEY;
    keyCode = readKey->code;

    if (keyCode == mapping->btnA)
        ev.code = BTN_A;
    else if (keyCode == mapping->btnB)
        ev.code = BTN_B;
    else if (keyCode == mapping->btnSELECT)
        ev.code = BTN_SELECT;
    else if (keyCode == mapping->btnSTART)
        ev.code = BTN_START;
    else if (keyCode == mapping->btnTHUMBL)
        ev.code = BTN_THUMBL;
    else if (keyCode == mapping->btnTHUMBR)
        ev.code = BTN_THUMBR;
    else if (keyCode == mapping->btnTL2)
        ev.code = BTN_TL2;
    else if (keyCode == mapping->btnTL)
        ev.code = BTN_TL;
    else if (keyCode == mapping->btnTR2)
        ev.code = BTN_TR2;
    else if (keyCode == mapping->btnTR)
        ev.code = BTN_TR;
    else if (keyCode == mapping->btnX)
        ev.code = BTN_X;
    else if (keyCode == mapping->btnY)
        ev.code = BTN_Y;
    else if (keyCode == mapping->dpadDOWN)
        ev.code = BTN_DPAD_DOWN;
    else if (keyCode == mapping->dpadLEFT)
        ev.code = BTN_DPAD_LEFT;
    else if (keyCode == mapping->dpadRIGHT)
        ev.code = BTN_DPAD_RIGHT;
    else if (keyCode == mapping->dpadUP)
        ev.code = BTN_DPAD_UP;
    else
        return 0;

    ev.value = readKey->value;

    /* writing the key change */
    if (write(fdGamepad, &ev, sizeof(struct input_event)) < 0)
    {
        syslog(LOG_CRIT, "gamepad button write failed: %d (%s)\n", errno, strerror(errno));
        return -1;
    }
    return 1;
}

/* test if a joystick key is pressed, if yes write the change
## return 1 if something is done
## return 0 is nothing is done
## return -1 on error */
int ParseJoysticks(int fdGamepad, struct kbjoy_gamepad *mapping, struct input_event *readKey, struct joystick_data *joystickL, struct joystick_data *joystickR)
{
    int keyCode;
    keyCode = readKey->code;

    if (keyCode == mapping->joystick_L.directionUP)
        return JoystickDirectionEvent(fdGamepad, &joystickL->axisY, joystickL->runMode, &mapping->joystick_L, 1, readKey->value);
    else if (keyCode == mapping->joystick_L.directionDOWN)
        return JoystickDirectionEvent(fdGamepad, &joystickL->axisY, joystickL->runMode, &mapping->joystick_L, -1, readKey->value);
    else if (keyCode == mapping->joystick_L.directionLEFT)
        return JoystickDirectionEvent(fdGamepad, &joystickL->axisX, joystickL->runMode, &mapping->joystick_L, -1, readKey->value);
    else if (keyCode == mapping->joystick_L.directionRIGHT)
        return JoystickDirectionEvent(fdGamepad, &joystickL->axisX, joystickL->runMode, &mapping->joystick_L, 1, readKey->value);

    else if (keyCode == mapping->joystick_R.directionUP)
        return JoystickDirectionEvent(fdGamepad, &joystickR->axisY, joystickR->runMode, &mapping->joystick_R, 1, readKey->value);
    else if (keyCode == mapping->joystick_R.directionDOWN)
        return JoystickDirectionEvent(fdGamepad, &joystickR->axisY, joystickR->runMode, &mapping->joystick_R, -1, readKey->value);
    else if (keyCode == mapping->joystick_R.directionLEFT)
        return JoystickDirectionEvent(fdGamepad, &joystickR->axisX, joystickR->runMode, &mapping->joystick_R, -1, readKey->value);
    else if (keyCode == mapping->joystick_R.directionRIGHT)
        return JoystickDirectionEvent(fdGamepad, &joystickR->axisX, joystickR->runMode, &mapping->joystick_R, 1, readKey->value);

    else if (keyCode == mapping->joystick_L.RunKey)
        return JoystickRunKeyEvent(fdGamepad, &mapping->joystick_L, joystickL, readKey->value);
    else if (keyCode == mapping->joystick_R.RunKey)
        return JoystickRunKeyEvent(fdGamepad, &mapping->joystick_R, joystickR, readKey->value);
    return 0;
}

/* called when a joystick direction is pressed */
int JoystickDirectionEvent(int fdGamepad, struct axis_data *axisData, int runMode, struct kbjoy_joystick *joystickConfig, int direction, int keyValue)
{
    int *value;

    if (direction > 0)
        value = &axisData->positiveDirection;
    else
        value = &axisData->negativeDirection;

    if (keyValue > 0)
        *value = 1;
    else
        *value = 0;

    if (WriteAxis(fdGamepad, axisData, runMode, joystickConfig) < 0)
        return -1;

    return 1;
}

/* called when the runKey key of a joystick is pressed */
int JoystickRunKeyEvent(int fdGamepad, struct kbjoy_joystick *joystickConfig, struct joystick_data *joystickData, int keyValue)
{
    if (joystickConfig->runInput == RUN_NONE)
        return 1;
    else if (joystickConfig->runInput == RUN_TOGGLE && keyValue == 1)
        joystickData->runMode = !joystickData->runMode;
    else if (joystickConfig->runInput == RUN_HOLD)
    {
        if (keyValue == 1)
            joystickData->runMode = 1;
        else if (keyValue == 0)
            joystickData->runMode = 0;
    }
    if (WriteAxis(fdGamepad, &joystickData->axisX, joystickData->runMode, joystickConfig) < 0)
        return -1;
    if (WriteAxis(fdGamepad, &joystickData->axisY, joystickData->runMode, joystickConfig) < 0)
        return -1;
    return 1;
}

/* test if a keyboard shortcut is pressed */
int KeyboardShortcuts(int fdKeyboard, int *rescueKey, int *keyPressedCount, int *cloneKeys,
                      int *pauseMappingKey, int *mappingRunning, int playerNumber, int fdOutSend, struct input_event *keyboardEvent)
{
    switch (keyboardEvent->code)
    {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
    case KEY_LEFTALT:
    case KEY_DELETE:
        if (keyboardEvent->value == 1)
            (*rescueKey)++;
        else if (keyboardEvent->value == 0 && *rescueKey > 0)
            (*rescueKey)--;

        if (*rescueKey == 3 && *keyPressedCount == 3 && *cloneKeys == 1)
        {
            syslog(LOG_INFO, "user -> 'kill shortcut' (ctrl+alt+suppr)\n");
            exit(10);
        }
        break;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
    case KEY_ESC:
        if (keyboardEvent->value == 1)
            (*pauseMappingKey)++;
        else if (keyboardEvent->value == 0 && *pauseMappingKey > 0)
            (*pauseMappingKey)--;

        if (*pauseMappingKey == 2 && *keyPressedCount == 2 && *cloneKeys == 1)
        {
            char buff[30];

            syslog(LOG_INFO, "user -> 'pause/play shortcut' (super+escape)\n");
            *mappingRunning = !(*mappingRunning);

            if (KeyboardReset(fdKeyboard) < 0)
            {
                syslog(LOG_CRIT, "keyboard reset failed");
                exit(EXIT_FAILURE);
            }

            /* Getting/Remove exclusive access */
            if (ioctl(fdKeyboard, EVIOCGRAB, *mappingRunning) < 0)
            {
                syslog(LOG_CRIT, "cannot get exclusive access: %d (%s)\n", errno, strerror(errno));
                exit(EXIT_FAILURE);
            }

            if (*mappingRunning)
                strcpy(buff, "kbjoy Player X gamepad mode");
            else
                strcpy(buff, "kbjoy Player X keyboard mode");

            buff[13] = playerNumber + 48;

            if (send(fdOutSend, buff, 29, 0) == -1)
            {
                syslog(LOG_CRIT, "send failed: %d (%s)\n", errno, strerror(errno));
                return -1;
            }
        }
    }
    return 0;
}