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

#if defined(TATE)
    #define GPIO_PINS               \
        Q(000, res_rw4, 0x1)        \
        Q(001, res_rw4, 0x2)        \
        Q(002, res_rw4, 0x4)        \
#else
    #error "Project name (TATE) not specified or not recognized."
#endif

/* Code leftover from CHANNELS adaptation
       
         
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
