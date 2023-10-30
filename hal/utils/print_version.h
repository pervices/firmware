#pragma once

#include "../drivers/mmap/mmap.h"

#ifdef VAUNT
    #include "variant_config/vaunt_special_config.h"
#elif defined(TATE_NRNT)
    #include "variant_config/tate_special_config.h"
    #include "../channels.h"
#endif


void print_version() {
    printf("Branch: %s\n", VERSIONGITBRANCH);
    printf("Revision: %s\n", VERSIONGITREVISION);
    #ifdef RTM3
        printf("RTM: 3\n");
    #elif RTM4
        printf("RTM: 4\n");
    #elif RTM5
        printf("RTM: 5\n");
    #elif RTM6
        printf("RTM: 6\n");
    #elif RTM7
        printf("RTM: 7\n");
    #elif RTM8
        printf("RTM: 8\n");
    #elif RTM9
        printf("RTM: 9\n");
    #elif RTM10
        printf("RTM: 10\n");
    #else
        #error "This file must be compiled with a valid hardware revision (RTM3, RTM4, RTM5, RTM6, RTM7, RTM8, RTM9, RTM10)"
    #endif
    printf("Date: %s UTC\n", VERSIONDATE);
    #if defined(TATE_NRNT)
        printf("Product: TATE_NRNT\n");
        printf("Rx: %i\n", NUM_RX_CHANNELS);
        printf("Tx: %i\n", NUM_TX_CHANNELS);
        printf("Rate: %i\n", MAX_SAMPLE_RATE);
    #elif defined(VAUNT)
        printf("Product: VAUNT\n");
    #else
        #error "This file must be compiled with a valid PRODUCT (VAUNT | TATE_NRNT). Confirm spelling and spaces."
    #endif

    uint32_t ver39_32, ver31_0;
    read_hps_reg("sys3", &ver39_32);
    read_hps_reg("sys4", &ver31_0);
    printf("FPGA: %01x%08x\n", (ver39_32 & 0xf), (ver31_0 & 0xffffffff));
    #ifdef TATE_NRNT
    uint32_t verjesd;
    read_hps_reg("res_ro11", &verjesd);
    printf("JESD: %02x\n", (verjesd & 0xff0000) >> 16);
    #endif
    #if USE_3G_AS_1G
    printf("USE_3G_AS_1G\n");
    #endif
    #if RX_40GHZ_FE
    printf("RX_40GHZ_FE\n");
    #endif
}
