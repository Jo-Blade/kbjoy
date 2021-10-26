#ifndef REMAPPER_H
#define REMAPPER_H

#include <string.h>
#include <stdlib.h>

#include "ipc.h"
#include "kbjoy.h"
#include "input.h"
#include "config.h"

struct axis_data
{
    int axisID;
    int positiveDirection;
    int negativeDirection;
};

struct joystick_data
{
    int runMode;
    struct axis_data axisX;
    struct axis_data axisY;
};

int loop(int fdGamepad, int fdKeyboard, struct kbjoy_gamepad *mapping, int playerNumber, int fdOutSend);

int WriteAxis(int fdGamepad, struct axis_data *axisData, int runMode, struct kbjoy_joystick *joystickConfig);

int WriteButton(int fdGamepad, struct kbjoy_gamepad *mapping, struct input_event *readKey);

int ParseJoysticks(int fdGamepad, struct kbjoy_gamepad *mapping, struct input_event *readKey, struct joystick_data *joystickL, struct joystick_data *joystickR);

int JoystickDirectionEvent(int fdGamepad, struct axis_data *axisData, int runMode, struct kbjoy_joystick *joystickConfig, int direction, int keyValue);

int JoystickRunKeyEvent(int fdGamepad, struct kbjoy_joystick *joystickConfig, struct joystick_data *joystickData, int keyValue);

int KeyboardShortcuts(int fdKeyboard, int *rescueKey, int *keyPressedCount, int *cloneKeys,
                      int *pauseMappingKey, int *mappingRunning, int playerNumber, int fdOutSend, struct input_event *keyboardEvent);

#endif