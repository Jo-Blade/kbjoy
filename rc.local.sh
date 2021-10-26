#!/bin/bash

# chgrp uinput /dev/uinput
# chmod 660 /dev/uinput

test=$(sudo cat /etc/rc.local | grep /dev/uinput)

if [[ $test = *[!\ ]* ]]; then
 	echo "/etc/rc.local already configured"
	exit 10
fi

sed '{$s/exit 0$/chgrp uinput \/dev\/uinput\nchmod 660 \/dev\/uinput\n\nexit 0/}' /etc/rc.local
