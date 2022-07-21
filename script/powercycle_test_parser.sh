#!/bin/bash

function print_help() {
    echo "Parses data from generate_inter_powercycle_log.sh:"
    echo " Short Option | Long Option         | Description"
    echo " -----------------------------------------------------------------------------------------------"
    echo "     -h       | --help              | Print this help message and exit"
    echo "     -d       | --directory         | Folder containing the results"
    echo "     -a       | --all               | Show verbose results"
    echo "     -j       | --jesd              | Parse JESD results"
    echo "     -s       | --sfp               | Parse SFP results"
    echo "     -r       | --rx_channels       | Number of rx channels"
    echo "     -t       | --tx_channels       | Number of tx channels"
    echo "If only specifying address, then only a read will occur. Writes require both address and value."
    exit
}

show_verbose=false
show_jesd=false
show_sfp=false
result_directory="results"
num_rx=0
num_tx=0

n=1
while [ $n -le $# ]
do
    if [ "${!n}" = "-h" ] || [ "${!n}" = "--help" ]
    then
        print_help
        exit
    fi
    let tmp=$n+1
    if [ "${!n}" = "-d" ] || [ "${!n}" = "--directory" ]
    then
        result_directory=${!tmp}
    fi
    if [ "${!n}" = "-a" ] || [ "${!n}" = "--all" ]
    then
        show_verbose=true
    fi
    if [ "${!n}" = "-j" ] || [ "${!n}" = "--jesd" ]
    then
        show_jesd=true
    fi
    if [ "${!n}" = "-s" ] || [ "${!n}" = "--sfp" ]
    then
        show_sfp=true
    fi
    if [ "${!n}" = "-r" ] || [ "${!n}" = "--rx_channels" ]
    then
        let num_rx=${!tmp}
    fi
    if [ "${!n}" = "-t" ] || [ "${!n}" = "--tx_channels" ]
    then
        let num_tx=${!tmp}
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

    let expected_rx_good=3*$num_rx
    let expected_tx_good=3*$num_tx

    # Number of times all jesd came up every server reboot
    perfect_rx_jesd=0
    perfect_tx_jesd=0
    # Number of times some jesd came up after some sever reboot
    partial_rx_jesd=0
    partial_tx_jesd=0
    # Number of power cycles where jesd never came up
    failed_rx_jesd=0
    failed_tx_jesd=0
    # Number of tests with the wrong number of rx and tx tests, usually and idicator that the test was stopped partway through
    invalid_cycle=0
    for result in $result_directory/*;
    do
        num_rx_jesd_good=$(grep -A 3 "Rx board jesd status" $result | grep -c 'good')
        num_tx_jesd_good=$(grep -A 3 "Tx board jesd status" $result | grep -c 'good')
        num_rx_bad=$(grep -A 3 "Rx board jesd status" $result | grep -c 'bad')
        num_tx_bad=$(grep -A 3 "Tx board jesd status" $result | grep -c 'bad')
        num_rx_no_reply=$(grep -A 3 "Rx board jesd status" $result | grep -c '1')
        num_tx_no_reply=$(grep -A 3 "Tx board jesd status" $result | grep -c '1')

        let rx_jesd_checks=$num_rx_jesd_good+$num_rx_bad+$num_rx_no_reply
        let tx_jesd_checks=$num_tx_jesd_good+$num_tx_bad+$num_tx_no_reply

        if [ $rx_jesd_checks -ne $expected_rx_good ] || [ $tx_jesd_checks -ne $expected_tx_good ]
        then
            let invalid_cycle=$invalid_cycle+1
            echo "$result: this power cycle had an incorect number of JESD checks. This is usually sign of corruption in the test itself, skipping result"
            continue
        fi

        if [ $num_rx_jesd_good -ne $expected_rx_good ] || [ $num_tx_jesd_good -ne $expected_tx_good ]
        then
            echo "Run: $result"
            echo "Number times rx JESD established: $num_rx_jesd_good"
            echo "Number times tx JESD established: $num_tx_jesd_good"
            echo "Number times rx JESD failed: $num_rx_bad"
            echo "Number times tx JESD failed: $num_tx_bad"
            echo "Number times rx JESD status failed to reply: $num_rx_no_reply"
            echo "Number times tx JESD status failed to reply: $num_tx_no_reply"
        fi

        if [ $num_rx_jesd_good == $expected_rx_good ]
        then
            let perfect_rx_jesd=$perfect_rx_jesd+1
        elif [ $num_rx_jesd_good == 0 ]
        then
            let failed_rx_jesd=$failed_rx_jesd+1
        else
            let partial_rx_jesd=$partial_rx_jesd+1
        fi
        if [ $num_tx_jesd_good == $expected_tx_good ]
        then
            let perfect_tx_jesd=$perfect_tx_jesd+1
        elif [ $num_tx_jesd_good == 0 ]
        then
            let failed_tx_jesd=$failed_tx_jesd+1
        else
            let partial_tx_jesd=$partial_tx_jesd+1
        fi

    done

    echo "Rx JESD fully succeeded on every sever restart on $perfect_rx_jesd power cycles"
    echo "Tx JESD fully succeeded on every sever restart on $perfect_tx_jesd power cycles"
    echo "Rx JESD succeeded sometimes channels on $partial_rx_jesd power cycles"
    echo "Tx JESD succeeded sometimes channels on $partial_tx_jesd power cycles"
    echo "Rx JESD never worked on $failed_rx_jesd power cycles"
    echo "Tx JESD never worked on $failed_tx_jesd power cycles"
    echo "There were $invalid_cycle tests with corrupted results"
fi

if $show_sfp
then
    echo "SFP status results. Each file is from one power cycle"
    if $show_verbose
    then
        grep -A 3 "sys18" $result_directory/*
    fi

    sfp_good=$(grep -A 3 "sys18" $result_directory/* | grep -c 0xf7001800)
    sfp_checks=$(grep -A 3 "sys18" ~/tmp/power_cycle_results/* | grep -c sys18)
    echo "The sfp port was good on $sfp_good server reboots out of $sfp_checks"

    # Number of times sfp ports came up after every server boot within a power cycle
    perfect_sfp=0

    # Number of times sfp ports came up after some server boots within a power cycle
    partial_sfp=0

    # Number of time the was never a time where all sfp ports came up within a power cycle
    failed_sfp=0

    # Number of tests with the wrong number of rx and tx tests, usually and idicator that the test was stopped partway through
    invalid_cycle=0

    for result in $result_directory/*;
    do
        sfp_good=$(grep -A 3 "sys18" $result* | grep -c 0xff)
        sfp_checks=$(grep -A 3 "sys18" $result* | grep -c 0x)

        invalid_cycle=0

        if [ $sfp_checks -ne 3 ]
        then
            let invalid_cycle=$invalid_cycle+1
            echo "$result: this power cycle had an incorect number of JESD checks. This is usually sign of corruption in the test itself, skipping result"
            continue
        fi

        if [ $sfp_good -ne $sfp_checks ]
        then
            echo "Run: $result"
            echo "The sfp ports were good $sfp_good out of $sfp_checks within this power cycle"
        fi

        if [ $sfp_good == $sfp_checks ]
        then
            let perfect_sfp=$perfect_sfp+1
        elif [ $sfp_good == 0 ]
        then
            let failed_sfp=$failed_sfp+1
        else
            let partial_sfp=$partial_sfp+1
        fi

    echo "The SFP always ports worked on $perfect_sfp, worked on some server boots on $partial_sfp, and never worked on $failed_sfp power cycles"

    done
fi
