//
// Copyright 2014 - 2019 Per Vices Corporation
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

#ifndef XMACRO_H_
#define XMACRO_H_

/* clang-format off */

#include "array-utils.h"
#if defined(TATE_NRNT) || defined(LILY)
    #include "variant_config/tate_special_config.h"
#elif defined(VAUNT)
    #include "variant_config/vaunt_special_config.h"
#else
    #error "You must specify either ( VAUNT | TATE_NRNT | LILY ) when compiling this project."
#endif

// https://en.wikipedia.org/wiki/X_Macro.

// Alpha tokens are writen once and expanded N times for however many
// channels specified. Channel operations will be done in the order of this
// specification.

// Converts an expanded channel to a compile time string.
#define STR(ch) #ch

//channel character, this system only works with 26 or fewer channels
#define CHR(ch) #ch[0]

//channel number
#define INT(ch) ((int)(CHR(ch) - 'a'))

#if defined(LILY)
    #if defined (S500)
        // Maximum sample rate the user can request
        #define MAX_USER_SAMPLE_RATE 500
        // Sample rate the ADC/DAC operates at
        #define RF_SAMPLE_RATE 1000
        #define INT_RX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + (int)((CHR(ch) - 'a')/4) + 2)
        #define INT_TX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + ((int)(CHR(ch) - 'a')/4) + 3)

        // Note: 1R1T uses the 4R4T FPGA
        #if defined(R1) && defined(T1)
            #define FPGA_4R4T_1G
            #define NUM_RX_CHANNELS 1
            #define NUM_TX_CHANNELS 1
            #define S_NUM_RX "1"
            #define S_NUM_TX "1"
        #elif defined(R4) && defined(T4)
            #define FPGA_4R4T_1G
            #define NUM_RX_CHANNELS 4
            #define NUM_TX_CHANNELS 4
            #define S_NUM_RX "4"
            #define S_NUM_TX "4"
        #else
            #error Invalid number of channels specified for LILY, must be: R1 T1, R4 T4
        #endif
    #else
        // TODO LILY: correct sample rate
        #error Invalid maximum sample rate specified (MHz), must be: S500
    #endif

#elif defined(TATE_NRNT)

    #if defined (S1000)
        // Maximum sample rate the user can request
        #define MAX_USER_SAMPLE_RATE 1000
        // Sample rate the ADC/DAC operates at
        #define RF_SAMPLE_RATE 1000

        //Default 1G RFE slots for each channel, ideally this would be in tate_fpga_config.h, but its needed for stuff that doesn't include it
        //Some channel combinations may override it
        #define INT_RX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + (int)((CHR(ch) - 'a')/4))
        #if USE_3G_AS_1G
            #define INT_TX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + ((int)(CHR(ch) - 'a')/4) + 3)
        #else
            #define INT_TX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + ((int)(CHR(ch) - 'a')/4) + 2)
        #endif

        // Note: 1R1T uses the 4R4T FPGA
        #if defined(R1) && defined(T1)
            #define FPGA_4R4T_1G
            #define NUM_RX_CHANNELS 1
            #define NUM_TX_CHANNELS 1
            #define S_NUM_RX "1"
            #define S_NUM_TX "1"

        // Note: 2R2T uses the 4R4T FPGA
        #elif defined(R2) && defined(T2)
            #define FPGA_4R4T_1G
            #define NUM_RX_CHANNELS 2
            #define NUM_TX_CHANNELS 2
            #define S_NUM_RX "2"
            #define S_NUM_TX "2"

        // Note: 4R2T uses the 4R4T FPGA
        #elif defined(R4) && defined(T2)
            #define FPGA_4R4T_1G
            #define NUM_RX_CHANNELS 4
            #define NUM_TX_CHANNELS 2
            #define S_NUM_RX "4"
            #define S_NUM_TX "2"

        #elif defined(R4) && defined(T4)
            #define FPGA_4R4T_1G
            #define NUM_RX_CHANNELS 4
            #define NUM_TX_CHANNELS 4
            #define S_NUM_RX "4"
            #define S_NUM_TX "4"

        #elif defined(R8) && defined(T0)
            #define FPGA_8R0T_1G
            #define NUM_RX_CHANNELS 8
            #define NUM_TX_CHANNELS 0
            #define S_NUM_RX "8"
            #define S_NUM_TX "0"
            // Refine rfe slot info for tx since it is unused
            #undef INT_TX

        #elif defined(R8) && defined(T8)
            #define FPGA_8R8T_1G
            #define NUM_RX_CHANNELS 8
            #define NUM_TX_CHANNELS 8
            #define S_NUM_RX "8"
            #define S_NUM_TX "8"
            #if USE_3G_AS_1G
                #error "Invalid channel combination with USE_3G_AS_1G"
            #endif

        #elif defined(R9) && defined(T7)
            #define FPGA_9R7T_1G
            #define NUM_RX_CHANNELS 9
            #define NUM_TX_CHANNELS 7
            #define S_NUM_RX "9"
            #define S_NUM_TX "7"
            //override default rfe slot mapping
            #undef INT_TX
            #define INT_TX(ch) ((int)(4*(((CHR(ch) + 1) - 'a')%4)) + ((int)((CHR(ch) + 1) - 'a')/4) + 2)
            #if USE_3G_AS_1G
                #error "Invalid channel combination with USE_3G_AS_1G"
            #endif
        #else
            #error Invalid number of channels specified for 1G, must be: R4 T4, R8 T0, R9 T7, R8 T8, R4 T2
        #endif

    #elif defined (S3000)
        // Maximum sample rate the user can request
        #define MAX_USER_SAMPLE_RATE 3000
        // Sample rate the ADC/DAC operates at
        #define RF_SAMPLE_RATE 3000

        //Default 3G RFE slots for each channel, ideally this would be in tate_fpga_config.h, but its needed for stuff that doesn't include it
        //Some channel combinations may override it
        #define INT_RX(ch) ((int)(4*(CHR(ch) - 'a')))
        #define INT_TX(ch) ((int)(4*(CHR(ch) - 'a')) + 3)

        #if USE_3G_AS_1G
            #error "Using USE_3G_AS_1G flag and 3G sample rate"
        #endif
        #if defined(R4) && defined(T4)
            #define FPGA_4R4T_3G
            #define NUM_RX_CHANNELS 4
            #define NUM_TX_CHANNELS 4
            #define S_NUM_RX "4"
            #define S_NUM_TX "4"
        #else
            #error Invalid number of rx channels specified for 3G, must be: R4 T4
        #endif
    #else
        #error Invalid maximum sample rate specified (MHz), must be: S1000, S3000
    #endif
