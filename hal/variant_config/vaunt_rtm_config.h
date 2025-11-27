//
// Copyright 2014 - 2023 Per Vices Corporation
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

// PRODUCT_RTM_VER refers to the RTM that applies to the complete products
// HARDWARE_RTM_VER refers to the RTM that applies to underlying hardware. For Vaunt it will match PRODUCT_RTM_VER, for Avery it will match the Vaunt version it is based on
#if defined(VAUNT)
    #if defined(RTM6)
        #define PRODUCT_RTM_VER 6
        #define HARDWARE_RTM_VER 6
    #elif defined(RTM7)
        #define PRODUCT_RTM_VER 7
        #define HARDWARE_RTM_VER 7
    #elif defined(RTM8)
        #define PRODUCT_RTM_VER 8
        #define HARDWARE_RTM_VER 8
    #elif defined(RTM9)
        #define PRODUCT_RTM_VER 9
        #define HARDWARE_RTM_VER 9
    #elif defined(RTM10)
        #define PRODUCT_RTM_VER 10
        #define HARDWARE_RTM_VER 10
    #elif defined(RTM11)
        #define PRODUCT_RTM_VER 11
        #define HARDWARE_RTM_VER 11
    #elif defined(RTM12)
        #define PRODUCT_RTM_VER 12
        #define HARDWARE_RTM_VER 12
    #else
        #error "VAUNT must be compiled with a valid hardware revision (RTM6, RTM7, RTM8, RTM9, RTM10, RTM11, RTM12)"
    #endif
#elif defined(AVERY)
    #if defined(RTM1)
        #define PRODUCT_RTM_VER 1
        #define HARDWARE_RTM_VER 10
    #elif defined(RTM2)
        #define PRODUCT_RTM_VER 2
        // TODO: confirm which RTM Avery will be based on
        // It is intended to be RTM15, but for development purposes I'm setting it to 12 since RTM15 hasn't been implemented yet
        #define HARDWARE_RTM_VER 12
    #else
        #error "AVERY must be compiled with a valid hardware revision (RTM1, RTM2)"
    #endif
#else
    #error "This file must be compiled with a product revision (VAUNT, AVERY)"
#endif

#if HARDWARE_RTM_VER == 6
    // Minimum LO in MHz
    #define MIN_LO 175000000
    #define MIN_LO_S "175000000"

#elif HARDWARE_RTM_VER == 7
    // Minimum LO in MHz
    #define MIN_LO 175000000
    #define MIN_LO_S "175000000"

#elif HARDWARE_RTM_VER == 8
    // Minimum LO in MHz
    // Theoretically can be 115MHz but becomes unreliable below 175MHz and has significant role off
    #define MIN_LO 175000000
    #define MIN_LO_S "175000000"

#elif HARDWARE_RTM_VER == 9
    // Minimum LO in MHz
    // TODO test to determine limits, using this as the minimum since it is likely a safe value
    #define MIN_LO 180000000
    #define MIN_LO_S "180000000"

#elif HARDWARE_RTM_VER == 10
    // Minimum LO in MHz
    // Assumed to have equivalent lo performance to RTM9
    // TODO test to determine limits, using this as the minimum since it is likely a safe value
    #define MIN_LO 180000000
    #define MIN_LO_S "180000000"

#elif HARDWARE_RTM_VER == 11
    // Minimum LO in MHz
    // TODO is MIN_LO even meaningful anymore as we move from ADF5355 to LMX2572 and do not generate lookup table anymore?
    #define MIN_LO 150000000
    #define MIN_LO_S "150000000"

#elif HARDWARE_RTM_VER == 12
    // Minimum LO in MHz
    // TODO: Change MIN_LO to reflect real RTM12 values
    #define MIN_LO 150000000
    #define MIN_LO_S "150000000"

#elif RTM15
    // Minimum LO in MHz
    // TODO: ensure same as RTM12
    #define MIN_LO 150000000
    #define MIN_LO_S "150000000"
    #define PRODUCT_RTM_VER 15

#else
    #error "Invalid HARDWARE_RTM_VER (derived from product and hardware revision). Valid HARDWARE_RTM_VER: 6, 7, 8, 9, 10, 11, 12"
#endif
