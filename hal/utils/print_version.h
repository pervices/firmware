#pragma once

#include "../drivers/mmap/mmap.h"

#ifdef VAUNT
    #include "variant_config/vaunt_special_config.h"
    #include "variant_config/vaunt_rtm_config.h"
#elif defined(TATE_NRNT)
    #include "variant_config/tate_special_config.h"
    #include "variant_config/tate_rtm_config.h"
    #include "../channels.h"
#endif


void print_version() {
    printf("Branch: %s\n", VERSIONGITBRANCH);
    printf("Revision: %s\n", VERSIONGITREVISION);
    printf("RTM: %i\n", RTM_VER);
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
