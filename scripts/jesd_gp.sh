#!/bin/bash

trap exit_function INT

function exit_function() {
    exit
}

function print_help() {
    echo "JESD register interaction script options:"
    echo " Short Option | Long Option         | Description"
    echo " -----------------------------------------------------------------------------------------------"
    echo "     -h       | --help              | Print this help message and exit"
    echo "     -j       | --jesd              | Which jesd to use (0-7 rx, 8-11 tx)"
    echo "     -a       | --address           | Address to read or write from"
    echo "     -v       | --value             | Value to write to address (not required if only reading)."
    echo "     -d       | --dump              | Dumps all registers"
    echo "If only specifying address, then only a read will occur. Writes require both address and value."
    exit
}

function write_reg() {
    jesd_idx=$1
    reg_full=$2
    value=$3

    let reg=reg_full/4
    let reg_check=reg*4

    #if [ $reg_check -lt $reg_full ];
    if [ $reg_check -lt $reg ];
    then
        echo "Ivalid register, must be a multiple of 4"
        exit
    fi

    jesd_core_onehot=$((1 << $jesd_idx))
    jesd_core_onehot_hex=$(echo "obase=16; $jesd_core_onehot" | bc)
    reg_hex=$(echo "obase=16; $reg" | bc)

    mem rw net6 $jesd_core_onehot_hex;  mem rw net7 $reg_hex; mem rw net8 $value; mem rw net9 0x2; mem rw net9 0x0;
}

function dump() {
    registers=( 0x0 0x04 0x8 0xc 0x10 0x14 0x18 0x1c 0x20 0x50 0x54 0x58 0x60 0x64 0x74 0x78 0x80 0x84 0x88 0x8c 0x94 0x98 0x9c 0xa0 0xa4 0xa8 0xac 0xb0 0xb4 0xc0 0xd0 0xd4 0xd8 0xdc 0xe0 0xf0 0xf4 0xf8 0xfc )

    jesd_idx=$1

    jesd_core_onehot=$((1 << $jesd_idx))
    jesd_core_onehot_hex=$(echo "obase=16; $jesd_core_onehot" | bc)

    echo "Dumping registers"

    for reg in "${registers[@]}"
    do
        let act_reg=reg/4
        act_reg_hex=$(echo "obase=16; $act_reg" | bc)
        mem rw net6 $jesd_core_onehot_hex;  mem rw net7 $act_reg_hex;  mem rw net9 0x1; mem rw net9 0x0;
        value=$(mem rr res_ro30)
        echo "$reg      $value"
    done
}

function read_reg() {
    jesd_idx=$1
    reg_full=$2

    let reg=reg_full/4
    let reg_check=reg*4

    #if [ $reg_check -lt $reg_full ];
    if [ $reg_check -lt $reg ];
    then
        echo "Ivalid register, must be a multiple of 4"
        exit
    fi

    jesd_core_onehot=$((1 << $jesd_idx))
    jesd_core_onehot_hex=$(echo "obase=16; $jesd_core_onehot" | bc)
    reg_hex=$(echo "obase=16; $reg" | bc)

    mem rw net6 $jesd_core_onehot_hex;  mem rw net7 $reg_hex;  mem rw net9 0x1; mem rw net9 0x0;
    value=$(mem rr res_ro30)
    echo $value
}

##############################################
# Parse the CmdLine arguments
##############################################
# saner programming env: these switches turn some bugs into errors
set -o pipefail -o noclobber -o nounset

dump=0
jesd=-1
value=""
address=""

n=1
while [ $n -le $# ]
do
    if [ "${!n}" = "-h" ] || [ "${!n}" = "--help" ]
    then
        print_help
        exit
    fi
    let tmp=$n+1
    if [ "${!n}" = "-j" ] || [ "${!n}" = "--jesd" ]
    then
        jesd=${!tmp}
    elif [ "${!n}" = "-v" ] || [ "${!n}" = "--value" ]
    then
        value=${!tmp}
    elif [ "${!n}" = "-a" ] || [ "${!n}" = "--address" ]
    then
        address=${!tmp}
    elif [ "${!n}" = "-d" ] || [ "${!n}" = "--dump" ]
    then
        dump=1
    fi
    let n=$n+1
done

if [ $jesd -eq -1 ];
then
    echo "What Jesd Channel ([0-7]=Rx, [8-11]=Tx)? "
    read jesd
fi

#if value is not -1 then the user request a write
if [ "$value" != "" ]
then
    write_reg $jesd $address $value
fi
if [ $dump -ne 0 ]
then
    dump $jesd
fi
if [ "$address" != "" ] && [ "$value" == "" ];
then
    read_reg $jesd $address
fi
