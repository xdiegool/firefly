#!/bin/bash
# TODO check valgrind too!
MUSIC_DIR="$HOME"/music
all_passed=0

pkill vlc
log_file="/tmp/firefly_make.log"
make test |& tee "$log_file"
if (grep "error:\|Segmentation fault" $log_file);  then
	all_passed=1
fi
#make test > "$log_file"

resul=$(grep tests "$log_file" | awk -F ' ' '{print $5}')
resul=($resul)

for res in "${resul[@]}"; do 
	#echo "$res"
	all_passed=$(echo "${all_passed}+${res}" | bc)
done

if [[ "$all_passed" == "0" ]]; then
	figlet -f /usr/share/figlet/slant.flf "Celebration!"
	cvlc "$MUSIC_DIR"/celebrate.mp3 &>/dev/null
else
	cowsay -f /usr/share/cowsay/daemon.cow "Fail!"
	cvlc "$MUSIC_DIR"/fail.mp3 &>/dev/null
fi
