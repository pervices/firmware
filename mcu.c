#include <stdio.h>
//
// Copyright 2014 Per Vices Corporation
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

#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>

#include "common.h"
#include "comm_manager.h"

// mmap must be initialed for printing the version
#include "mmap.h"
#include "hal/utils/print_version.h"

#define ARG_MCU_SILENT "-s"
#define ARG_MCU_CONSOLE "-c"
#define ARG_MCU_FWD "-f"
#define ARG_MCU_TIMEOUT "-t"
#define ARG_MCU_UART_TX "-TX"
#define ARG_MCU_UART_SN "-SN"
#define ARG_MCU_UART_RX "-RX"

#define DEFAULT_TIMEOUT 100 // Milliseconds

//#define DEBUG_OUTPUTS

static char buf[MAX_UART_SEND_LEN] = {};
static boolean silent = FALSE;
static boolean console = FALSE;
static uint32_t timeout = DEFAULT_TIMEOUT;

// Crimson files
#ifdef VAUNT
static const char *UART_CRIMSON_SN = "/dev/ttycrimson-time";
static const char *UART_CRIMSON_TX = "/dev/ttycrimson-tx";
static const char *UART_CRIMSON_RX = "/dev/ttycrimson-rx";

enum TARGET_DEV {
    DEV_TIME,
    DEV_TX,
    DEV_RX
};

#elif defined(TATE_NRNT)
// Cyan files
static const char *UART_CYAN_SN = "/dev/ttycyan-time";
static const char *UART_CYAN_RFE[16]  = {"/dev/ttycyan-rfe-0",
                                    "/dev/ttycyan-rfe-1",
                                    "/dev/ttycyan-rfe-2",
                                    "/dev/ttycyan-rfe-3",
                                    "/dev/ttycyan-rfe-4",
                                    "/dev/ttycyan-rfe-5",
                                    "/dev/ttycyan-rfe-6",
                                    "/dev/ttycyan-rfe-7",
                                    "/dev/ttycyan-rfe-8",
                                    "/dev/ttycyan-rfe-9",
                                    "/dev/ttycyan-rfe-10",
                                    "/dev/ttycyan-rfe-11",
                                    "/dev/ttycyan-rfe-12",
                                    "/dev/ttycyan-rfe-13",
                                    "/dev/ttycyan-rfe-14",
                                    "/dev/ttycyan-rfe-15"};

enum TARGET_DEV {
    DEV_TIME,
    DEV_RFE
};

static int target_rfe = -1;

#else
    #error "This file must be compiled with a valid PRODUCT (VAUNT | TATE_NRNT)"
#endif

static enum TARGET_DEV target_dev;

static int uart_target_fd;

static int contains(const char *str, char letter, int size) {
    int cnt = 0;
    for (int i = 0; i < size; i++) {
        if (str[i] == letter)
            cnt++;
    }
    return cnt;
}

#ifdef DEBUG_OUTPUTS
    static void dump_args(void) {
        int i = 0;
    #if defined(TATE_NRNT)
        printf("%s\n", UART_CYAN_SN);
        for (i = 0; i < 16; i++) {
            printf("%s\n", UART_CYAN_RFE[i]);
        }
    #elif defined(VAUNT)
        printf("%s\n", UART_CRIMSON_SN);
        printf("%s\n", UART_CRIMSON_TX);
        printf("%s\n", UART_CRIMSON_RX);
        printf("%d\n", uart_crimson_synth_fd);
        printf("%d\n", uart_crimson_tx_fd);
        printf("%d\n", uart_crimson_rx_fd);
    #else
        #error "This file must be compiled with a valid PRODUCT (TATE_NRNT, VAUNT). Confirm spelling and spaces."
    #endif
        printf("%d\n", uart_comm_fd);
    }
#endif

static void help(void) {
    printf("Usage: mcu"
           "[%s /dev/tty] "
           "[%s /dev/tty] "
           "[%s /dev/tty] "
           "[%s] "
           "[%s] "
           "[%s [t|r|s]] "
           "[%s milliseconds]\n",
           ARG_MCU_UART_TX, ARG_MCU_UART_RX, ARG_MCU_UART_SN, ARG_MCU_SILENT,
           ARG_MCU_CONSOLE, ARG_MCU_FWD, ARG_MCU_TIMEOUT);
    exit(1);
}

static boolean streql(char *a, char *b) { return strcmp(a, b) == 0; }

static boolean last(const int arg, const int argc) { return arg == argc - 1; }

