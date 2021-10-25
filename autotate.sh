#!/bin/sh

# For 4R4T use TATE_4R4T
# For 8R use TATE_8R
# For 3G 4R4T use TATE_4R4T_3G
# to check if all of them compile use ALL

if [ "$1" == "ALL" ]; then
    echo "T1"
    ./$0 TATE_4R4T
    if [ ! -f "server" ]; then
        echo "TATE_4R4T failed"
        exit 80
    fi
    ./$0 TATE_8R
    if [ ! -f "server" ]; then
        echo "TATE_8R failed"
        exit 80
    fi
    ./$0 TATE_4R4T_3G
    if [ ! -f "server" ]; then
        echo "TATE_4R4T_3G failed"
        exit 80
    fi
    exit 0
fi

./autogen.sh clean
./autogen.sh

./configure                         \
        --prefix=/usr                   \
        --host=x86_64                   \
        CC=aarch64-linux-gnu-gcc        \
        CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall \
                -march=armv8-a -mtune=cortex-a53 " \
        CPPFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall \
                  -march=armv8-a -mtune=cortex-a53 " \
        CXX=aarch64-linux-gnu-g++       \
        CXXFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall \
                  -march=armv8-a -mtune=cortex-a53 " \
        PRODUCT=$1

make -j9
