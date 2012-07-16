#!/bin/bash

pkill vlc

make test | tee makelog.tmp

res=$(grep tests makelog.tmp | awk -F ' ' '{print $5}')

if [[ "$res" == "0" ]]; then
	cvlc celebrate.mp3 &>/dev/null &
else
	cvlc fail.mp3 &>/dev/null &
fi
