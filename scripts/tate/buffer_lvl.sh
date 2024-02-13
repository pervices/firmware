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

buff_limit=$(mem rr res_ro14)
buff_limit=$(($buff_limit))
buff_multiple=$(mem rr res_ro15)
buff_multiple=$(($buff_multiple))

while true; do
        buff_lvl=$(mem rr $mem_location);
        buff_lvl=$(($buff_lvl))*$buff_multiple;
        high=$buff_limit;

        if [[ "$buff_lvl" -gt "$max" ]]
        then
                max=$buff_lvl;
        fi
        buff_percent=$((100*buff_lvl));
        buff_percent=$((buff_percent/high));

        if [[ "$buff_percent" -gt "$maxperc" ]]
        then
            maxperc=$buff_percent;
        fi

        if [[ "$buff_percent" -lt "$minperc" ]]
        then
            minperc=$buff_percent;
        fi

        buff_ap=$((100-buff_percent));
        v=$(printf "%-${buff_percent}s" "");
        vv=$(printf "%-${buff_ap}s" " ");
        echo -ne "\r\e[KBuff Lvl= $buff_lvl \tMax= $max \tFilled= $buff_percent% \tMin= $minperc% \tMax= $maxperc%    \t //${v// /#} ${vv// / } //"

done
