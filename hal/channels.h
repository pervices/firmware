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

#if defined(TATE)
    #define CHANNELS \
        X(a, tx) \
        X(b, tx) \
        X(c, tx) \
        X(d, tx) \
        X(e, tx) \
        X(f, tx) \
        X(g, tx) \
        X(h, tx) \
        X(i, tx) \
        X(j, tx) \
        X(k, tx) \
        X(l, tx) \
        X(m, tx) \
        X(n, tx) \
        X(o, tx) \
        X(p, tx)
#elif defined(TATE_8R)
    //Will be only using the populated RF slots
    //Column 3 is for rx, 4 is for tx
    #define CHANNELS \
        X(a) \
        X(b) \
        X(c) \
        X(d) \
        X(e) \
        X(f) \
        X(g) \
        X(h)

#elif defined(TATE_4R4T)
    //Will be only using the populated RF slots
    //Column 3 is for rx, 4 is for tx
    #define CHANNELS \
        X(a) \
        X(b) \
        X(c) \
        X(d)

#elif defined(TATE_9R7T)
    //Will be only using the populated RF slots
    //Column 3 is for rx, 4 is for tx
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

#elif defined(TATE_4R4T_3G)
    //Will be only using the populated RF slots
    //Column 3 is for rx, 4 is for tx
    #define CHANNELS \
        X(a) \
        X(b) \
        X(c) \
        X(d)

#elif defined(VAUNT)
    #define CHANNELS \
        X(a, io) \
        X(b, io) \
        X(c, io) \
        X(d, io)
#elif defined(TATE_NRNT)

    #if defined (S1000)
        #define MAX_SAMPLE_RATE 1000
        #if defined(R4) && defined(T4)
            #define NUM_RX_CHANNELS 4
            #define NUM_TX_CHANNELS 4
            //RFE slots for each channel
            #define INT_RX(ch) ((int)(4*(CHR(ch) - 'a')))
            #define INT_TX(ch) ((int)(4*(CHR(ch) - 'a')) + 2)

        #elif defined(R8) && defined(T0)
            #define NUM_RX_CHANNELS 8
            #define NUM_TX_CHANNELS 0
            //RFE slots for each channel
            #define INT_RX(ch) ((int)((INT(ch)%4)*4)+(1*(INT(ch)/4)))

        #elif defined(R8) && defined(T8)
            #define NUM_RX_CHANNELS 8
            #define NUM_TX_CHANNELS 8
            //RFE slots for each channel
            #define INT_RX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + (int)((CHR(ch) - 'a')/4))
            #define INT_TX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + ((int)(CHR(ch) - 'a')/4) + 2)

        #elif defined(R9) && defined(T7)
            #define NUM_RX_CHANNELS 9
            #define NUM_TX_CHANNELS 7
            //RFE slots for each channel
            #define INT_RX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + (int)((CHR(ch) - 'a')/4))
            #define INT_TX(ch) ((int)(4*(((CHR(ch) + 1) - 'a')%4)) + ((int)((CHR(ch) + 1) - 'a')/4) + 2)
        #else
            #error Invalid number of channels specified for 1G, must be: R4 T4, R8 T0, R9 T7, R8 T8
        #endif

    #elif defined (S3000)
        #define MAX_SAMPLE_RATE 3000
        #if defined(R4) && defined(T4)
            #define NUM_RX_CHANNELS 4
            #define NUM_TX_CHANNELS 4
            //RFE slots for each channel
            #define INT_RX(ch) ((int)(4*(CHR(ch) - 'a')))
            #define INT_TX(ch) ((int)(4*(CHR(ch) - 'a')) + 3)
        #else
            #error Invalid number of rx channels specified for 3G, must be: R4 T4
        #endif
    #else
        #error Invalid maximum sample rate specified (MHz), must be: S1000, S3000
    #endif

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
        #error Invalid configuration, currently supported configurations for NRNT: R9 T7, R4 T4, R8 T0, R8 T8
    #endif

#else
    #error "Project name (TATE | TATE_8R | TATE_4R4T | TATE_9R7T | TATE_4R4T_3G | VAUNT | TATE_NRNT) not specified or not recognized."
#endif

//Below is the old way of defining some channel specific properties, new versions should be integrated into the previous macro
//creates channel maps
#if defined (TATE_4R4T)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"
    //All NUM_CHANNELS should be replaced with tx and rx specific constants, until then NUM_CHANNELS is greater of NUM_TX_CHANNELS and NUM_RX_CHANNELS
    #define NUM_CHANNELS 4
    #define NUM_TX_CHANNELS 4

    #define NUM_RX_CHANNELS 4

    // Converts an expanded char into a runtime integer.
    #define INT_RX(ch) ((int)(4*(CHR(ch) - 'a')))
    #define INT_TX(ch) ((int)(4*(CHR(ch) - 'a')) + 2)

    //hps maps
    static const char* const channel_names[] = {
    #define X(ch) STR(ch),
        CHANNELS
    #undef X
    };
#elif defined (TATE_9R7T)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"

    #define NUM_TX_CHANNELS 7

    #define NUM_RX_CHANNELS 9

    // Converts an expanded char into a runtime integer.
    #define INT_RX(ch) ((int)(4*((CHR(ch) - 'a')%4)) + (int)((CHR(ch) - 'a')/4))
    #define INT_TX(ch) ((int)(4*(((CHR(ch) + 1) - 'a')%4)) + ((int)((CHR(ch) + 1) - 'a')/4) + 2)

#elif defined (TATE_8R)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"
    //All NUM_CHANNELS should be replaced with tx and rx specific constants, until then NUM_CHANNELS is greater of NUM_TX_CHANNELS and NUM_RX_CHANNELS
    #define NUM_CHANNELS 8
    #define NUM_TX_CHANNELS 0

    #define NUM_RX_CHANNELS 8

    //rfe port mapping
    #define INT_RX(ch) ((int)((INT(ch)%4)*4)+(1*(INT(ch)/4)))

    //hps maps
    static const char* const channel_names[] = {
    #define X(ch) STR(ch),
        CHANNELS
    #undef X
    };
//creates channel maps
#elif defined (TATE_4R4T_3G)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"

    #define NUM_TX_CHANNELS 4

    #define NUM_RX_CHANNELS 4

    #define INT_RX(ch) ((int)(4*(CHR(ch) - 'a')))
    #define INT_TX(ch) ((int)(4*(CHR(ch) - 'a')) + 3)

    //hps maps
    static const char* const channel_names[] = {
    #define X(ch) STR(ch),
        CHANNELS
    #undef X
    };
//very old version of tate
#elif defined (TATE)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"

    // Channel names as strings.
    static const char* const channel_names[] = {
    #define X(ch, io) STR(ch),
        CHANNELS
    #undef X
    };
//old method used by tate, and vaunt
#elif defined (VAUNT)
    //directory of the state tree
    #define BASE_DIR "/var/volatile/crimson"
    //state tree
    #define STATE_DIR "/var/volatile/crimson/state"

    // Channel names as strings.
    static const char* const channel_names[] = {
    #define X(ch, io) STR(ch),
        CHANNELS
    #undef X
    };

#endif
/* clang-format on */

#endif
