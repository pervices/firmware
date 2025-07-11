#!/bin/sh
set -Eeuo pipefail

# DIAGNOSTICS
SCRIPT_USER=`whoami`
SCRIPT_HM=`hostname`
SCRIPT_DATE=`date +%Y%M%dT%H%M%S.%N`
SCRIPT_DIR=`pwd`
SCRIPT_PROG=$0
SCRIPT_ARGS=$@
SCRIPT_DIAGNOSTICS_SHORT="$SCRIPT_USER@$SCRIPT_HM:$SCRIPT_DIR/$SCRIPT_PROG $SCRIPT_ARGS"
SCRIPT_UNAME=`uname -n -s -r -m`

function print_diagnostics_short(){
    echo "INFO: `date +%Y%M%dT%H%M%S.%N`: $SCRIPT_DIAGNOSTICS_SHORT"
}

function print_diagnostics() {
    echo "----------------------------------------"
    echo $0 $@
    echo "----------------------------------------"
    echo "Running as: $SCRIPT_USER@$SCRIPT_HM    "
    echo "Script Directory: $SCRIPT_DIR"
    echo "Script Launch Date: $SCRIPT_DATE"
    echo "Uname: $SCRIPT_UNAME"
    echo "----------------------------------------"
    echo "Current Time: `date +%Y%M%dT%H%M%S.%N`  "
    echo "PWD: `pwd`                              "
    echo "----------------------------------------"
}



# Used for validating values from user
VALID_PRODUCTS=('TATE_NRNT' 'LILY' 'VAUNT')
VAUNT_RTMS=('RTM6' 'RTM7' 'RTM8' 'RTM9' 'RTM10' 'RTM11' 'RTM12')
VAUNT_RATES=('NA')  # Vaunt detects sample rate at runtime
TATE_RTMS=('RTM3' 'RTM4' 'RTM5' 'RTM6' 'RTM7')
TATE_RATES=('1000' '3000')
LILY_RTMS=('RTM0' 'RTM1')
LILY_RATES=('500')


function print_help() {
    echo "Compiles server for device with provided configuration options."
    echo "Usage:"
    echo "  ./autobuild.sh -p <PRODUCT> -v <RTM_VERSION> -r <NUM_RX> -t <NUM_TX> -s <MAX_RATE_MHZ> [optional flags]..."
    echo "  ./autobuild.sh [-h|--help]  # Prints this help menu"
    echo ""
    echo "Required:"
    echo "  -p, --product       The product to compile the server for (TATE_NRNT|LILY|VAUNT)"
    echo "  -v, --hw-revision   The hardware revision to compile for (RTM5, RTM6, etc...)"
    echo "  -r, --rx-channels   The number of Rx channels on the device"
    echo "  -t, --tx-channels   The number of Tx channels on the device"
    echo "  -s, --max-rate      The max sample rate of the device in MHz (3000|1000|500|NA)"
    echo ""
    echo "Optional:"
    echo "  --rx_40ghz_fe       Indicates the device Tx board has been replaced by a 40GHz Rx frontend"
    echo "  --use_3g_as_1g      Use 1GSPS sample rate with 3GSPS backplane"
    echo "  --user_lo           Enable user LO mode"
    echo ""
    echo "Examples:"
    echo "  ./autobuild.sh -p VAUNT -v RTM10 -r 4 -t 4 -s NA                        # Vaunt RTM10. Vaunt detects sample rate at runtime, so set to NA for compilation"
    echo "  ./autobuild.sh -p TATE_NRNT -v RTM5 -r 4 -t 4 -s 1000 --use_3g_as_1g    # Tate RTM5 using 3G backplane as 1G"
    echo "  ./autobuild.sh -p LILY -v RTM1 -r 4 -t 4 -s 500                         # Lily RTM1"
}

# Compares a value to an array of valid values
function validate_value() {
    local val=$1
    shift
    local valid_values=("$@")

    local is_valid=0
    for v in ${valid_values[@]}; do
        if [ "${val}" == "$v" ]; then
            is_valid=1
            break
        fi
    done

    return $is_valid
}

