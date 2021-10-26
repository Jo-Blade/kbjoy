/* This file contains all functions to read the config file and modify
## the mapping of the keys */

#include "config.h"

/* set all gamepad button to 0 (unasigned key) */
void InitGamepadStruct(struct kbjoy_gamepad *mapping)
{
    mapping->btnA = 0;
    mapping->btnB = 0;
    mapping->btnSELECT = 0;
    mapping->btnSTART = 0;
    mapping->btnTHUMBL = 0;
    mapping->btnTHUMBR = 0;
    mapping->btnTL2 = 0;
    mapping->btnTL = 0;
    mapping->btnTR2 = 0;
    mapping->btnTR = 0;
    mapping->btnX = 0;
    mapping->btnY = 0;
    mapping->dpadDOWN = 0;
    mapping->dpadLEFT = 0;
    mapping->dpadRIGHT = 0;
    mapping->dpadUP = 0;

    InitJoystickStruct(&mapping->joystick_L);
    InitJoystickStruct(&mapping->joystick_R);
}

/* set all joystick values to null */
void InitJoystickStruct(struct kbjoy_joystick *joystick)
{
    joystick->defaultForce = 256;
    joystick->directionDOWN = 0;
    joystick->directionLEFT = 0;
    joystick->directionRIGHT = 0;
    joystick->directionUP = 0;
    joystick->runForce = 256;
    joystick->runInput = RUN_NONE;
    joystick->RunKey = 0;
}

/* this function return a pointer to the button in the struct
## that match with the name sent */
int *GetGamepadBtn(char *btnName, struct kbjoy_gamepad *mapping)
{
    if (!strcmp(btnName, "BTN_A"))
        return &mapping->btnA;
    if (!strcmp(btnName, "BTN_B"))
        return &mapping->btnB;
    if (!strcmp(btnName, "BTN_Y"))
        return &mapping->btnY;
    if (!strcmp(btnName, "BTN_X"))
        return &mapping->btnX;

    if (!strcmp(btnName, "BTN_TR"))
        return &mapping->btnTR;
    if (!strcmp(btnName, "BTN_TR2"))
        return &mapping->btnTR2;
    if (!strcmp(btnName, "BTN_TL"))
        return &mapping->btnTL;
    if (!strcmp(btnName, "BTN_TL2"))
        return &mapping->btnTL2;

    if (!strcmp(btnName, "BTN_START"))
        return &mapping->btnSTART;
    if (!strcmp(btnName, "BTN_SELECT"))
        return &mapping->btnSELECT;
    if (!strcmp(btnName, "BTN_THUMBL"))
        return &mapping->btnTHUMBL;
    if (!strcmp(btnName, "BTN_THUMBR"))
        return &mapping->btnTHUMBR;

    if (!strcmp(btnName, "BTN_DPAD_UP"))
        return &mapping->dpadUP;
    if (!strcmp(btnName, "BTN_DPAD_DOWN"))
        return &mapping->dpadDOWN;
    if (!strcmp(btnName, "BTN_DPAD_LEFT"))
        return &mapping->dpadLEFT;
    if (!strcmp(btnName, "BTN_DPAD_RIGHT"))
        return &mapping->dpadRIGHT;

    syslog(LOG_INFO, "unknow gamepad button: '%s'\n", btnName);
    return NULL;
}

/* return a pointer to the specified joystick config value to modify it
## isKeyValue = 1 if the config member needs a keyboard code
## isKeyValue = 0 if the config member needs an int value */
int *GetJoystickConfig(char *configName, struct kbjoy_joystick *joystick, int *isKeyValue)
{
    *isKeyValue = 1;
    if (!strcmp(configName, "UP"))
        return &joystick->directionUP;
    if (!strcmp(configName, "DOWN"))
        return &joystick->directionDOWN;
    if (!strcmp(configName, "LEFT"))
        return &joystick->directionLEFT;
    if (!strcmp(configName, "RIGHT"))
        return &joystick->directionRIGHT;
    if (!strcmp(configName, "RUN_KEY"))
        return &joystick->RunKey;

    *isKeyValue = 0;

    if (!strcmp(configName, "RUN_INPUT"))
        return &joystick->runInput;
    if (!strcmp(configName, "RUN_FORCE"))
        return &joystick->runForce;
    if (!strcmp(configName, "FORCE"))
        return &joystick->defaultForce;

    syslog(LOG_INFO, "unknow joystick configuration: '%s'\n", configName);
    return NULL;
}

