#!/bin/sh

if [ -z $CC ]; then
	SERVER_CC="arm-unknown-linux-gnueabihf-gcc"
fi

if [ -z $CXX ]; then
	SERVER_CXX="arm-unknown-linux-gnueabihf-g++"
fi

./autogen.sh clean
./autogen.sh


./configure                                             \
        --prefix=/usr                                       \
        --host=x86_64                                       \
	CC=$SERVER_CC \
        CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall  \
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon     \
            -Werror -lm" \
        CPPFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall\
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon"     \
	CXX=$SERVER_CXX \
        CXXFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall\
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon"     \
        PRODUCT=VAUNT\
        HW_REV=$1

make -j9
