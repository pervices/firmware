#Requires setting up passwordless login over ssh to work

#use ssh-keygen then ssh-copy-id -i <path specified during ssh-keygen> dev0@192.168.10.2

#set -e

#Get number of rx and tx channels from user, and how many times to run the cycle
if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
    echo "This program records the state of various parts of the machine after power cycles and server restarts to determine reliability"
    echo "Expected arguments: directory to store results, number of rx channels, number of tx channels, number of times to run the cycle <optional> device (Supported values: cyan, chestnut)"
    exit
fi

results_dir=$1
num_rx_channels=$2
num_tx_channels=$3
num_power_cycles=$4
if [ "$#" -gt 4 ]; then
    device=$5
else
    device="cyan"
fi

mkdir -p "$1"

uhd_usrp_info -v > "$1/config.txt"

num_system_restarts=0

# Sets server to turn on on boot
echo dev0 | ssh -tt dev0@192.168.10.2 "sudo sh -c \"systemctl enable $device-server\""

while [[ $num_system_restarts -lt $num_power_cycles ]]
do
    echo "Issuing system reboot $num_system_restarts" | tee -a $result_path
    echo dev0 | ssh -o "ServerAliveInterval 2" -tt dev0@192.168.10.2 'sudo sh -c "systemctl reboot"'
    echo "Waiting for system reboot $num_system_restarts" | tee -a $result_path

    sleep 120
    #Format: year, month, day, hour (24 hour clock), minute, second. Every part takes 2 digits
    time=$(date +"%y%m%d%H%M%S")
    result_path=($results_dir/powercycle_log_run_${run_num}_time_${time}.txt)

    if test -f "$result_path"; then
        echo "A file with the current timestamp already exists, this should be impossible"
        echo "To avoid the risk of overwriting data the program will exit now"
        exit
    fi

    #Do however many times the user requests
    #Reboot unit

    #Wait for the unit to power on

    #Do 3 times

    num_server_restarts=0
    while [ $num_server_restarts -lt 3 ]
    do
        echo "Server run $num_server_restarts" | tee -a $result_path
        if [ $num_server_restarts -ne 0 ]
        then
            echo "Issuing server restart $num_server_restarts"
            echo dev0 | ssh -tt dev0@192.168.10.2 "sudo sh -c \"systemctl restart $device-server\""
            # Waits for server to come up
            sleep 120
            echo "Finished server restart $num_server_restarts"
        fi
        #sys18 is included since it contains useful info about the FPGA notably the sfp status
        echo "sys18" | tee -a $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'sudo sh -c "mem rr sys18"' >> $result_path
        echo "sfpa status" | tee -a $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x1; mem rw net22 0x303; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x1; mem rw net22 0x320; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x1; mem rw net22 0x321; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x1; mem rw net22 0x322; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x1; mem rw net22 0x508; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo "sfpb status" | tee -a $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x2; mem rw net22 0x303; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x2; mem rw net22 0x320; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x2; mem rw net22 0x321; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x2; mem rw net22 0x322; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x2; mem rw net22 0x508; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo "sfpc status" | tee -a $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x4; mem rw net22 0x303; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x4; mem rw net22 0x320; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x4; mem rw net22 0x321; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x4; mem rw net22 0x322; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x4; mem rw net22 0x508; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo "sfpd status" | tee -a $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x8; mem rw net22 0x303; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x8; mem rw net22 0x320; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x8; mem rw net22 0x321; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x8; mem rw net22 0x322; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 'mem rw net21 0x8; mem rw net22 0x508; mem rw net24 0x1; mem rw net24 0x0; mem rr res_ro27' >> $result_path
        echo "Time board dump" | tee -a $result_path
        echo dev0 | ssh -tt dev0@192.168.10.2 "sudo sh -c \"echo 1 > /var/volatile/$device/state/time/board/dump\"; sleep 1; sudo cat /var/volatile/$device/state/time/board/dump" >> $result_path
        n=0
        while [ $n -lt $num_rx_channels ]
        do
            let int_ch=$n+97
            ch=$(echo -ne \\x$(printf %02x $int_ch))
            dump_command=$(printf "sudo sh -c \"echo 1 > /var/volatile/$device/state/rx/%s/board/dump\"; sleep 1; cat /var/volatile/$device/state/rx/%s/board/dump" "$ch" "$ch")
            echo "Rx board dump ch: $ch" | tee -a $result_path
            echo dev0 | ssh -tt dev0@192.168.10.2 $dump_command >> $result_path
            jesd_command=$(printf "sudo sh -c \"echo 1 > /var/volatile/$device/state/rx/%s/jesd/status\"; sleep 1; cat /var/volatile/$device/state/rx/%s/jesd/status" "$ch" "$ch")
            #The reply from jesd_result for some reason has " good" or " bad" overwrite the first few letters of password in the reply. Its not  an issue but is unexpected behaviour that turn into a problem in the future
            echo "Rx board jesd status ch: $ch" | tee -a $result_path
            echo dev0 | ssh -tt dev0@192.168.10.2 $jesd_command >> $result_path
            echo "" >> $result_path
            let "n=$n+1"
        done

        n=0
        while [ $n -lt $num_tx_channels ]
        do
            let int_ch=$n+97
            ch=$(echo -ne \\x$(printf %02x $int_ch))
            dump_command=$(printf "sudo sh -c \"echo 1 > /var/volatile/$device/state/tx/%s/board/dump\"; sleep 1; cat /var/volatile/$device/state/tx/%s/board/dump" "$ch" "$ch")
            echo "Tx board dump ch: $ch" | tee -a $result_path
            echo dev0 | ssh -tt dev0@192.168.10.2 $dump_command >> $result_path
            jesd_command=$(printf "sudo sh -c \"echo 1 > /var/volatile/$device/state/tx/%s/jesd/status\"; sleep 1; cat /var/volatile/$device/state/tx/%s/jesd/status" "$ch" "$ch")
            echo "Tx board jesd status ch: $ch" | tee -a $result_path
            echo dev0 | ssh -tt dev0@192.168.10.2 $jesd_command >> $result_path
            echo "" >> $result_path
            let "n=$n+1"
        done

        let "num_server_restarts=$num_server_restarts+1"
    done
    let "num_system_restarts=$num_system_restarts+1"
done

echo "Done"
