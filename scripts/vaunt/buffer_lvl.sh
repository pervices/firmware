#!/bin/bash
clear
set  -euo pipefail

#echo Script Name: $0
#echo Pass Channel to monitor as a,b,c or d.
#echo This version can only monitor one channel at a time.

bufferlevel=0
channel=a

if [ $# -eq 1 ]
then
  channel=${1^^}
fi

#Crimson specific values
high=2*32767;
max=0;
maxperc=0;

readbuffer() {
	channel=$1
	if [[ "$channel" == "A" ]]
	then
		eval bufferlevel=$(mem rr res_ro4);
	elif [[ "$channel" == "B" ]]
	then
		eval bufferlevel=$(mem rr res_ro5)
	elif [[ "$channel" == "C" ]]
	then
		eval bufferlevel=$(mem rr res_ro6)
	elif [[ "$channel" == "D" ]]
	then
		eval bufferlevel=$(mem rr res_ro7)
	else 
		eval bufferlevel=$(mem rr res_ro4)
	fi
	
}

while true; do
	readbuffer $channel;
	buff_lvl=$bufferlevel;
	buff_lvl=$(($buff_lvl));
	
	if [[ "$buff_lvl" > "$max" ]]
       	then
		max=$buff_lvl;
	fi
	buff_percent=$((100*buff_lvl));
	buff_percent=$((buff_percent/high));

	if [[ "$buff_percent" > "$maxperc" ]]
	then
		maxperc=$buff_percent;
	fi

	buff_ap=$((100-buff_percent));
	v=$(printf "%-${buff_percent}s" "");
	vv=$(printf "%-${buff_ap}s" " ");
	echo -ne "\r\e[KBuff Lvl= $buff_lvl \tMax= $max \tFilled= $buff_percent% \tMax= $maxperc%    \t //${v// /#} ${vv// / } //"

done
