#!/bin/bash

function print_help() {
    echo "Parses data from generate_inter_powercycle_log.sh:"
    echo " Short Option | Long Option         | Description"
    echo " -----------------------------------------------------------------------------------------------"
    echo "     -h       | --help              | Print this help message and exit"
    echo "     -a       | --all               | Show verbose results"
    echo "     -j       | --jesd              | Parse JESD results"
    echo "     -f       | --fpga              | Parse JESD results"
    echo "If only specifying address, then only a read will occur. Writes require both address and value."
    exit
}

show_verbose=false
show_jesd=false
show_fpga=false

n=1
while [ $n -le $# ]
do
    if [ "${!n}" = "-h" ] || [ "${!n}" = "--help" ]
    then
        print_help
        exit
    fi
    if [ "${!n}" = "-a" ] || [ "${!n}" = "--all" ]
    then
        show_verbose=true
    fi
    if [ "${!n}" = "-j" ] || [ "${!n}" = "--jesd" ]
    then
        show_jesd=true
    fi
    if [ "${!n}" = "-f" ] || [ "${!n}" = "--fpga" ]
    then
        show_fpga=true
    fi
    let n=$n+1
done

if $show_jesd
then
    echo "JESD status results. Each file is from one power cycle"
    if $show_verbose
    then
        grep 'good\|bad\|System reboot\|Server restart\|Rx board dump ch\|Tx board dump ch' results/*
    fi
    for result in results/*;
    do
        echo "Run: $result"
        echo "Number times rx JESD established: $(grep -A 3 "Rx board jesd status" $result | grep -c 'good')"
        echo "Number times tx JESD established: $(grep -A 3 "Tx board jesd status" $result | grep -c 'good')"
        echo "Number times rx JESD failed: $(grep -A 3 "Rx board jesd status" $result | grep -c 'bad')"
        echo "Number times tx JESD failed: $(grep -A 3 "Tx board jesd status" $result | grep -c 'bad')"
    done
fi

if $show_fpga
then
    echo "JESD status results. Each file is from one power cycle"
    if $show_verbose
    then
        grep -A 3 "sys18" results/*
    fi
    #for result in results/*;
    #do
#
    #done
fi
