#ifndef INPUT_H
#define INPUT_H

#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/uinput.h>
#include <unistd.h>

int DeleteVirtualJoy(int fd);

int NewVirtualJoy(char *ctrlName);

int ControllerReset(int fd);

int KeyboardReset(int fd);

int OpenDevsInfoFile();

int FindNextKeyboard(int fdDevsFile);

#endif