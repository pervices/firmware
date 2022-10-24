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

#ifndef COMMON_H_
#define COMMON_H_

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

// Define used for debugging
//#define DEBUG

// Global return values
#define RETURN_SUCCESS 0
#define RETURN_ERROR -1
#define RETURN_ERROR_PARAM -2
#define RETURN_ERROR_COMM_INIT -3
#define RETURN_ERROR_COMM_NO_DATA -4
#define RETURN_ERROR_COMM_BUS -5
#define RETURN_ERROR_COMM_MMAP -6
#define RETURN_ERROR_INOTIFY -7
#define RETURN_ERROR_GET_PROP -8
#define RETURN_ERROR_SET_PROP -9
#define RETURN_ERROR_UART_TIMEOUT -10
#define RETURN_ERROR_CMD_CURRUPT -11
#define RETURN_ERROR_INVALID_REGISTER -12
#define RETURN_ERROR_ADDR_OUT_OF_RANGE -13
#define RETURN_ERROR_INSUFFICIENT_RESOURCES -99
#define RETURN_ERROR_UART_FLUSH -14
#define RETURN_ERROR_UART_IO -16
#define RETURN_ERROR_UART_INVALID_FD -3450
#define RETURN_ERROR_INSUFFICIENT_BUFFER -15

#define EXIT_ERROR_RFE_CONTROL 43

// server options
#define SERVER_DEBUG_OPT (1 << 0)
#define SERVER_RESVD_OPT (1 << 1)

// General purpose temp buffer size
#define BUF_SIZE (16384)
#define MAX_PATH_LEN (128)
#define MAX_PROP_LEN (16384)
// Having the buffer containing default properties be large results in the server file size being very large
// Since the only time large buffers are needed is for dumps, the max size of default values is set to be very low
#define MAX_DEFAULT_PROP_LEN (128)
#define MAX_UART_SEND_LEN (16384)
#define MAX_UART_RET_LEN (16384)
//length for property path buffer
#define PROP_PATH_LEN MAX_PATH_LEN

//Bits to indicate the power state. Bitwise or when multiple states are active at once. For example if there is board installed but you want the server to act like there is a board and its on, the correct value is PWR_NO_BOARD | PWR_ON
//Of the following PWR, only PWR_OFF and PWR_ON are valid inputs to pwr, the rest are used internally to check the status of things
//indicates that no board is present
#define PWR_NO_BOARD 2
//indicates that either the board if off, if no board is present it will default to this value until the fact that the baord is missing is detected
#define PWR_OFF 0
//indicates that the board is on, but the rest of the startup sequence has not been doner
#define PWR_HALF_ON 4
#define PWR_ON 1

// Output data structure
typedef struct outdata {
    uint8_t status; // status of the operation
    uint32_t seq;   // sequence of cmd, must match	NOT YET IMPLEMENTED
    char *data;     // data the be sent back
} outdata_t;

// Input data structure
typedef struct indata {
    uint32_t seq;  // sequence of cmd, must match NOT YET IMPLEMENTED
    uint8_t ctgry; // category
    uint8_t op;    // operation
    char *cmd;     // command
    char *data;    // write data if operation is a SET
} indata_t;

// UDP ports
typedef enum {
    UDP_MGMT_PORT = 42799,
    UDP_RXA_PORT = 42800,
    UDP_RXB_PORT = 42801,
    UDP_RXC_PORT = 42802,
    UDP_RXD_PORT = 42803,
    UDP_TXA_PORT = 42804,
    UDP_TXB_PORT = 42805,
    UDP_TXC_PORT = 42806,
    UDP_TXD_PORT = 42807,
    num_udp_ports = 9
} udp_port_t;

enum {
    UDP_FLOW_CNTRL_PORT = 42808, // this one has to be the last one
};

// Boolean datatype
typedef enum { FALSE, TRUE } boolean;

typedef enum { ERROR, INFO, DEBUG, VERBOSE, DUMP } print_t;

// printf wrapper
void PRINT_WRAPPER(print_t priority, const char *format, ...);

#define PRINT(prio, fmt, args...)                                              \
    do {                                                                       \
        struct timespec _ts;                                                   \
        clock_gettime(CLOCK_REALTIME, &_ts);                                   \
        if (ERROR == prio) {                                                   \
            PRINT_WRAPPER(prio, "[%6ld.%03ld] %s: %s(): " fmt,                 \
                          (long)_ts.tv_sec, _ts.tv_nsec / 1000000UL, #prio,    \
                          __func__, ##args);                                   \
        } else {                                                               \
            PRINT_WRAPPER(prio, "[%6ld.%03ld] %s: " fmt, (long)_ts.tv_sec,     \
                          _ts.tv_nsec / 1000000UL, #prio, ##args);             \
        }                                                                      \
    } while (0)

#endif /* COMMON_H_ */
