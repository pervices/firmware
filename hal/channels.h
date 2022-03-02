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
    #define CHANNELS   \
        X(a, rx, a, c) \
        X(b, rx, e, g) \
        X(c, rx, i, k) \
        X(d, rx, m, o) \
        X(e, rx, b, d) \
        X(f, rx, f, h) \
        X(g, rx, j, l) \
        X(h, rx, n, p)

#elif defined(TATE_4R4T)
    //Will be only using the populated RF slots
    //Column 3 is for rx, 4 is for tx
    #define CHANNELS \
        X(a, io, a, c) \
        X(b, io, e, g) \
        X(c, io, i, k) \
        X(d, io, m, o)

#elif defined(TATE_9R7T)
    //Will be only using the populated RF slots
    //Column 3 is for rx, 4 is for tx
    #define RX_CHANNELS \
        X(a, io, a, g) \
        X(b, io, e, k) \
        X(c, io, i, o) \
        X(d, io, m, d) \
        X(e, io, b, h) \
        X(f, io, f, l) \
        X(g, io, j, o) \
        X(h, rx, n, p) \
        X(i, rx, c, q)
        
    #define TX_CHANNELS \
        X(a, io, a, g) \
        X(b, io, e, k) \
        X(c, io, i, o) \
        X(d, io, m, d) \
        X(e, io, b, h) \
        X(f, io, f, l) \
        X(g, io, j, o)

#elif defined(TATE_4R4T_3G)
    //Will be only using the populated RF slots
    //Column 3 is for rx, 4 is for tx
    #define CHANNELS \
        X(a, io, a, c) \
        X(b, io, e, g) \
        X(c, io, i, k) \
        X(d, io, m, o)

#elif defined(VAUNT)
    #define CHANNELS \
        X(a, io) \
        X(b, io) \
        X(c, io) \
        X(d, io)
#else
    #error "Project name (TATE | TATE_8R | TATE_4R4T | TATE_9R7T | TATE_4R4T_3G | VAUNT) not specified or not recognized."
#endif

// Converts an expanded channel to a compile time string.
#define STR(ch) #ch

//channel character, this system only works with 26 or fewer channels
#define CHR(ch) #ch[0]

//channel number
#define INT(ch) ((int)(CHR(ch) - 'a'))

//creates channel maps
#if defined (TATE_4R4T)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"
    //All NUM_CHANNELS should be replaced with tx and rx specific constants, until then NUM_CHANNELS is greater of NUM_TX_CHANNELS and NUM_RX_CHANNELS
    #define NUM_CHANNELS 4
    #define NUM_TX_CHANNELS 4
    static uint8_t tx_power[NUM_TX_CHANNELS] = {0};

    #define NUM_RX_CHANNELS 4
    static uint8_t rx_power[NUM_RX_CHANNELS] = {0};

    #define STR_RX(crx) #crx
    #define STR_TX(ctx) #ctx

    // Converts an expanded char into a runtime integer.
    #define INT(ch) ((int)(CHR(ch) - 'a'))
    #define INT_RX(ch) ((int)(4*(CHR(ch) - 'a')))
    #define INT_TX(ch) ((int)(4*(CHR(ch) - 'a')) + 2)

    //hps maps
    static const char* const channel_names[] = {
    #define X(ch, io, crx, ctx) STR(ch),
        CHANNELS
    #undef X
    };
#elif defined (TATE_9R7T)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"
    //All NUM_CHANNELS should be replaced with tx and rx specific constants, until then NUM_CHANNELS is greater of NUM_TX_CHANNELS and NUM_RX_CHANNELS
    #define NUM_CHANNELS 9
    #define NUM_TX_CHANNELS 7
    static uint8_t tx_power[NUM_TX_CHANNELS] = {0};

    #define NUM_RX_CHANNELS 9
    static uint8_t rx_power[NUM_RX_CHANNELS] = {0};

    #define STR_RX(crx) #crx
    #define STR_TX(ctx) #ctx

    // Converts an expanded char into a runtime integer.
    #define INT(ch) ((int)(CHR(ch) - 'a'))
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
    static uint8_t tx_power[NUM_TX_CHANNELS] = {0};

    #define NUM_RX_CHANNELS 8
    static uint8_t rx_power[NUM_RX_CHANNELS] = {0};

    #define STR_RX(crx) #crx
    #define STR_TX(ctx) #ctx
    #define CHR_RX(crx) #crx[0]

    //rfe port mapping
    #define INT_RX(ch) ((int)((INT(ch)%4)*4)+(1*(INT(ch)/4)))
    #define INT_TX(ch) ((int)(4*(CHR_RX(ch) - 'a')) + 2)

    //hps maps
    static const char* const channel_names[] = {
    #define X(ch, io, crx, ctx) STR(ch),
        CHANNELS
    #undef X
    };
//creates channel maps
#elif defined (TATE_4R4T_3G)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"
    //All NUM_CHANNELS should be replaced with tx and rx specific constants, until then NUM_CHANNELS is greater of NUM_TX_CHANNELS and NUM_RX_CHANNELS
    #define NUM_CHANNELS 4
    #define NUM_TX_CHANNELS 4
    static uint8_t tx_power[NUM_TX_CHANNELS] = {0};

    #define NUM_RX_CHANNELS 4
    static uint8_t rx_power[NUM_RX_CHANNELS] = {0};

    #define STR_RX(crx) #crx
    #define STR_TX(ctx) #ctx

    // Converts an expanded char into a runtime integer.
    #define INT(ch) ((int)(CHR(ch) - 'a'))
    #define INT_RX(ch) ((int)(4*(CHR(ch) - 'a')))
    #define INT_TX(ch) ((int)(4*(CHR(ch) - 'a')) + 3)

    //hps maps
    static const char* const channel_names[] = {
    #define X(ch, io, crx, ctx) STR(ch),
        CHANNELS
    #undef X
    };
//very old version of tate
#elif defined (TATE)
    //directory of the state tree
    #define BASE_DIR "/var/cyan"
    //state tree
    #define STATE_DIR "/var/cyan/state"
    // Converts an expanded char into a runtime integer.
    #define INT(ch) ((int)(CHR(ch) - 'a'))

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
    // Converts an expanded char into a runtime integer.
    #define INT(ch) ((int)(CHR(ch) - 'a'))

    // Channel names as strings.
    static const char* const channel_names[] = {
    #define X(ch, io) STR(ch),
        CHANNELS
    #undef X
    };

#endif
/* clang-format on */

#endif
