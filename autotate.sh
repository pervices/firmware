#!/bin/sh

# For 4R4T 1G use TATE_4R4T
# For 9R7T 1G use TATE_9R7T
# For 8R 1G use TATE_8R
# For 4R4T 3G use TATE_4R4T_3G
# to check if all of them compile use ALL

if [ "$1" == "ALL" ]; then
    ./$0 TATE_4R4T
    if [ ! -f "server" ]; then
        echo "TATE_4R4T failed"
        exit 80
    fi
    ./$0 TATE_9R7T
    if [ ! -f "server" ]; then
        echo "TATE_9R7T failed"
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
