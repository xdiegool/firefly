#!/bin/bash
# TODO celebration should only occur if all test programs and suits in them 
								# passes.
pkill vlc
log_file="/tmp/firefly_make.log"
make test | tee "$log_file"

res=$(grep tests "$log_file" | awk -F ' ' '{print $5}')

if [[ "$res" == "0" ]]; then
	cvlc celebrate.mp3 &>/dev/null
else
	cvlc fail.mp3 &>/dev/null
fi