#elif defined(VAUNT)
    #define NUM_RX_CHANNELS 4
    #define S_NUM_RX "4"

    // In RX_40GHZ_FE mode the tx boards are replaced with the 40GHz equipment
    #if RX_40GHZ_FE
        #define NUM_TX_CHANNELS 0
        #define S_NUM_TX "0"
    #else
        #define NUM_TX_CHANNELS 4
        #define S_NUM_TX "4"
    #endif
#endif

#if defined(VAUNT)
    #define RX_CHANNELS \
        X(a) \
        X(b) \
        X(c) \
        X(d)

    // In RX_40GHZ_FE mode the tx boards are replaced with the 40GHz equipment
    #if RX_40GHZ_FE
        #define TX_CHANNELS
    #else
        #define TX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d)
    #endif

#elif defined(TATE_NRNT) || defined(LILY)
    #if (NUM_RX_CHANNELS == 4 && NUM_TX_CHANNELS ==4)
        //TODO generate this dynamically, used by the macro to create the functions for each channel
        #define RX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d)

        #define TX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d)

    #elif (NUM_RX_CHANNELS == 1 && NUM_TX_CHANNELS == 1)
        //TODO generate this dynamically, used by the macro to create the functions for each channel
        #define RX_CHANNELS \
            X(a)

        #define TX_CHANNELS \
            X(a)

    #elif (NUM_RX_CHANNELS == 2 && NUM_TX_CHANNELS == 2)
        //TODO generate this dynamically, used by the macro to create the functions for each channel
        #define RX_CHANNELS \
            X(a) \
            X(b)

        #define TX_CHANNELS \
            X(a) \
            X(b)

    #elif (NUM_RX_CHANNELS == 4 && NUM_TX_CHANNELS == 2)
        //TODO generate this dynamically, used by the macro to create the functions for each channel
        #define RX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d)

        #define TX_CHANNELS \
            X(a) \
            X(b)

    #elif (NUM_RX_CHANNELS == 8 && NUM_TX_CHANNELS == 0)
        //TODO generate this dynamically, used by the macro to create the functions for each channel
        #define RX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d) \
            X(e) \
            X(f) \
            X(g) \
            X(h)

        #define TX_CHANNELS

    // Configuration for 9R7T
    #elif (NUM_RX_CHANNELS == 9 && NUM_TX_CHANNELS ==7)
        //TODO generate this dynamically, used by the macro to create the functions for each channel
        #define RX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d) \
            X(e) \
            X(f) \
            X(g) \
            X(h) \
            X(i)

        #define TX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d) \
            X(e) \
            X(f) \
            X(g)
            
    // Configuration for 8R8T
    #elif (NUM_RX_CHANNELS == 8 && NUM_TX_CHANNELS ==8)
        //TODO generate this dynamically, used by the macro to create the functions for each channel
        #define RX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d) \
            X(e) \
            X(f) \
            X(g) \
            X(h)

        #define TX_CHANNELS \
            X(a) \
            X(b) \
            X(c) \
            X(d) \
            X(e) \
            X(f) \
            X(g) \
            X(h)

    #else
        #error Invalid configuration, currently supported configurations: R9 T7, R4 T4, R8 T0, R8 T8, R1 T1, R2 T2
    #endif

#else
    #error "You must specify either ( VAUNT | TATE_NRNT | LILY ) when compiling this project."
#endif

/* clang-format on */

#endif
