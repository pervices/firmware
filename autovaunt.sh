#!/bin/sh

echo "The ./autovaunt.sh script is deprecated. Calling ./autobuild.sh instead..."

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

if [ $special_flags == "F1" ]; then
    OPTIONAL_FLAGS="--rx_40ghz_fe"
fi

./autobuild.sh -p VAUNT -v $1 -r 4 -t 4 -s NA ${OPTIONAL_FLAGS}
