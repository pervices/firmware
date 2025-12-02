#!/bin/bash

if test -f "/var/volatile/crimson/state/time/clk/set_time"; then
    date +%s.%N > /var/volatile/crimson/state/time/clk/set_time
elif test -f "/var/volatile/calamine/state/time/clk/set_time"; then
    date +%s.%N > /var/volatile/calamine/state/time/clk/set_time
else
    echo "ERROR: unable to locate state/time/clk/set_time of Crimson or Calamine"
    exit 1
fi
hwclock -w
