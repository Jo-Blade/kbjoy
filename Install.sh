#!/bin/bash

echo "script and program by Teo Pisenti"
echo "this script will install kbjoy and all needed components on your computer"

echo "compile binaries"

make -C kbjoy
make -C kbjoy clean

make -C kbjoy-client

echo "copy daemon binaries in /usr/sbin:/usr/lib/kbjoy"

sudo cp kbjoy/bin/kbjoy-daemon /usr/sbin/

sudo mkdir /usr/lib/kbjoy
sudo cp kbjoy/bin/kbjoy-remapper /usr/lib/kbjoy/
sudo cp kbjoy/bin/kbjoy-trigger /usr/lib/kbjoy/

echo "copy default mapping config in /etc/kbjoy-default.map"

sudo cp configs/example.map /etc/kbjoy-default.map

echo "copy clients binaries kbjoy-[client|connect] in /usr/bin"

sudo cp kbjoy-client/bin/connect /usr/bin/kbjoy-connect
sudo cp kbjoy-client/bin/client /usr/bin/kbjoy-client

echo "get a folder in /run (/lib/tmpfiles.d/kbjoy.conf)"

sudo cp kbjoy.conf /usr/lib/tmpfiles.d/

echo "create the new system user \"kbjoy\""

sudo useradd -r kbjoy

echo "create group \"uinput\""

sudo groupadd uinput

echo "add groups \"input\" and \"uinput\" to user \"kbjoy\""

sudo adduser kbjoy input
sudo adduser kbjoy uinput

echo "create the systemd service kbjoy"

sudo cp kbjoy.service /etc/systemd/system/

echo "configure udev to change group of /dev/uinput to \"uinput\""

sudo cp 99-input.rules /etc/udev/rules.d

echo "configure /etc/rc.local to change group of /dev/uinput to \"uinput\""

test=$(sudo cat /etc/rc.local | grep /dev/uinput)

if [[ $test = *[!\ ]* ]]; then
 	echo "/etc/rc.local already configured"
	exit 10
fi

sed -i '{$s/exit 0$/chgrp uinput \/dev\/uinput\nchmod 660 \/dev\/uinput\n\nexit 0/}' /etc/rc.local

echo "kbjoy was successfully installed on your computer!"
echo "you must restart to make all changes take effect"
