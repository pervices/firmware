#!/bin/sh

./autogen.sh clean
./autogen.sh

./configure                                             \
        --prefix=/usr                                       \
        --host=x86_64                                       \
        CC="arm-unknown-linux-gnueabihf-gcc"                \
        CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall  \
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon     \
            -Werror -lm" \
        CPPFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall\
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon"     \
        CXX="arm-unknown-linux-gnueabihf-g++"               \
        CXXFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall\
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon"     \
        PRODUCT=VAUNT

make -j9
