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

#ifndef COMMON_IED_H_
#define COMMON_IED_H_

#include <errno.h>
#include <string.h>

#ifdef HAVE_SYSLOG

// openlog(3) must be called prior to using any of the macros below
// use setlogmask(3) to set enable debug messages

#include <syslog.h>

#define I(fmt, args...)                                                        \
    do {                                                                       \
        syslog(LOG_INFO, fmt "\n", ##args);                                    \
    } while (0)

#define E(fmt, args...)                                                        \
    if (0 == errno) {                                                          \
        syslog(LOG_CRIT, "%s: %s(): %d: " fmt "\n", __FILE__, __FUNCTION__,    \
            __LINE__, ##args);                                                 \
    } else {                                                                   \
        syslog(LOG_CRIT, "%s: %s(): %d: %s (%d): " fmt "\n", __FILE__,         \
            __FUNCTION__, __LINE__, strerror(errno), errno, ##args);           \
    }

#define D(fmt, args...)                                                        \
    do {                                                                       \
        syslog(                                                                \
            LOG_DEBUG, "%s: %s(): " fmt "\n", __FILE__, __FUNCTION__, ##args); \
    } while (0)

#else /* HAVE_SYSLOG */

// poor-man's logging via stdio
// #define DEBUG before including this header to enable debug messages

#include <stdio.h>

#define I(fmt, args...)                                                        \
    do {                                                                       \
        fprintf(stdout, "I: " fmt "\n", ##args);                               \
    } while (0)

#define E(fmt, args...)                                                        \
    if (0 == errno) {                                                          \
        fprintf(stderr, "E: %s: %s(): %d: " fmt "\n", __FILE__, __FUNCTION__,  \
            __LINE__, ##args);                                                 \
    } else {                                                                   \
        fprintf(stderr, "E: %s: %s(): %d: %s (%d): " fmt "\n", __FILE__,       \
            __FUNCTION__, __LINE__, strerror(errno), errno, ##args);           \
    }

#ifdef DEBUG
#define D(fmt, args...)                                                        \
    do {                                                                       \
        fprintf(                                                               \
            stderr, "D: %s: %s(): " fmt "\n", __FILE__, __FUNCTION__, ##args); \
    } while (0)
#else
#define D(fmt, args...)
#endif /* DEBUG */

#endif /* HAVE_SYSLOG */

#endif /* COMMON_IED_H_ */
