#!/bin/bash
pkill vlc
log_file="/tmp/firefly_make.log"
make test | tee "$log_file"
#make test > "$log_file"

resul=$(grep tests "$log_file" | awk -F ' ' '{print $5}')
resul=($resul)

all_passed=0
for res in "${resul[@]}"; do 
	#echo "$res"
	all_passed=$(echo "${all_passed}+${res}" | bc)
done

if [[ "$all_passed" == "0" ]]; then
	figlet -f /usr/share/figlet/slant.flf "Celebration!"
	cvlc celebrate.mp3 &>/dev/null
else
	cowsay -f /usr/share/cowsay/daemon.cow "Fail!"
	cvlc fail.mp3 &>/dev/null
fi
