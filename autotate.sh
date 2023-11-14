#!/bin/sh

if [ $# -eq 0 ]; then
    echo "Version unspecified"
    exit 1
fi

./autogen.sh clean
./autogen.sh

if [ $# -eq 1 ]; then
    echo "This mode is no longer supported, use NRNT mode"
    echo "Example: ./autotate TATE_NRNT RTM5 R4 T4 S1000 for TATE_4R4T"
    echo "special variants: ./autotate TATE_NRNT RTM5 R4 T4 S1000 F1"
    exit 80
elif [ $# -eq 2 ]; then
    echo "This mode is no longer supported, use NRNT mode"
    exit 80
elif [ $# -ge 5 ]; then
    if [ "$1" != "TATE_NRNT" ]; then
        echo "Parameters for NRNT specified but product is not NRNT"
        exit 80
    fi

    # SPECIAL_FLAGS is a hex bitmask with an F before it (optional)
    # see firmware/hal/variant_config/tate_special_config.h for details
    if [ $# -ge 6 ]; then
        special_flags=$6
    else
        special_flags="F0"
    fi

    ./configure                         \
            --prefix=/usr                   \
            --host=x86_64                   \
            CC=aarch64-linux-gnu-gcc        \
            CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall \
                    -march=armv8-a -mtune=cortex-a53 \
                    -Werror -lm" \
            CPPFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall \
                    -march=armv8-a -mtune=cortex-a53 " \
            CXX=aarch64-linux-gnu-g++       \
            CXXFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall \
                    -march=armv8-a -mtune=cortex-a53 " \
            PRODUCT=$1 \
            HW_REV=$2 \
            NRX=$3 \
            NTX=$4 \
            MAX_RATE=$5 \
            SPECIAL_FLAGS=$special_flags
else
    echo "No valid configuration specified"
    exit 80
fi

make -j$(nproc)
