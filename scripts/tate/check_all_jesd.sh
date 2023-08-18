#!/bin/bash

if [ "$#" -ne 2 ]
then
    echo "When using this script, pass the number of rx and tx channels"
    echo "Example: $0 9 7 for a device with 9 rx and 7 tx"
    exit
fi

echo "good indicates that the jesd link is up for that channel"
echo "bad indicates that the jesd link is down for that channel"
echo "0 indicates that the server is unresponsive or the board is off"

n=0
while [ $n -lt $1 ]
do

    let int_ch=$n+97
    ch=$(echo -ne \\x$(printf %02x $int_ch))
    echo 0 > "/var/cyan/state/rx/$ch/jesd/status"
    sleep 0.5
    result=$(cat "/var/cyan/state/rx/$ch/jesd/status")
    echo "rx $ch: $result"
    let "n=$n+1"
done

n=0
while [ $n -lt $2 ]
do

    let int_ch=$n+97
    ch=$(echo -ne \\x$(printf %02x $int_ch))
    echo 0 > "/var/cyan/state/tx/$ch/jesd/status"
    sleep 0.1
    result=$(cat "/var/cyan/state/tx/$ch/jesd/status")
    echo "tx $ch: $result"
    let "n=$n+1"
done


