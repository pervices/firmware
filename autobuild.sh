#!/bin/sh

# Used for validating values from user
VALID_PRODUCTS=('TATE_NRNT' 'LILY' 'VAUNT')
VAUNT_RTMS=('RTM6' 'RTM7' 'RTM8' 'RTM9' 'RTM10' 'RTM11' 'RTM12')
VAUNT_RATES=('162M5')
TATE_RTMS=('RTM3' 'RTM4' 'RTM5' 'RTM6' 'RTM7')
TATE_RATES=('1000' '3000')
LILY_RTMS=('RTM0' 'RTM1')
LILY_RATES=('500')


function print_help() {
    echo "Compiles server for device with provided configuration options."
    echo "Usage:"
    echo "  ./autobuild.sh -p <TATE_NRNT|LILY|VAUNT> -v <RTMx> -r <num_rx> -t <num_tx> -s <max_sample_rate> [optional flags]..."
    echo "  ./autobuild.sh [-h|--help]  # Prints this help menu"
    echo ""
    echo "Required:"
    echo "  -p, --product       The product to compile the server for (TATE_NRNT|LILY|VAUNT)"
    echo "  -v, --hw-revision   The hardware revision to compile for (RTM5, RTM10, etc...)"
    echo "  -r, --rx-channels   The number of Rx channels on the device"
    echo "  -t, --tx-channels   The number of Tx channels on the device"
    echo "  -s, --max-rate      The max sample rate of the device (3000|1000|500|162M5)"
    echo ""
    echo "Optional:"
    echo "  --rx_40ghz_fe       Indicates the device Tx board has been replaced by a 40GHz Rx frontend"
    echo "  --use_3g_as_1g      Use 1GSPS sample rate with 3GSPS backplane"
    echo "  --user_lo           Enable user LO mode"
    echo ""
    echo "Examples:"
    echo "  ./autobuild.sh -p VAUNT -v RTM10 -r 4 -t 4 -s 162M5                     # Vaunt RTM10"
    echo "  ./autobuild.sh -p TATE_NRNT -v RTM5 -r 4 -t 4 -s 1000 --use_3g_as_1g    # Tate RTM5 using 3G backplane as 1G"
    echo "  ./autobuild.sh -p LILY -v RTM1 -r 4 -t 4 -s 500                         # Lily RTM1"
}

# Validate the value provided for an argument
function validate_value() {

}

# Required arguments
PRODUCT=
HW_REV=
NUM_RX=
NUM_TX=
MAX_RATE=
# Optional arguments
RX_40GHZ_FE=0
USE_3G_AS_1G=0
USER_LO=0

while [ $# -gt 0 ]; do
    key="$1"
    case $key in
        -h|--help)
            print_help
            exit 0
            ;;
        -p|--product)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -p|--product flag."
                echo "        Make sure you provide a valid product after the product flag."
                exit 1
            fi
            PRODUCT="${2^^}"
            shift
            shift
            ;;
        -v|--hw-revision)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -v|--hw-revision flag."
                echo "        Make sure you provide a valid RTM number after the hardware revision flag."
                exit 1
            fi
            HW_REV="${2^^}"
            shift
            shift
            ;;
        -r|--rx-channels)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -r|--rx-channels flag."
                echo "        Make sure you provide a value after the rx-channels flag."
                exit 1
            fi
            NUM_RX="R$2"
            shift
            shift
            ;;
        -t|--tx-channels)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -t|--tx-channels flag."
                echo "        Make sure you provide a value after the rx-channels flag."
                exit 1
            fi
            NUM_TX="T$2"
            shift
            shift
            ;;
        -s|--max-rate)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -s|--max-rate flag."
                echo "        Make sure you provide a value after the max rate flag."
                exit 1
            fi
            MAX_RATE=$2
            shift
            shift
            ;;
        --rx_40ghz_fe)
            RX_40GHZ_FE=1
            shift
            ;;
        --use_3g_as_1g)
            USE_3G_AS_1G=1
            shift
            ;;
        --user_lo)
            USER_LO=1
            shift
            ;;
        *)
            echo Unrecognized option: $key
            echo
            print_help
            exit 1
            ;;
    esac
    let n=$n+1
