#!/bin/bash
clear
#max =0;
#maxperc =0;
while true; do
	buff_lvl=$(mem rr res_ro4);
	buff_lvl=$(($buff_lvl));
	high=2*32767;
	
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
	echo -ne "Buff Lvl= $buff_lvl \tMax= $max \tFilled= $buff_percent% \tMax= $maxperc%    \t //${v// /#} ${vv// / } // \r" 

done
