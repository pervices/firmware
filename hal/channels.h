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

#if defined(VAUNT)
    #define CHANNELS \
        X(a, io) \
        X(b, io) \
        X(c, io) \
        X(d, io)
#elif defined(TATE)
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
#elif defined(TATE_TEST)
    #define CHANNELS \
        X(a, tx) \
        X(b, tx) \
        X(c, tx) \
        X(d, tx)
    #undef TATE_TEST
    #define TATE
#else
    #error "Project name (VAUNT | TATE) not specified or not recognized."
#endif

// Converts an expanded channel to a compile time string.
#define STR(ch) #ch

// Converts an expanded string into a runtime char.
#define CHR(ch) #ch[0]

// Converts an expanded char into a runtime integer.
#define INT(ch) ((int)(CHR(ch) - 'a'))

// Channel names as strings.
static const char* const channel_names[] = {
#define X(ch, io) STR(ch),
    CHANNELS
#undef X
};

// Number of channels.
#define NUM_CHANNELS ARRAY_SIZE(channel_names)

/* clang-format on */

#endif
