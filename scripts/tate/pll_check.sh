#!/bin/bash

MAX_ATTEMPTS=3
FAIL=1

for CURR_ATTEMPT in $(seq 1 $MAX_ATTEMPTS); do
    output=$(echo board -e | mcu -f s 2>&1)
    echo "$output"

    uart_timeout=$(echo $output | grep recv_uart_timeout)
    if [[ ! -z $uart_timeout ]]; then
        echo "[PLL_CHECK][ERROR][ATTEMPT $CURR_ATTEMPT]: Unable to communicate with the time board."
    else
        unlocked=$(echo $output | grep Unlocked)
        if [[ ! -z $unlocked ]]; then
            echo "[PLL_CHECK][ERROR][ATTEMPT $CURR_ATTEMPT]: PLL(s) unlocked"
        else
            # 3 LMKs, each with 2 PLLs means we're expecting to see `Locked` exactly 6 times in the response. 
            # Anything else is unexpected and considered a failure.
            lock_count=$(echo $output | grep -o Locked | wc -l)
            if [[ $lock_count -eq 6 ]]; then
                echo "All PLLs locked." 
                FAIL=0
                break
            else
                echo "[PLL_CHECK][ERROR][ATTEMPT $CURR_ATTEMPT]: Unrecognized response from time board."
            fi
        fi
    fi

    sleep 0.1
done

exit $FAIL