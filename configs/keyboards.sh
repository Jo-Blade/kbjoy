#!/bin/bash

killall kbjoy-connect
kbjoy-client trigger stop
kbjoy-client del 1
kbjoy-client del 2
kbjoy-client del 3
kbjoy-client del 4
kbjoy-client del 5
kbjoy-client del 6
kbjoy-client del 7
kbjoy-client del 8
sleep 1

kbjoy-client trigger KEY_Q
stdbuf -oL kbjoy-connect > /tmp/kbjoy-tmp.txt &

echo "Tous les joueurs doivent maintenant appuyer sur
A dans l'ordre"

sleep 5
killall kbjoy-connect

i=1

grep ^recv:\ trigger:1 /tmp/kbjoy-tmp.txt | cut -d\; -f2- | tail -n4 |
	while read line
	do
		echo $line
		kbjoy-client add $i
		kbjoy-client start $i $line /home/blade/Documents/kbjoy/configs/clonehero.map
		i=$((i+1))
	done

kbjoy-client trigger stop
