//
// Copyright 2014 - 2022 Per Vices Corporation
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

// Lily RTM1 is based off of Tate RTM6
// PRODUCT_RTM_VER refers to the RTM that applies to the complete products
// HARDWARE_RTM_VER refers to the RTM that applies to underlying hardware. For Tate it will match PRODUCT_RTM_VER, for Lily it will be PRODUCT_RTM_VER + 5
#if defined(TATE_NRNT)
    #if defined(RTM3)
        #define PRODUCT_RTM_VER 3
        #define HARDWARE_RTM_VER 3
    #elif defined(RTM4)
        #define PRODUCT_RTM_VER 4
        #define HARDWARE_RTM_VER 4
    #elif defined(RTM5)
        #define PRODUCT_RTM_VER 5
        #define HARDWARE_RTM_VER 5
    #elif defined(RTM6)
        #define PRODUCT_RTM_VER 6
        #define HARDWARE_RTM_VER 6
    #else
        #error "TATE_NRNT must be compiled with a valid hardware revision (RTM3, RTM4, RTM6)"
    #endif
#elif defined(LILY)
    // No actual Lily version exists based on RTM5 hardware, this option exists for testing using a converted RTM5 Tate
    #if defined(RTM0)
        #define PRODUCT_RTM_VER 0
        #define HARDWARE_RTM_VER 5
    #elif defined(RTM1)
        #define PRODUCT_RTM_VER 1
        #define HARDWARE_RTM_VER 6
    #else
        #error "LILY must be compiled with a valid hardware revision (RTM0, RTM1)"
    #endif
#else
    #error "This file must be compiled with a product revision (TATE_NRNT, LILY)"
#endif

#if HARDWARE_RTM_VER == 3 || HARDWARE_RTM_VER == 4
    // On 1Gsps sysref is activated on the falling edge of sysref, other stuff is active on the rising edge
    // To compensate for this some revisions swapped dev clk on hardware
    #define INVERT_DEV_CLOCK_HARDWARE_SWAP 1

    // Default analog delay
    // 0 = 0
    // Otherwise delay = n * 300ps + 300ps
    #define DEFAULT_ANALOG_SYSREF_DELAY 0

#elif HARDWARE_RTM_VER == 5 || HARDWARE_RTM_VER == 6
    // On 1Gsps sysref is activated on the falling edge of sysref, other stuff is active on the rising edge
    // To compensate for this some revisions swapped dev clk on hardware
    #define INVERT_DEV_CLOCK_HARDWARE_SWAP 0

    // Default analog delay
    // 0 = 0
    // Otherwise delay = n * 300ps + 300ps
    #define DEFAULT_ANALOG_SYSREF_DELAY 0

#else
    #error "Invalid HARDWARE_RTM_VER (derived from product and hardware revision). Valid HARDWARE_RTM_VER: 3, 4, 5, 6"
#endif

#if HARDWARE_RTM_VER == 3 || HARDWARE_RTM_VER == 4 || HARDWARE_RTM_VER == 5
    #define TX_LO_POWER 60
    #define RX_LO_POWER 45

#elif HARDWARE_RTM_VER == 6
    // TODO: optimize lo power for RTM6
    #define TX_LO_POWER 60
    #define RX_LO_POWER 45
#else
    #error "Invalid HARDWARE_RTM_VER (derived from product and hardware revision). Valid HARDWARE_RTM_VER: 3, 4, 5, 6"
#endif
