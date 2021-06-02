#!/bin/sh

#A script that runs the commands for building the server for crimsom with autogen clean added at the begining
sh autogen.sh clean

sed -i 's/DTATE/DVAUNT/g' configure.ac #Optional, clean git repo defaults to Vaunt.

sh autogen.sh
CXX="/opt/gcc-arm32/bin/arm-unknown-linux-gnueabihf-g++" \
CC="/opt/gcc-arm32/bin/arm-unknown-linux-gnueabihf-gcc" \
CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall -march=armv7-a -mtune=cortex-a9 -mfpu=neon" \
./configure --prefix=/usr --host=arm-unknown-linux-gnueabihf
make