done

# Check for all required arguments
if [ -z $PRODUCT ]; then
    echo "[ERROR] Product was not specified but is required for compilation."
    exit 1
fi

is_valid=0
for product_cmp in ${VALID_PRODUCTS[@]}; do
    if [ "${PRODUCT}" == "$product_cmp" ]; then
        is_valid=1
        break
    fi
done

if [ ${!is_valid} ]; then
    echo "[ERROR] Invalid product: $PRODUCT"
    echo "  Valid products are: ${VALID_PRODUCTS[@]}"
    exit 1
fi

if [ -z $HW_REV ]; then
    echo "[ERROR] Hardware revision was not specified but is required for compilation."
    exit 1
fi

if [ $PRODUCT == "VAUNT" ]; then
    VALID_RTMS=${VAUNT_RTMS[@]}
    VALID_RATES=${VAUNT_RATES[@]}
elif [ $PRODUCT == "TATE_NRNT" ]; then
    VALID_RTMS=${TATE_RTMS[@]}
    VALID_RATES=${TATE_RATES[@]}
elif [ $PRODUCT == "LILY" ]; then
    VALID_RTMS=${LILY_RTMS[@]}
    VALID_RATES=${LILY_RATES[@]}
fi

is_valid=0
for rtm in ${VALID_RTMS[@]}; do
    if [ $HW_REV == $rtm ]; then
        is_valid=1
        break
    fi
done

if [ ${!is_valid} ]; then
    echo "[ERROR] Invalid RTM: $HW_REV"
    echo "  Valid RTMS for specified product are: ${VALID_RTMS[@]}"
    exit 1
fi

if [ -z $NUM_RX ]; then
    echo "[ERROR] Number of RX channels was not specified but is required for compilation."
    exit 1
fi

if [ -z $NUM_TX ]; then
    echo "[ERROR] Number of TX channels was not specified but is required for compilation."
    exit 1
fi

if [ -z $MAX_RATE ]; then
    echo "[ERROR] Max sample rate was not specified but is required for compilation."
    exit 1
fi

# TODO: Move validation to function
is_valid=0
for rate in ${VALID_RATES[@]}; do
    if [ $MAX_RATE == $rate ]; then
        is_valid=1
        break
    fi
done

if [ ${!is_valid} ]; then
    echo "[ERROR] Invalid max sample rate: $MAX_RATE"
    echo "  Valid rates for specified product are: ${VALID_RATES[@]}"
    exit 1
fi

if [ "${PRODUCT}" == "VAUNT" ]; then
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

    PRODUCT_CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wfatal-errors  \
                    -march=armv7-a -mtune=cortex-a9 -mfpu=neon"
elif [ "${PRODUCT}" == "TATE_NRNT" ] || [ "${PRODUCT}" == "LILY" ]; then
    SERVER_CC="aarch64-linux-gnu-gcc"
    SERVER_CXX="aarch64-linux-gnu-g++"
    PRODUCT_CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wfatal-errors \
                    -march=armv8-a -mtune=cortex-a53"
fi

./autogen.sh clean
./autogen.sh

./configure                         \
    --prefix=/usr                   \
    --host=x86_64                   \
    CC=$SERVER_CC                   \
    CFLAGS="${PRODUCT_CFLAGS}       \
            -Werror -lm -pthread"   \
    CPPFLAGS="$PRODUCT_CFLAGS"      \
    CXX=$SERVER_CXX                 \
    CXXFLAGS="$PRODUCT_CFLAGS"      \
    PRODUCT=$PRODUCT                \
    HW_REV=$HW_REV                  \
    NRX=$NUM_RX                     \
    NTX=$NUM_TX                     \
    MAX_RATE="S${MAX_RATE}"         \
    RX_40GHZ_FE=$RX_40GHZ_FE        \
    USE_3G_AS_1G=$USE_3G_AS_1G      \
    USER_LO=$USER_LO                \

make -j$(nproc)

