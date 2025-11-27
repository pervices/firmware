#!/bin/bash

date +%s.%N > /var/volatile/crimson/state/time/clk/set_time
hwclock -w
