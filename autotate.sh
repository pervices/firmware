#!/bin/sh

# For 4R4T 1G use TATE_4R4T
# For 9R7T 1G use TATE_9R7T
# For 8R 1G use TATE_8R
# For 4R4T 3G use TATE_4R4T_3G
# to check if all of them compile use ALL
# Use the hardware version number as the second argument (currently only required for rtm3)

if [ $# -eq 0 ]; then
    echo "Version unspecified"
    exit 1
fi

if [ "$1" == "ALL" ]; then
    ./$0 TATE_4R4T RTM3
    if [ ! -f "server" ]; then
        echo "TATE_4R4T RTM3 failed"
        exit 80
    fi
    ./$0 TATE_9R7T RTM3
    if [ ! -f "server" ]; then
        echo "TATE_9R7T RTM3 failed"
        exit 80
    fi
    ./$0 TATE_8R RTM3
    if [ ! -f "server" ]; then
        echo "TATE_8R RTM3 failed"
        exit 80
    fi
    ./$0 TATE_4R4T_3G RTM3
    if [ ! -f "server" ]; then
        echo "TATE_4R4T_3G RTM3 failed"
        exit 80
    fi
    ./$0 TATE_4R4T RTM4
    if [ ! -f "server" ]; then
        echo "TATE_4R4T RTM4 failed"
        exit 80
    fi
    ./$0 TATE_9R7T RTM4
    if [ ! -f "server" ]; then
        echo "TATE_9R7T RTM4 failed"
        exit 80
    fi
    ./$0 TATE_8R RTM4
    if [ ! -f "server" ]; then
        echo "TATE_8R RTM4 failed"
        exit 80
    fi
    ./$0 TATE_4R4T_3G RTM4
    if [ ! -f "server" ]; then
        echo "TATE_4R4T_3G RTM4 failed"
        exit 80
    fi
    exit 0
fi

./autogen.sh clean
./autogen.sh

if [ $# -eq 1 ]; then
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
            PRODUCT=$1 \
            HW_REV="UNSPECIFIED" \
            NRX="UNSPECIFIED" \
            NTX="UNSPECIFIED" \
            MAX_RATE="UNSPECIFIED"
elif [ $# -eq 2 ]; then
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
            PRODUCT=$1 \
            HW_REV=$2 \
            NRX="UNSPECIFIED" \
            NTX="UNSPECIFIED" \
            MAX_RATE="UNSPECIFIED"
elif [ $# -eq 5 ]; then
    if [ "$1" != "TATE_NRNT" ]; then
        echo "Parameters for NRNT specified but product is not NRNT"
        exit 80
    fi

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
            PRODUCT=$1 \
            HW_REV=$2 \
            NRX=$3 \
            NTX=$4 \
            MAX_RATE=$5
else
    echo "No valid configuration specified"
    exit 80
fi

make -j9