//TODO: clean up this function. When merging the branches I made it ignore stuff using Crimson paths in Cyan and vice versa
//However at a glance it looks like this means some commands are only implemented in one of them
static void parse_args(int argc, char *argv[]) {
    // parse through the arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            // This program only requires the mmap to be initialed when getting version info
            int r = mmap_init();
            if (EXIT_SUCCESS != r) {
                PRINT(ERROR, "mmap_init failed, some version info will be inaccurate\n");
            }
            print_version();
            exit(0);
            return;
        }
        // if argument to silent the output
        else if (streql(argv[i], ARG_MCU_SILENT)) {
            silent = TRUE;

            // if argument to open a console transaction
        } else if (streql(argv[i], ARG_MCU_CONSOLE)) {
            console = TRUE;
#if defined(VAUNT)
            // if argument to specify this is a forward command
        } else if (streql(argv[i], ARG_MCU_UART_SN) && !last(i, argc)) {

            UART_CRIMSON_SN = argv[i + 1];
            i++;
        } else if (streql(argv[i], ARG_MCU_UART_TX) && !last(i, argc)) {

            UART_CRIMSON_TX = argv[i + 1];
            i++;
        } else if (streql(argv[i], ARG_MCU_UART_RX) && !last(i, argc)) {

            UART_CRIMSON_RX = argv[i + 1];
            i++;
#endif
        } else if (streql(argv[i], ARG_MCU_FWD) && !last(i, argc)) {
            i++;
            if (argv[i][0] == 's') {
                target_dev = DEV_TIME;
#if defined(VAUNT)
            } else if (argv[i][0] == 't') {
                target_dev = DEV_TX;
            } else if (argv[i][0] == 'r') {
                target_dev = DEV_RX;
#elif defined(TATE_NRNT)
            } else if (atoi(argv[i]) < 16 && atoi(argv[i]) >= 0) {
                target_dev = DEV_RFE;
                target_rfe = atoi(argv[i]);
#else
    #error "This file must be compiled with a valid PRODUCT (VAUNT | TATE_NRNT)"
#endif
            } else {
                help();
            }

            // if argument to reconfigure the timeout
        } else if (streql(argv[i], ARG_MCU_TIMEOUT) && !last(i, argc)) {
            i++;
            sscanf(argv[i], "%" SCNu32 "", &timeout);

        } else {
            // usage menu
            help();
        }
    }
}

int main(int argc, char *argv[]) {

    parse_args(argc, argv);
    #ifdef DEBUG_OUTPUTS
        dump_args();
    #endif

#if defined(TATE_NRNT)
    if(target_dev == DEV_TIME) {
        if (init_uart_comm(&uart_target_fd, UART_CYAN_SN, 0) < 0) {
            PRINT(ERROR, "Cannot initialize uart %s. The time board will not work\n", UART_CYAN_SN);
            return RETURN_ERROR_INSUFFICIENT_RESOURCES;
        }
    } else if(target_dev == DEV_RFE) {
        if (init_uart_comm(&uart_target_fd, UART_CYAN_RFE[target_rfe], 0) < 0) {
            PRINT(ERROR, "Failed to initialize UART %s, any board in this slot will not work\n", UART_CYAN_RFE[target_rfe]);
            return RETURN_ERROR_INSUFFICIENT_RESOURCES;
        }
    } else {
        PRINT(ERROR, "Invalid target device\n");
        return RETURN_ERROR_PARAM;
    }

#elif defined(VAUNT)
    // initialize the comm port
    if(target_dev == DEV_TIME) {
        if (init_uart_comm(&uart_target_fd, UART_CRIMSON_SN, 0) < 0) {
            PRINT(ERROR, "cannot initialize uart %s\n", UART_CRIMSON_SN);
            return RETURN_ERROR_INSUFFICIENT_RESOURCES;
        }
    }
    else if(target_dev == DEV_TX) {
        if (init_uart_comm(&uart_target_fd, UART_CRIMSON_TX, 0) < 0) {
            PRINT(ERROR, "cannot initialize uart %s\n", UART_CRIMSON_TX);
            return RETURN_ERROR_INSUFFICIENT_RESOURCES;
        }
    }
    else if(target_dev == DEV_RX) {
        if (init_uart_comm(&uart_target_fd, UART_CRIMSON_RX, 0) < 0) {
            PRINT(ERROR, "cannot initialize uart %s\n", UART_CRIMSON_RX);
            return RETURN_ERROR_INSUFFICIENT_RESOURCES;
        }
    } else {
        PRINT(ERROR, "Invalid target device\n");
        return RETURN_ERROR_PARAM;
    }
#else
    #error "This file must be compiled with a valid PRODUCT (TATE_NRNT VAUNT). Confirm spelling and spaces."
#endif


    // initiate UART transaction
    do {
        fgets(buf, MAX_UART_SEND_LEN, stdin);
        int read_in_n = strnlen(buf, MAX_UART_SEND_LEN);
        snprintf(buf + read_in_n, MAX_UART_SEND_LEN - read_in_n, "'\r");

        send_uart_comm(uart_target_fd, (uint8_t *)buf, strlen(buf));

        // if not silent, read the output
        if (!silent) {
            memset(buf, 0, MAX_UART_SEND_LEN);
            int32_t total_bytes = 0;
            uint32_t cur_bytes = 0;

            while (contains(buf, '>', total_bytes) < 1) {
                if (recv_uart_comm(uart_target_fd, ((uint8_t *)buf) + total_bytes,
                                   &cur_bytes, MAX_UART_SEND_LEN - total_bytes)) {
                    return 0;
                }
                total_bytes += cur_bytes;
            }

            printf("%s\n", buf);
        }
    } while (console);
    return 0;
}
