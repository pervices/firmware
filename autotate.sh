#!/bin/sh
echo "The ./autotate.sh script is deprecated. Calling ./autobuild.sh instead..."

if [ $# -eq 0 ]; then
    echo "Version unspecified"
    echo "Example: ./autotate.sh TATE_NRNT RTM5 R4 T4 S1000 for TATE_4R4T"
    echo "Example: ./autotate.sh LILY RTM1 R4 T4 S500 for LILY"
    exit 1
fi

if [ $# -eq 1 ]; then
    echo "This mode is no longer supported, use NRNT mode"
    echo "Example: ./autotate TATE_NRNT RTM5 R4 T4 S1000 for TATE_4R4T"
        echo "Example: ./autotate.sh LILY RTM1 R4 T4 S500 for LILY"
    echo "special variants: ./autotate.sh TATE_NRNT RTM5 R4 T4 S1000 F1"
    exit 80
elif [ $# -eq 2 ]; then
    echo "This mode is no longer supported, use NRNT mode"
    echo "Example: ./autotate.sh TATE_NRNT RTM5 R4 T4 S1000 for TATE_4R4T"
    exit 80
elif [ $# -ge 5 ]; then
    if [ "$1" != "TATE_NRNT" -a "$1" != "LILY" ]; then
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

    OPTIONAL_FLAGS=
    if [ $special_flags == "F1" ]; then
        OPTIONAL_FLAGS="--use_3g_as_1g"
    elif [ $special_flags == "F2" ]; then
        OPTIONAL_FLAGS="--user_lo"
    elif [ $special_flags == "F3" ]; then
        OPTIONAL_FLAGS="--use_3g_as_1g --user_lo"
    fi

    ./autobuild.sh -p $1 -v $2 -r ${3#R} -t ${4#T} -s ${5#S} ${OPTIONAL_FLAGS}

fi