/* read the config file and configure the kbjoy_gamepad struct */
int ReadConfig(int fdConfig, struct kbjoy_gamepad *mapping)
{
    struct flock lock;

    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_RDLCK;

    /* prepare timeout of 5 seconds */
    if (signal(SIGALRM, timeout_handler) == SIG_ERR)
        return (-1);
    alarm(5);

    /* lock the config file for reading */
    if (fcntl(fdConfig, F_SETLKW, &lock) < 0)
    {
        syslog(LOG_CRIT, "cannot lock config file for reading: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    /* remove the timeout */
    alarm(0);

    /* set all gamepad keys to 0 */
    InitGamepadStruct(mapping);

    /* get configuration, only buttons for the time, no axis */
    ConfigParse(fdConfig, mapping);

    /* unlock the config file before reading */
    lock.l_type = F_UNLCK;
    if (fcntl(fdConfig, F_SETLK, &lock) < 0)
    {
        syslog(LOG_CRIT, "cannot lock config file for reading: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}

void timeout_handler(int sig)
{
    if (sig == SIGALRM)
        syslog(LOG_INFO, "signal timeout\n");
}

/* ignore all remaining characters in the current line
## return 1 on sucess
## return 0 for error or EOF */
int GotoEndLine(int fd)
{
    char readChar;
    int ret;

    do
    {
        if ((ret = read(fd, &readChar, sizeof(char))) < 1)
        {
            if (ret < 0)
                syslog(LOG_CRIT, "read failed: %d (%s)\n", errno, strerror(errno));

            return 0;
        }

    } while (readChar != '\n');

    return 1;
}

/* read and copy the next Arg in the buffer
## len is the char len without the null ending char
## return -2 on error or when the file is finished
## return -1 if the config btn section is ended
## else return the char len
## configValue contains a pointer to the second part of the argument (after a ':') */
int ReadArg(int fd, char buff[], int len, char **configValue)
{
    int i;

    for (i = 0; i < len; i++)
    {
        int ret;
        char readChar;

        /* read the next char */
        if ((ret = read(fd, &readChar, sizeof(char))) < 1)
        {
            if (ret < 0)
                syslog(LOG_CRIT, "read failed: %d (%s)\n", errno, strerror(errno));
            return -2;
        }

        switch (readChar)
        {
            /* detect the separator between config arg name and value */
        case ':':
            if (configValue == NULL)
                break;
            buff[i] = '\0';
            *configValue = &buff[i + 1];
            continue;
            /* ignore blank spaces, go to next iteration of for */
        case ' ':
        case '\t':
            i--;
            continue;

            /* skip the line containing the symbol # (user comments) */
        case '#':
            if (GotoEndLine(fd))
                return 0;
            else
                return -2;

            /* '\n' meaning the arg is ended */
        case '\n':
            buff[i] = '\0';
            /* if the config section is ended return -1 */
            if (!strncmp(buff, "[end-section]", 13))
                return -1;

            return i;
        }

        buff[i] = readChar;
    }

    buff[i] = '\0';
    /* if the config section is ended return -1 */
    if (!strncmp(buff, "[end-section]", 13))
        return -1;

    return i;
}

/* extract config from file and save it */
void ConfigParse(int fd, struct kbjoy_gamepad *mapping)
{
    char buff[ARGLEN + 1];
    int len;

    /* while there is something read */
    while (ReadArg(fd, buff, ARGLEN, NULL) > -2)
    {
        /* if we are in the btns-section */
        if (!strncmp(buff, "[btns-section]", 14))
        {
            char *keyboardKeyCode;
            keyboardKeyCode = NULL;

            /* while there is something read and the section is not finished */
            while ((len = ReadArg(fd, buff, ARGLEN, &keyboardKeyCode)) > -1)
            {
                if (len > 0 && keyboardKeyCode != NULL)
                {
                    int *gamepadKey;

                    if ((gamepadKey = GetGamepadBtn(buff, mapping)) != NULL)
                        SetKeyboardKey(keyboardKeyCode, gamepadKey);
                }
                keyboardKeyCode = NULL;
            }
        }
        /* joystickL configuration */
        if (!strncmp(buff, "[joyL-section]", 14) || !strncmp(buff, "[joyR-section]", 14))
        {
            struct kbjoy_joystick *joystick;
            char *configValue;
            configValue = NULL;

            if (buff[4] == 'L')
                joystick = &mapping->joystick_L;
            else
                joystick = &mapping->joystick_R;

            /* while there is something read and the section is not finished */
            while ((len = ReadArg(fd, buff, ARGLEN, &configValue)) > -1)
            {
                if (len > 0 && configValue != NULL)
                {
                    int *joystickConfig;
                    int isKeyValue;

                    if ((joystickConfig = GetJoystickConfig(buff, joystick, &isKeyValue)) != NULL)
                    {
                        if (isKeyValue)
                            SetKeyboardKey(configValue, joystickConfig);
                        else if (!strncmp(configValue, "NONE", 4))
                            *joystickConfig = RUN_NONE;
                        else if (!strncmp(configValue, "TOGGLE", 6))
                            *joystickConfig = RUN_TOGGLE;
                        else if (!strncmp(configValue, "HOLD", 4))
                            *joystickConfig = RUN_HOLD;
                        else if ((*joystickConfig = atoi(configValue)) < 1)
                        {
                            syslog(LOG_CRIT, "wrong int value: %s\n", configValue);
                            *joystickConfig = 0;
                        }
                    }
                }
                configValue = NULL;
            }
        }
    }
}

/* turn a keyboard key name into a key code and assign it to gamepadKey
## gamepadKey is a pointer to a btn in the kbjoy_gamepad struct */
void SetKeyboardKey(char *keyName, int *gamepadKey)
{
    /* if the keyName begin by the antislash, enter numerical code */
    if (*keyName == '\\')
    {
        keyName++;
        if ((*gamepadKey = atoi(keyName)) < 1)
        {
            syslog(LOG_CRIT, "wrong key code: %s\n", keyName);
            *gamepadKey = 0;
        }
    }

    /* turn key names into key codes */
    else if (!strcmp(keyName, "KEY_A"))
        *gamepadKey = KEY_A;
    else if (!strcmp(keyName, "KEY_B"))
        *gamepadKey = KEY_B;
    else if (!strcmp(keyName, "KEY_C"))
        *gamepadKey = KEY_C;
    else if (!strcmp(keyName, "KEY_D"))
        *gamepadKey = KEY_D;
    else if (!strcmp(keyName, "KEY_E"))
        *gamepadKey = KEY_E;
    else if (!strcmp(keyName, "KEY_F"))
        *gamepadKey = KEY_F;
    else if (!strcmp(keyName, "KEY_G"))
        *gamepadKey = KEY_G;
    else if (!strcmp(keyName, "KEY_H"))
        *gamepadKey = KEY_H;
    else if (!strcmp(keyName, "KEY_I"))
        *gamepadKey = KEY_I;
    else if (!strcmp(keyName, "KEY_J"))
        *gamepadKey = KEY_J;
    else if (!strcmp(keyName, "KEY_K"))
        *gamepadKey = KEY_K;
    else if (!strcmp(keyName, "KEY_L"))
        *gamepadKey = KEY_L;
    else if (!strcmp(keyName, "KEY_M"))
        *gamepadKey = KEY_M;
    else if (!strcmp(keyName, "KEY_N"))
        *gamepadKey = KEY_N;
    else if (!strcmp(keyName, "KEY_O"))
        *gamepadKey = KEY_O;
    else if (!strcmp(keyName, "KEY_P"))
        *gamepadKey = KEY_P;
    else if (!strcmp(keyName, "KEY_Q"))
        *gamepadKey = KEY_Q;
    else if (!strcmp(keyName, "KEY_R"))
        *gamepadKey = KEY_R;
    else if (!strcmp(keyName, "KEY_S"))
        *gamepadKey = KEY_S;
    else if (!strcmp(keyName, "KEY_T"))
        *gamepadKey = KEY_T;
    else if (!strcmp(keyName, "KEY_U"))
        *gamepadKey = KEY_U;
    else if (!strcmp(keyName, "KEY_V"))
        *gamepadKey = KEY_V;
    else if (!strcmp(keyName, "KEY_W"))
        *gamepadKey = KEY_W;
    else if (!strcmp(keyName, "KEY_X"))
        *gamepadKey = KEY_X;
    else if (!strcmp(keyName, "KEY_Y"))
        *gamepadKey = KEY_Y;
    else if (!strcmp(keyName, "KEY_Z"))
        *gamepadKey = KEY_Z;

    else if (!strcmp(keyName, "KEY_0"))
        *gamepadKey = KEY_0;
    else if (!strcmp(keyName, "KEY_1"))
        *gamepadKey = KEY_1;
    else if (!strcmp(keyName, "KEY_2"))
        *gamepadKey = KEY_2;
    else if (!strcmp(keyName, "KEY_3"))
        *gamepadKey = KEY_3;
    else if (!strcmp(keyName, "KEY_4"))
        *gamepadKey = KEY_4;
    else if (!strcmp(keyName, "KEY_5"))
        *gamepadKey = KEY_5;
    else if (!strcmp(keyName, "KEY_6"))
        *gamepadKey = KEY_6;
    else if (!strcmp(keyName, "KEY_7"))
        *gamepadKey = KEY_7;
    else if (!strcmp(keyName, "KEY_8"))
        *gamepadKey = KEY_8;
    else if (!strcmp(keyName, "KEY_9"))
        *gamepadKey = KEY_9;

    else if (!strcmp(keyName, "KEY_UP"))
        *gamepadKey = KEY_UP;
    else if (!strcmp(keyName, "KEY_DOWN"))
        *gamepadKey = KEY_DOWN;
    else if (!strcmp(keyName, "KEY_LEFT"))
        *gamepadKey = KEY_LEFT;
    else if (!strcmp(keyName, "KEY_RIGHT"))
        *gamepadKey = KEY_RIGHT;
    else
    {
        syslog(LOG_INFO, "unknow keyboard key: '%s'\n", keyName);
        *gamepadKey = 0;
    }
}