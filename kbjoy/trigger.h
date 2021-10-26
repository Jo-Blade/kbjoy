#ifndef TRIGGER_H
#define TRIGGER_H

#include <stdlib.h>
#include <linux/input.h>

#include "kbjoy.h"
#include "ipc.h"

void loop(int fdOutSend, int *fdKeyboards, int *keyboardNames, int KeyboardsLength, int triggerKey);

#endif