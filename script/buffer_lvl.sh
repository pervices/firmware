#!/bin/bash
clear
#max =0;
#maxperc =0;
max=0;

channel=0;
mem_location="res_ro4"

if [ $# -eq 1 ]; then
    channel=$1
    mem_reg=$(((($channel/4)*8)+($channel%4)+4))
    mem_location="res_ro${mem_reg}"
fi

while true; do
        buff_lvl=$(mem rr $mem_location);
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