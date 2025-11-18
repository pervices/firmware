#!/bin/bash

date +%s.%N > /var/volatile/crimson/state/time/clk/set_time

date +%s.%N > /var/volatile/calamine/state/time/clk/set_time
hwclock -w
