# kbjoy
linux daemon to use multiple keyboards as multiple joysticks in supported games using uinput

I created this program in order to play multiplayer local games using multiple keyboards separately
like clonehero.

# install

just run the install script and then restart your computer
`$ sudo ./Install.sh`

the install script will create a new systemd service. You can start it using:
`$ sudo systemctl start kbjoy`

or start it automatically on boot:
`$ sudo systemctl enable kbjoy`

# usage

before first time usage, make sure your user have the necessary rights:
`$ sudo adduser your_user_account kbjoy`

## when running

WARNING: when a keyboard is connected, it will act as a gamepad and can't
be used as a keyboard until you exit the **gamepad mode**

**You can force disconnect a keyboard to kbjoy using ctrl+alt+del**
it will destroy the virtual device and make the keyboard act as a keyboard again

When connected, the keyboard has two different modes:
1. gamepad mode: all the input event are send to the virtual gamepad, the keyboard is disabled
2. keyboard mode: the gamepad exist but is in idle. the keyboard act normally

you can switch between the two mode using SUPER+ESCAPE

## example

If you want to give a try to the app, you can run the example script: config/keyboards.sh
who will automatically run all the command.

Just be sure to replace "/home/blade/Documents/kbjoy/configs/clonehero.map" with the path of your config mapping file
(or by nothing to use default settings)

## run command

you can send command to the daemon using kbjoy-client program
(it will send your command to the unix domain socket of the daemon)
the syntax is:
`$ kbjoy-client your_command arg1 [arg2...]`

the available commands are:
1. ping [no arg] : test if the daemon is running
2. exit [no arg] : exit the daemon properly
3. add [int_gamepad_id] : create the virtual device with the specified id
4. start [int_gamepad_id] [keyboard path] [optional: config path] : assign a keyboard to a previously created virtual gamepad
5. trigger [KEY_ID] : report the path of the keyboard when a specific key is pressed

## get output of a command

the programm kbjoy-connect (with no arg) is used to monitor et read the output of
all the previous command (except for ping)

example output:

```
sent: connect
receive 9 accept:
connection ID: 3
About to extract fd
Extracted fd 4
pid: 3469 
testlock
file locked
timeout set 
skipped: list-start
skipped: /dev/input/event0
skipped: list-end
skipped: begin:
skipped: begin:
skipped: begin:
timeout removed 
recv: list-start
recv: /dev/input/event0
recv: list-end
```

the keyword "skipped" means the message was sent before kbjoy-connect was running

# config file
you can edit the mapping of your virtual gamepad using a mapping file.
You can see examples in config/

(I may add informations to this section later)
