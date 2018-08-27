#ifndef _XMACRO_H_
#define _XMACRO_H_

/* clang-format off */

#include "array-utils.h"

// https://en.wikipedia.org/wiki/X_Macro.

// Alpha tokens are writen once and expanded N times for however many
// channels specified. Channel operations will be done in the order of this
// specification.

#if defined(VAUNT)
    #define CHANNELS  \
        X(a) /*  0 */ \
        X(b) /*  1 */ \
        X(c) /*  2 */ \
        X(e) /*  3 */
#elif defined(TATE)
    #define CHANNELS  \
        X(a) /*  0 */ \
        X(b) /*  1 */ \
        X(c) /*  2 */ \
        X(d) /*  3 */ \
        X(e) /*  4 */ \
        X(f) /*  5 */ \
        X(g) /*  6 */ \
        X(h) /*  7 */ \
        X(i) /*  8 */ \
        X(j) /*  9 */ \
        X(k) /* 10 */ \
        X(l) /* 11 */ \
        X(m) /* 12 */ \
        X(n) /* 13 */ \
        X(o) /* 14 */ \
        X(p) /* 15 */
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
#define X(ch) STR(ch)
    CHANNELS
#undef X
};

// Number of channels.
#define NUM_CHANNELS ARRAY_SIZE(channel_names)

/* clang-format on */

#endif
