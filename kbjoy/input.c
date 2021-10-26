/* This file contains all function about reading from keyboards
## and writing to controllers
*/

#include "input.h"

/* delete a virtual joystick, it is disconnected in games */
int DeleteVirtualJoy(int fd)
{
    ioctl(fd, UI_DEV_DESTROY);
    if (close(fd) < 0)
    {
        syslog(LOG_CRIT, "close virtual controller failed! -> %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}

/* Add a new virtual joystick, visible in games. Return fd */
int NewVirtualJoy(char *ctrlName)
{
    /* setting the default settings of Gamepad */
    struct uinput_user_dev uidev;
    int fd;

    /* opening of uinput */
    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        syslog(LOG_CRIT, "Opening of uinput failed! -> %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    /* setting Gamepad keys */
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_A);
    ioctl(fd, UI_SET_KEYBIT, BTN_B);
    ioctl(fd, UI_SET_KEYBIT, BTN_X);
    ioctl(fd, UI_SET_KEYBIT, BTN_Y);
    ioctl(fd, UI_SET_KEYBIT, BTN_TL);
    ioctl(fd, UI_SET_KEYBIT, BTN_TR);
    ioctl(fd, UI_SET_KEYBIT, BTN_TL2);
    ioctl(fd, UI_SET_KEYBIT, BTN_TR2);
    ioctl(fd, UI_SET_KEYBIT, BTN_START);
    ioctl(fd, UI_SET_KEYBIT, BTN_SELECT);
    ioctl(fd, UI_SET_KEYBIT, BTN_THUMBL);
    ioctl(fd, UI_SET_KEYBIT, BTN_THUMBR);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_UP);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);

    /* setting Gamepad thumbsticks */
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(fd, UI_SET_ABSBIT, ABS_RX);
    ioctl(fd, UI_SET_ABSBIT, ABS_RY);

    memset(&uidev, 0, sizeof(uidev));

    /* Name of Gamepad */
    strncpy(uidev.name, ctrlName, UINPUT_MAX_NAME_SIZE);
    /* Security if ctrlName length > UINPUT_MAX_NAME_SIZE */
    uidev.name[UINPUT_MAX_NAME_SIZE - 1] = '\0';

    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x3;
    uidev.id.product = 0x3;
    uidev.id.version = 2;

    /* Parameters of thumbsticks */
    uidev.absmax[ABS_X] = 512;
    uidev.absmin[ABS_X] = -512;
    uidev.absfuzz[ABS_X] = 0;
    uidev.absflat[ABS_X] = 15;
    uidev.absmax[ABS_Y] = 512;
    uidev.absmin[ABS_Y] = -512;
    uidev.absfuzz[ABS_Y] = 0;
    uidev.absflat[ABS_Y] = 15;
    uidev.absmax[ABS_RX] = 512;
    uidev.absmin[ABS_RX] = -512;
    uidev.absfuzz[ABS_RX] = 0;
    uidev.absflat[ABS_RX] = 16;
    uidev.absmax[ABS_RY] = 512;
    uidev.absmin[ABS_RY] = -512;
    uidev.absfuzz[ABS_RY] = 0;
    uidev.absflat[ABS_RY] = 16;

    /* writing settings */
    if (write(fd, &uidev, sizeof(uidev)) < 0)
    {
        syslog(LOG_CRIT, "NewVirtualJoy error: write -> %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    /* writing ui dev create */
    if (ioctl(fd, UI_DEV_CREATE) < 0)
    {
        syslog(LOG_CRIT, "NewVirtualJoy error: ui_dev_create -> %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    return fd;
}

/* release all keys in the controller */
/* DON'T RESET AXIS FOR THE TIME */
int ControllerReset(int fd)
{
    int btns[17] = {
        EV_KEY,
        BTN_A,
        BTN_B,
        BTN_X,
        BTN_Y,
        BTN_TL,
        BTN_TR,
        BTN_TL2,
        BTN_TR2,
        BTN_START,
        BTN_SELECT,
        BTN_THUMBL,
        BTN_THUMBR,
        BTN_DPAD_UP,
        BTN_DPAD_DOWN,
        BTN_DPAD_LEFT,
        BTN_DPAD_RIGHT};

    int i;
    struct input_event ev;

    syslog(LOG_INFO, "Reset Controller");

    /* setting the memory for event */
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_KEY;
    ev.value = 0;

    for (i = 0; i < 17; i++)
    {
        ev.code = btns[i];

        /* writing the key change */
        if (write(fd, &ev, sizeof(struct input_event)) < 0)
        {
            syslog(LOG_CRIT, "key write error: %d (%s)\n", errno, strerror(errno));
            return -1;
        }
    }

    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;

    /* writing the sync report */
    if (write(fd, &ev, sizeof(struct input_event)) < 0)
    {
        syslog(LOG_CRIT, "sync report error: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}

/* release all keys on the keyboard */
int KeyboardReset(int fd)
{
    int i;
    struct input_event ev;

    syslog(LOG_INFO, "Reset Keyboard");

    /* setting the memory for event */
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_KEY;
    ev.value = 0;

    for (i = 0; i < 249; i++)
    {
        ev.code = i;

        /* writing the key change */
        if (write(fd, &ev, sizeof(struct input_event)) < 0)
        {
            syslog(LOG_CRIT, "key write error: %d (%s)\n", errno, strerror(errno));
            return -1;
        }
    }

    memset(&ev, 0, sizeof(struct input_event));
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;

    /* writing the sync report */
    if (write(fd, &ev, sizeof(struct input_event)) < 0)
    {
        syslog(LOG_CRIT, "sync report error: %d (%s)\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}

/* open the devices info file, return fd or -1 on error */
int OpenDevsInfoFile()
{
    int fd;
    if ((fd = open("/proc/bus/input/devices", O_RDONLY)) == -1)
    {
        syslog(LOG_CRIT, "Open /proc/bus/input/devices failed: %d (%s)\n", errno, strerror(errno));
        return -1;
    }
    return fd;
}

/* find next connected keyboard
## return number of the next /dev/input/eventX who is a keyboard
## return <-1 on error
## return -1 if the file is ended */
int FindNextKeyboard(int fdDevsFile)
{
    char *pHandlers, *pEV;

    do
    {
        int rd, ret;
        char dev[4096];

        memset(dev, 0, sizeof(dev));
        for (rd = 0; rd < 4096 - 1; rd++)
        {
            if ((ret = read(fdDevsFile, &dev[rd], sizeof(char))) < 1)
            {
                if (ret == 0)
                    return -1;

                syslog(LOG_CRIT, "read failed: %d (%s)\n", errno, strerror(errno));
                return -2;
            }
            if (dev[rd] == '\n' && dev[rd - 1] == '\n')
                break;
        }

        pHandlers = strstr(dev, "Handlers=");
        pEV = strstr(dev, "EV=");

        if (pHandlers == NULL || pEV == NULL)
        {
            syslog(LOG_CRIT, "string \"Handlers=\" or \"EV=\" not found: %d (%s)\n",
                   errno, strerror(errno));
            return -2;
        }

    } while (strncmp(pEV + 3, "120013", 6));

    pHandlers = strstr(pHandlers, "event");
    if (pHandlers)
        return (atoi(&pHandlers[5]));
    else
        syslog(LOG_CRIT, "[ERR] Abnormal keyboard event device\n");
    return -2;
}
