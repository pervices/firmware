These scripts and services write the Linux system time to the FPGA. This service is meant to be disabled by default. This service should only be enabled by the user when they are using NTPD or a similar service to ensure that the Linux time is correct. The locations of the files when installed on a crimson unit are listed below

FILE                                LOCATION
crimson-fpga-time-sync.sh           /usr/bin
crimson-fpga-time-sync.service      /lib/systemd/system
crimson-fpga-time-sync.timer        /lib/systemd/system
