#!/bin/sh

# For 4R4T use TATE_4R4T
# For 8R use TATE_8R

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