function check_argument_exists() {
    local name=$1
    local arg=${2:-}

    if [ -z $arg ]; then
        print_diagnostics_short
        echo "[ERROR] $name was not specified but is required for compilation."
        exit 1
    fi
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

# Parse arguments/flags
while [ $# -gt 0 ]; do
    key="$1"
    case $key in
        -h|--help)
            print_help
            exit 0
            ;;
        -p|--product)
            if [ -z "${2:-}" ]; then
                print_diagnostics_short
                echo "[ERROR] Could not find value for -p|--product flag."
                echo "        Make sure you provide a valid product after the product flag."
                exit 1
            fi
            PRODUCT="${2^^}"
            shift
            shift
            ;;
        -v|--hw-revision)
            if [ -z "${2:-}" ]; then
                print_diagnostics_short
                echo "[ERROR] Could not find value for -v|--hw-revision flag."
                echo "        Make sure you provide a valid RTM number after the hardware revision flag."
                exit 1
            fi
            HW_REV="${2^^}"
            shift
            shift
            ;;
        -r|--rx-channels)
            if [ -z "${2:-}" ]; then
                print_diagnostics_short
                echo "[ERROR] Could not find value for -r|--rx-channels flag."
                echo "        Make sure you provide a value after the rx-channels flag."
                exit 1
            fi
            NUM_RX="R$2"
            shift
            shift
            ;;
        -t|--tx-channels)
            if [ -z "${2:-}" ]; then
                print_diagnostics_short
                echo "[ERROR] Could not find value for -t|--tx-channels flag."
                echo "        Make sure you provide a value after the rx-channels flag."
                exit 1
            fi
            NUM_TX="T$2"
            shift
            shift
            ;;
        -s|--max-rate)
            if [ -z "${2:-}" ]; then
                print_diagnostics_short
                echo "[ERROR] Could not find value for -s|--max-rate flag."
                echo "        Make sure you provide a value after the max rate flag."
                exit 1
            fi
            MAX_RATE=$2
            shift
            shift
            ;;
        --rx_40ghz_fe)
            # Indicates the unit is an avery rx where the tx board has been replaced by a 40GHz RX front end
            RX_40GHZ_FE=1
            shift
            ;;
        --use_3g_as_1g)
            # Uses 1GSPS sample rate with 3GSPS backplane, can use 3GSPS or 1GSPS RFEs.
            # The server will set 3GSPS RX boards programmed with Rx3 code to 1GSPS mode.
            # TX boards must be programmed with regular (1GSPS) Tx mcu.
            # Time board must be programmed with Time1on3 mcu if 3GSPS RX boards, or regular (1GSPS) time mcu if 1GSPS RX boards.
            USE_3G_AS_1G=1
            shift
            ;;
        --user_lo)
            USER_LO=1
            shift
            ;;
        *)
            print_diagnostics $@
            echo Unrecognized option: $key
            echo
            print_help
            exit 1
            ;;
    esac
done

# Validate values and check that required arguments were included
check_argument_exists "Product" $PRODUCT
if validate_value $PRODUCT ${VALID_PRODUCTS[@]}; then
    print_diagnostics_short
    echo "[ERROR] Invalid product: $PRODUCT"
    echo "  Valid products are: ${VALID_PRODUCTS[@]}"
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

check_argument_exists "Hardware revision" $HW_REV
if validate_value $HW_REV ${VALID_RTMS[@]}; then
    print_diagnostics_short
    echo "[ERROR] Invalid RTM: $HW_REV"
    echo "        Valid RTMS for specified product are: ${VALID_RTMS[@]}"
    exit 1
fi


check_argument_exists "Number of Rx channels" $NUM_RX

check_argument_exists "Number of Tx channels" $NUM_TX

check_argument_exists "Max sample rate" $MAX_RATE
if validate_value $MAX_RATE ${VALID_RATES[@]}; then
    print_diagnostics_short
    echo "[ERROR] Invalid max sample rate: $MAX_RATE"
    echo "        Valid rates for specified product are: ${VALID_RATES[@]}"
    exit 1
fi

# Determine compiler info
if [ "${PRODUCT}" == "VAUNT" ]; then
    if [ -z "${CC:-}" ]; then
        if command -v arm-linux-gnueabihf-gcc 2>&1 >/dev/null; then
            SERVER_CC="arm-linux-gnueabihf-gcc"
        elif command -v arm-unknown-linux-gnueabihf-gcc 2>&1 >/dev/null; then
            SERVER_CC="arm-unknown-linux-gnueabihf-gcc"
            echo "WARNING: using different compiler than CI system" # see pvpkg/firmware-scripts/server.sh
        else
            print_diagnostics_short
            echo "ERROR: GCC compiler for ARM not found"
            exit 1
        fi
    else
        SERVER_CC=$CC
    fi

    if [ -z "${CXX:-}" ]; then
        if command -v arm-linux-gnueabihf-g++ 2>&1 >/dev/null; then
            SERVER_CXX="arm-linux-gnueabihf-g++"
        elif command -v arm-unknown-linux-gnueabihf-g++ 2>&1 >/dev/null; then
            SERVER_CXX="arm-unknown-linux-gnueabihf-g++"
            echo "WARNING: using different compiler than CI system" # see pvpkg/firmware-scripts/server.sh
        else
            print_diagnostics_short
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

if ! ./autogen.sh clean; then
    echo "----------------------------------------" 
    echo "ERROR: ./autogen clean returned non-zero exit code, exiting..." 
    echo "----------------------------------------" 
    print_diagnostics_short
    exit $?
fi 

if ! ./autogen.sh; then
    echo "----------------------------------------" 
    echo "ERROR: Autogen returned non-zero exit code, exiting..." 
    echo "----------------------------------------" 
    print_diagnostics_short
    exit $?
fi 

if ! ./configure                        \
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
        USER_LO=$USER_LO; then
    echo "----------------------------------------" 
    echo "ERROR: Configure returned non-zero exit code, exiting..." 
    echo "----------------------------------------" 
    print_diagnostics_short
    exit $?
fi 


if ! make -j$(nproc); then
    echo "----------------------------------------" 
    echo "ERROR: Make returned non-zero exit code, exiting..." 
    echo "----------------------------------------" 
    print_diagnostics_short
    exit $?
fi

exit 0

