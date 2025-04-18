#!/bin/sh

if [ $# -eq 0 ]; then
    echo "Hardware revision required"
    echo "Example: ./autovaunt.sh RTM10"
    echo "special variants: ./autovaunt.sh RTM10 F1"
    exit 80
elif [ $# -eq 1 ]; then
    special_flags="F0"
else
    # SPECIAL_FLAGS is a hex bitmask with an F before it (optional)
    # see firmware/hal/variant_config/vaunt_special_config.h for details
    special_flags="${2^^}"
fi

if [ -z $CC ]; then
    if command -v arm-linux-gnueabihf-gcc 2>&1 >/dev/null; then
        SERVER_CC="arm-linux-gnueabihf-gcc"
    elif command -v arm-unknown-linux-gnueabihf-gcc 2>&1 >/dev/null; then
        SERVER_CC="arm-unknown-linux-gnueabihf-gcc"
        echo "WARNING: using different compiler than CI system" # see pvpkg/firmware-scripts/server.sh
    else
        echo "ERROR: GCC compiler for ARM not found"
        exit 1
    fi
else 
	SERVER_CC=$CC
fi

if [ -z $CXX ]; then
    if command -v arm-linux-gnueabihf-g++ 2>&1 >/dev/null; then
        SERVER_CXX="arm-linux-gnueabihf-g++"
    elif command -v arm-unknown-linux-gnueabihf-g++ 2>&1 >/dev/null; then
        SERVER_CXX="arm-unknown-linux-gnueabihf-g++"
        echo "WARNING: using different compiler than CI system" # see pvpkg/firmware-scripts/server.sh
    else
        echo "ERROR: G++ compiler for ARM not found"
        exit 1
    fi
else
	SERVER_CXX=$CXX
fi

./autogen.sh clean
./autogen.sh


./configure                                             \
        --prefix=/usr                                       \
        --host=x86_64                                       \
	CC=$SERVER_CC \
        CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wfatal-errors  \
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon     \
            -Werror -lm -pthread" \
        CPPFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wfatal-errors\
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon"     \
	CXX=$SERVER_CXX \
        CXXFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wfatal-errors\
            -march=armv7-a -mtune=cortex-a9 -mfpu=neon"     \
        PRODUCT="VAUNT" \
        HW_REV="${1^^}" \
        NRX="R4" \
        NTX="T4" \
        MAX_RATE="S162M5" \
        SPECIAL_FLAGS=$special_flags

make -j$(nproc)
