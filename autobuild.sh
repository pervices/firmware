#!/bin/sh

function print_help() {
    echo ""
    echo "Compiles server for device with provided configuration options."
    echo "Usage:"
    echo "  ./autobuild.sh []"
    echo ""
    echo "Required:"
    echo "  -p, --product       The product to compile the server for (TATE_NRNT|LILY|VAUNT)"
    echo "  -v, --hw-revision   The hardware revision to compile for (RTM5, RTM10, etc...)"
    echo "  -r, --rx-channels   The number of RX channels on the device"
    echo "  -t, --tx-channels   The number of TX channels on the device"
    echo "  -s, --max-rate      The max sample rate of the device (3000|1000|500|162.5)"
    echo ""
    echo "Optional:"
    echo "  --rx_40ghz_fe       Indicates the device tx board has been replaced by a 40GHz RX frontend"
    echo "  --use_3g_as_1g      Use 1GSPS sample rate with 3GSPS backplane"
    echo "  --user_lo       "
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
                echo "  Make sure you provide a valid product after the product flag."
                exit 1
            fi
            PRODUCT="${2^^}"
            shift
            shift
            ;;
        -v|--hw-revision)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -v|--hw-revision flag."
                echo "  Make sure you provide a hardware revision after the hardware revision flag."
            fi
            HW_REV="${2^^}"
            shift
            shift
            ;;
        -r|--rx-channels)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -r|--rx-channels flag."
                echo "  Make sure you provide a value after the rx-channels flag."
            fi
            NUM_RX="R$2"
            shift
            shift
            ;;
        -t|--tx-channels)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -t|--tx-channels flag."
                echo "  Make sure you provide a value after the rx-channels flag."
            fi
            NUM_TX="T$2"
            shift
            shift
            ;;
        -s|--max-rate)
            if [ -z $2 ]; then
                echo "[ERROR] Could not find value for -s|--max-rate flag."
                echo "  Make sure you provide a value after the max rate flag."
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

VALID_PRODUCTS=('TATE_NRNT' 'LILY' 'VAUNT')

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


